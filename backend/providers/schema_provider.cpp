#include "schema_provider.h"

#include "../database/connection_utils.h"
#include "../database/schema_inspector.h"
#include "../database/sqlserver_driver.h"
#include "../interfaces/providers/connection_provider.h"
#include "../utils/json_utils.h"
#include "../utils/logger.h"
#include "../utils/sql_validation.h"
#include "simdjson.h"

#include <charconv>
#include <format>
#include <ranges>

namespace velocitydb {

namespace {

[[nodiscard]] std::string splitCsvToJsonArray(std::string_view csv) {
    if (csv.empty())
        return "[]";
    std::string result = "[";
    bool first = true;
    for (auto part : csv | std::views::split(',')) {
        if (!first)
            result += ',';
        result += std::format("\"{}\"", JsonUtils::escapeString({part.begin(), part.end()}));
        first = false;
    }
    result += "]";
    return result;
}

struct TableQueryParams {
    std::string tableName;
    std::shared_ptr<SQLServerDriver> driver;
};

[[nodiscard]] std::expected<TableQueryParams, std::string> extractTableQueryParams(const simdjson::dom::element& doc, IConnectionProvider& connections) {
    auto connectionIdResult = doc["connectionId"].get_string();
    auto tableNameResult = doc["table"].get_string();
    if (connectionIdResult.error() || tableNameResult.error()) [[unlikely]]
        return std::unexpected("Missing required fields: connectionId or table");

    auto tableName = std::string(tableNameResult.value());
    if (!isValidIdentifier(tableName)) [[unlikely]]
        return std::unexpected("Invalid table name");

    auto connectionId = std::string(connectionIdResult.value());
    auto driver = connections.getMetadataDriver(connectionId);
    if (!driver) [[unlikely]]
        return std::unexpected(std::format("Connection not found: {}", connectionId));

    return TableQueryParams{std::move(tableName), std::move(driver)};
}

}  // namespace

SchemaProvider::SchemaProvider(IConnectionProvider& connections) : m_connections(connections), m_schemaInspector(std::make_unique<SchemaInspector>()) {}

SchemaProvider::~SchemaProvider() = default;

std::string SchemaProvider::handleGetDatabases(std::string_view params) {
    auto connectionIdResult = extractConnectionId(params);
    if (!connectionIdResult) {
        return JsonUtils::errorResponse(connectionIdResult.error());
    }
    try {
        auto driver = m_connections.getMetadataDriver(*connectionIdResult);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", *connectionIdResult));
        }
        auto queryResult = driver->execute("SELECT name FROM sys.databases ORDER BY name");
        auto jsonResponse = JsonUtils::buildRowArray(queryResult.rows, 1, [](std::string& out, const ResultRow& row) { out += std::format(R"("{}")", JsonUtils::escapeString(row.values[0])); });
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetTables(std::string_view params) {
    log<LogLevel::DEBUG>(std::format("SchemaProvider::handleGetTables called with params: {}", params));
    auto connectionIdResult = extractConnectionId(params);
    if (!connectionIdResult) {
        return JsonUtils::errorResponse(connectionIdResult.error());
    }
    auto connectionId = *connectionIdResult;
    try {
        auto driver = m_connections.getMetadataDriver(connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }
        constexpr auto tableListQuery = R"(
            SELECT
                t.TABLE_SCHEMA,
                t.TABLE_NAME,
                t.TABLE_TYPE,
                CAST(ep.value AS NVARCHAR(MAX)) AS comment
            FROM INFORMATION_SCHEMA.TABLES t
            LEFT JOIN sys.extended_properties ep ON ep.major_id = OBJECT_ID(t.TABLE_SCHEMA + '.' + t.TABLE_NAME)
                AND ep.minor_id = 0
                AND ep.class = 1
                AND ep.name = 'MS_Description'
            WHERE t.TABLE_TYPE IN ('BASE TABLE', 'VIEW')
            ORDER BY t.TABLE_SCHEMA, t.TABLE_NAME
        )";
        auto queryResult = driver->execute(tableListQuery);
        auto jsonResponse = JsonUtils::buildRowArray(queryResult.rows, 3, [](std::string& out, const ResultRow& row) {
            auto comment = row.values.size() >= 4 ? row.values[3] : std::string{};
            out += std::format(R"({{"schema":"{}","name":"{}","type":"{}","comment":"{}"}})", JsonUtils::escapeString(row.values[0]), JsonUtils::escapeString(row.values[1]),
                               JsonUtils::escapeString(row.values[2]), JsonUtils::escapeString(comment));
        });
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetColumns(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;
        auto [schema, tbl] = splitSchemaTable(tableName);

        auto columnQuery = std::format(R"(
            SELECT
                c.name AS column_name,
                t.name AS data_type,
                c.max_length,
                c.is_nullable,
                CASE WHEN pk.column_id IS NOT NULL THEN 1 ELSE 0 END AS is_primary_key,
                CAST(ep.value AS NVARCHAR(MAX)) AS comment
            FROM sys.columns c
            INNER JOIN sys.types t ON c.user_type_id = t.user_type_id
            INNER JOIN sys.objects o ON c.object_id = o.object_id
            INNER JOIN sys.schemas s ON o.schema_id = s.schema_id
            LEFT JOIN (
                SELECT ic.object_id, ic.column_id
                FROM sys.index_columns ic
                INNER JOIN sys.indexes i ON ic.object_id = i.object_id AND ic.index_id = i.index_id
                WHERE i.is_primary_key = 1
            ) pk ON c.object_id = pk.object_id AND c.column_id = pk.column_id
            LEFT JOIN sys.extended_properties ep ON ep.major_id = c.object_id
                AND ep.minor_id = c.column_id
                AND ep.class = 1
                AND ep.name = 'MS_Description'
            WHERE o.name = '{}' AND s.name = '{}'
            ORDER BY c.column_id
        )",
                                       escapeSqlString(tbl), escapeSqlString(schema));

        auto columnResult = driver->execute(columnQuery);

        auto jsonResponse = JsonUtils::buildRowArray(columnResult.rows, 5, [](std::string& out, const ResultRow& row) {
            std::string_view sizeStr = row.values[2];
            int colSize = 0;
            std::from_chars(sizeStr.data(), sizeStr.data() + sizeStr.size(), colSize);
            auto comment = row.values.size() >= 6 ? row.values[5] : std::string{};
            auto nullable = row.values[3] == "1" ? "true" : "false";
            auto isPk = row.values[4] == "1" ? "true" : "false";
            out += std::format(R"({{"name":"{}","type":"{}","size":{},"nullable":{},"isPrimaryKey":{},"comment":"{}"}})", JsonUtils::escapeString(row.values[0]),
                               JsonUtils::escapeString(row.values[1]), colSize, nullable, isPk, JsonUtils::escapeString(comment));
        });
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetIndexes(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

        auto indexQuery = std::format(R"(
            SELECT
                i.name AS IndexName,
                i.type_desc AS IndexType,
                i.is_unique AS IsUnique,
                i.is_primary_key AS IsPrimaryKey,
                STUFF((
                    SELECT ',' + c.name
                    FROM sys.index_columns ic
                    JOIN sys.columns c ON ic.object_id = c.object_id AND ic.column_id = c.column_id
                    WHERE ic.object_id = i.object_id AND ic.index_id = i.index_id
                    ORDER BY ic.key_ordinal
                    FOR XML PATH('')
                ), 1, 1, '') AS Columns
            FROM sys.indexes i
            WHERE i.object_id = OBJECT_ID('{}')
              AND i.name IS NOT NULL
            ORDER BY i.is_primary_key DESC, i.name
        )",
                                      escapeSqlString(tableName));

        auto queryResult = driver->execute(indexQuery);

        auto json = JsonUtils::buildRowArray(queryResult.rows, 5, [](std::string& out, const ResultRow& row) {
            out += "{";
            out += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            out += std::format("\"type\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            out += std::format("\"isUnique\":{},", row.values[2] == "1" ? "true" : "false");
            out += std::format("\"isPrimaryKey\":{},", row.values[3] == "1" ? "true" : "false");
            out += "\"columns\":";
            out += splitCsvToJsonArray(row.values[4]);
            out += "}";
        });
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetConstraints(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;
        auto [cSchema, cTbl] = splitSchemaTable(tableName);

        auto constraintQuery = std::format(R"(
            SELECT
                tc.CONSTRAINT_NAME,
                tc.CONSTRAINT_TYPE,
                STUFF((
                    SELECT ',' + kcu.COLUMN_NAME
                    FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE kcu
                    WHERE kcu.CONSTRAINT_NAME = tc.CONSTRAINT_NAME
                      AND kcu.TABLE_NAME = tc.TABLE_NAME
                    ORDER BY kcu.ORDINAL_POSITION
                    FOR XML PATH('')
                ), 1, 1, '') AS Columns,
                ISNULL(cc.CHECK_CLAUSE, dc.definition) AS Definition
            FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS tc
            LEFT JOIN INFORMATION_SCHEMA.CHECK_CONSTRAINTS cc
                ON tc.CONSTRAINT_NAME = cc.CONSTRAINT_NAME
            LEFT JOIN sys.default_constraints dc
                ON dc.name = tc.CONSTRAINT_NAME
            WHERE tc.TABLE_NAME = '{}' AND tc.TABLE_SCHEMA = '{}'
            ORDER BY tc.CONSTRAINT_TYPE, tc.CONSTRAINT_NAME
        )",
                                           escapeSqlString(cTbl), escapeSqlString(cSchema));

        auto queryResult = driver->execute(constraintQuery);

        auto json = JsonUtils::buildRowArray(queryResult.rows, 4, [](std::string& out, const ResultRow& row) {
            out += "{";
            out += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            out += std::format("\"type\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            out += "\"columns\":";
            out += splitCsvToJsonArray(row.values[2]);
            out += ",";
            out += std::format("\"definition\":\"{}\"", JsonUtils::escapeString(row.values[3]));
            out += "}";
        });
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetForeignKeys(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

        auto fkQuery = std::format(R"(
            SELECT
                fk.name AS FKName,
                STUFF((
                    SELECT ',' + COL_NAME(fkc.parent_object_id, fkc.parent_column_id)
                    FROM sys.foreign_key_columns fkc
                    WHERE fkc.constraint_object_id = fk.object_id
                    ORDER BY fkc.constraint_column_id
                    FOR XML PATH('')
                ), 1, 1, '') AS Columns,
                OBJECT_SCHEMA_NAME(fk.referenced_object_id) + '.' + OBJECT_NAME(fk.referenced_object_id) AS ReferencedTable,
                STUFF((
                    SELECT ',' + COL_NAME(fkc.referenced_object_id, fkc.referenced_column_id)
                    FROM sys.foreign_key_columns fkc
                    WHERE fkc.constraint_object_id = fk.object_id
                    ORDER BY fkc.constraint_column_id
                    FOR XML PATH('')
                ), 1, 1, '') AS ReferencedColumns,
                fk.delete_referential_action_desc AS OnDelete,
                fk.update_referential_action_desc AS OnUpdate
            FROM sys.foreign_keys fk
            WHERE fk.parent_object_id = OBJECT_ID('{}')
            ORDER BY fk.name
        )",
                                   escapeSqlString(tableName));

        auto queryResult = driver->execute(fkQuery);

        auto json = JsonUtils::buildRowArray(queryResult.rows, 6, [](std::string& out, const ResultRow& row) {
            out += "{";
            out += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            out += "\"columns\":";
            out += splitCsvToJsonArray(row.values[1]);
            out += ",";
            out += std::format("\"referencedTable\":\"{}\",", JsonUtils::escapeString(row.values[2]));
            out += "\"referencedColumns\":";
            out += splitCsvToJsonArray(row.values[3]);
            out += ",";
            out += std::format("\"onDelete\":\"{}\",", JsonUtils::escapeString(row.values[4]));
            out += std::format("\"onUpdate\":\"{}\"", JsonUtils::escapeString(row.values[5]));
            out += "}";
        });
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetReferencingForeignKeys(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

        auto refFkQuery = std::format(R"(
            SELECT
                fk.name AS FKName,
                OBJECT_SCHEMA_NAME(fk.parent_object_id) + '.' + OBJECT_NAME(fk.parent_object_id) AS ReferencingTable,
                STUFF((
                    SELECT ',' + COL_NAME(fkc.parent_object_id, fkc.parent_column_id)
                    FROM sys.foreign_key_columns fkc
                    WHERE fkc.constraint_object_id = fk.object_id
                    ORDER BY fkc.constraint_column_id
                    FOR XML PATH('')
                ), 1, 1, '') AS ReferencingColumns,
                STUFF((
                    SELECT ',' + COL_NAME(fkc.referenced_object_id, fkc.referenced_column_id)
                    FROM sys.foreign_key_columns fkc
                    WHERE fkc.constraint_object_id = fk.object_id
                    ORDER BY fkc.constraint_column_id
                    FOR XML PATH('')
                ), 1, 1, '') AS Columns,
                fk.delete_referential_action_desc AS OnDelete,
                fk.update_referential_action_desc AS OnUpdate
            FROM sys.foreign_keys fk
            WHERE fk.referenced_object_id = OBJECT_ID('{}')
            ORDER BY fk.name
        )",
                                      escapeSqlString(tableName));

        auto queryResult = driver->execute(refFkQuery);

        auto json = JsonUtils::buildRowArray(queryResult.rows, 6, [](std::string& out, const ResultRow& row) {
            out += "{";
            out += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            out += std::format("\"referencingTable\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            out += "\"referencingColumns\":";
            out += splitCsvToJsonArray(row.values[2]);
            out += ",";
            out += "\"columns\":";
            out += splitCsvToJsonArray(row.values[3]);
            out += ",";
            out += std::format("\"onDelete\":\"{}\",", JsonUtils::escapeString(row.values[4]));
            out += std::format("\"onUpdate\":\"{}\"", JsonUtils::escapeString(row.values[5]));
            out += "}";
        });
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetTriggers(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

        auto triggerQuery = std::format(R"(
            SELECT
                t.name AS TriggerName,
                CASE WHEN t.is_instead_of_trigger = 1 THEN 'INSTEAD OF' ELSE 'AFTER' END AS TriggerType,
                STUFF((
                    SELECT ',' + CASE te.type WHEN 1 THEN 'INSERT' WHEN 2 THEN 'UPDATE' WHEN 3 THEN 'DELETE' END
                    FROM sys.trigger_events te
                    WHERE te.object_id = t.object_id
                    FOR XML PATH('')
                ), 1, 1, '') AS Events,
                CASE WHEN t.is_disabled = 0 THEN 1 ELSE 0 END AS IsEnabled,
                OBJECT_DEFINITION(t.object_id) AS Definition
            FROM sys.triggers t
            WHERE t.parent_id = OBJECT_ID('{}')
            ORDER BY t.name
        )",
                                        escapeSqlString(tableName));

        auto queryResult = driver->execute(triggerQuery);

        auto json = JsonUtils::buildRowArray(queryResult.rows, 5, [](std::string& out, const ResultRow& row) {
            out += "{";
            out += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            out += std::format("\"type\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            out += "\"events\":";
            out += splitCsvToJsonArray(row.values[2]);
            out += ",";
            out += std::format("\"isEnabled\":{},", row.values[3] == "1" ? "true" : "false");
            out += std::format("\"definition\":\"{}\"", JsonUtils::escapeString(row.values[4]));
            out += "}";
        });
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetTableMetadata(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

        auto metadataQuery = std::format(R"(
            SELECT
                OBJECT_SCHEMA_NAME(o.object_id) AS SchemaName,
                o.name AS TableName,
                o.type_desc AS ObjectType,
                ISNULL(p.rows, 0) AS RowCount,
                CONVERT(varchar, o.create_date, 120) AS CreatedAt,
                CONVERT(varchar, o.modify_date, 120) AS ModifiedAt,
                ISNULL(USER_NAME(o.principal_id), 'dbo') AS Owner,
                ISNULL(ep.value, '') AS Comment
            FROM sys.objects o
            LEFT JOIN sys.partitions p ON o.object_id = p.object_id AND p.index_id IN (0, 1)
            LEFT JOIN sys.extended_properties ep ON ep.major_id = o.object_id AND ep.minor_id = 0 AND ep.name = 'MS_Description'
            WHERE o.object_id = OBJECT_ID('{}')
        )",
                                         escapeSqlString(tableName));

        auto queryResult = driver->execute(metadataQuery);

        if (queryResult.rows.empty()) {
            return JsonUtils::errorResponse("Table not found");
        }

        const auto& row = queryResult.rows[0];
        if (row.values.size() < 8) {
            return JsonUtils::errorResponse("Unexpected column count in metadata result");
        }
        std::string json = "{";
        json += std::format("\"schema\":\"{}\",", JsonUtils::escapeString(row.values[0]));
        json += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[1]));
        json += std::format("\"type\":\"{}\",", JsonUtils::escapeString(row.values[2]));
        json += std::format("\"rowCount\":{},", row.values[3]);
        json += std::format("\"createdAt\":\"{}\",", JsonUtils::escapeString(row.values[4]));
        json += std::format("\"modifiedAt\":\"{}\",", JsonUtils::escapeString(row.values[5]));
        json += std::format("\"owner\":\"{}\",", JsonUtils::escapeString(row.values[6]));
        json += std::format("\"comment\":\"{}\"", JsonUtils::escapeString(row.values[7]));
        json += "}";
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetTableDDL(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params).value();

        auto extracted = extractTableQueryParams(doc, m_connections);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;
        auto [ddlSchema, ddlTbl] = splitSchemaTable(tableName);

        auto columnQuery = std::format(R"(
            SELECT
                c.COLUMN_NAME,
                c.DATA_TYPE,
                c.CHARACTER_MAXIMUM_LENGTH,
                c.NUMERIC_PRECISION,
                c.NUMERIC_SCALE,
                c.IS_NULLABLE,
                c.COLUMN_DEFAULT
            FROM INFORMATION_SCHEMA.COLUMNS c
            WHERE c.TABLE_NAME = '{}' AND c.TABLE_SCHEMA = '{}'
            ORDER BY c.ORDINAL_POSITION
        )",
                                       escapeSqlString(ddlTbl), escapeSqlString(ddlSchema));

        auto columnResult = driver->execute(columnQuery);

        auto sanitizedTable = quoteBracketIdentifier(tableName);
        std::string ddl = "CREATE TABLE " + sanitizedTable + " (\n";
        bool first = true;
        for (const auto& row : columnResult.rows) {
            if (row.values.size() < 7)
                continue;
            if (!first)
                ddl += ",\n";
            first = false;
            ddl += "    " + quoteBracketIdentifier(row.values[0]) + " " + row.values[1];
            if (!row.values[2].empty() && row.values[2] != "-1") {
                ddl += "(" + row.values[2] + ")";
            } else if (!row.values[3].empty() && row.values[3] != "0") {
                ddl += "(" + row.values[3];
                if (!row.values[4].empty() && row.values[4] != "0")
                    ddl += "," + row.values[4];
                ddl += ")";
            }
            if (row.values[5] == "NO")
                ddl += " NOT NULL";
            if (!row.values[6].empty())
                ddl += " DEFAULT " + row.values[6];
        }

        auto pkQuery = std::format(R"(
            SELECT COLUMN_NAME
            FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE
            WHERE TABLE_NAME = '{}' AND TABLE_SCHEMA = '{}'
              AND CONSTRAINT_NAME = (
                  SELECT CONSTRAINT_NAME
                  FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
                  WHERE TABLE_NAME = '{}' AND TABLE_SCHEMA = '{}' AND CONSTRAINT_TYPE = 'PRIMARY KEY'
              )
            ORDER BY ORDINAL_POSITION
        )",
                                   escapeSqlString(ddlTbl), escapeSqlString(ddlSchema), escapeSqlString(ddlTbl), escapeSqlString(ddlSchema));

        auto pkResult = driver->execute(pkQuery);
        if (!pkResult.rows.empty()) {
            ddl += ",\n    CONSTRAINT " + quoteBracketIdentifier("PK_" + tableName) + " PRIMARY KEY (";
            bool pkFirst = true;
            for (const auto& row : pkResult.rows) {
                if (row.values.empty())
                    continue;
                if (!pkFirst)
                    ddl += ", ";
                pkFirst = false;
                ddl += quoteBracketIdentifier(row.values[0]);
            }
            ddl += ")";
        }
        ddl += "\n);";

        return JsonUtils::successResponse(std::format("{{\"ddl\":\"{}\"}}", JsonUtils::escapeString(ddl)));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SchemaProvider::handleGetExecutionPlan(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        auto sqlQueryResult = doc["sql"].get_string();
        if (connectionIdResult.error() || sqlQueryResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required fields: connectionId or sql");
        }
        auto connectionId = std::string(connectionIdResult.value());
        auto sqlQuery = std::string(sqlQueryResult.value());
        bool actualPlan = false;
        if (auto actual = doc["actual"].get_bool(); !actual.error())
            actualPlan = actual.value();

        auto driver = m_connections.getQueryDriver(connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        std::string planQuery;
        if (actualPlan) {
            planQuery = std::format("SET STATISTICS XML ON;\n{}\nSET STATISTICS XML OFF;", sqlQuery);
        } else {
            planQuery = std::format("SET SHOWPLAN_TEXT ON;\n{}\nSET SHOWPLAN_TEXT OFF;", sqlQuery);
        }

        auto queryResult = driver->execute(planQuery);

        std::string planText;
        for (const auto& row : queryResult.rows) {
            for (const auto& value : row.values) {
                if (!planText.empty())
                    planText += "\n";
                planText += value;
            }
        }

        auto planJson = std::format(R"({{"plan":"{}","actual":{}}})", JsonUtils::escapeString(planText), actualPlan ? "true" : "false");
        return JsonUtils::successResponse(planJson);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

}  // namespace velocitydb
