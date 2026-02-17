#include "database_context.h"

#include "../database/async_query_executor.h"
#include "../database/connection_registry.h"
#include "../database/connection_utils.h"
#include "../database/driver_interface.h"
#include "../database/query_history.h"
#include "../database/result_cache.h"
#include "../database/schema_inspector.h"
#include "../database/sqlserver_driver.h"
#include "../database/transaction_manager.h"
#include "../network/ssh_tunnel.h"
#include "../parsers/sql_parser.h"
#include "../utils/json_utils.h"
#include "../utils/logger.h"
#include "../utils/simd_filter.h"
#include "../utils/sql_validation.h"
#include "simdjson.h"

#include <charconv>
#include <chrono>
#include <format>
#include <ranges>

using namespace std::literals;

namespace velocitydb {

namespace {

/// Sanitize a single SQL Server identifier part (no dots).
[[nodiscard]] std::string sanitizeIdentifierPart(std::string_view part) {
    if (part.empty())
        return {};
    std::string result;
    result.reserve(part.size() + 2);
    result += '[';
    for (char c : part) {
        if (c == ']')
            result += "]]";
        else
            result += c;
    }
    result += ']';
    return result;
}

/// Sanitize SQL Server identifier (table/column name) to prevent SQL injection.
[[nodiscard]] std::string sanitizeIdentifier(std::string_view identifier) {
    if (identifier.empty())
        return {};
    std::string result;
    result.reserve(identifier.size() + 4);
    size_t start = 0;
    size_t pos = identifier.find('.');
    while (pos != std::string_view::npos) {
        auto part = sanitizeIdentifierPart(identifier.substr(start, pos - start));
        if (!part.empty()) {
            if (!result.empty())
                result += '.';
            result += part;
        }
        start = pos + 1;
        pos = identifier.find('.', start);
    }
    auto lastPart = sanitizeIdentifierPart(identifier.substr(start));
    if (!lastPart.empty()) {
        if (!result.empty())
            result += '.';
        result += lastPart;
    }
    return result;
}

/// Escape a string for use in SQL string literals (doubles single quotes).
[[nodiscard]] std::string escapeSqlString(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        if (c == '\'')
            result += "''";
        else
            result += c;
    }
    return result;
}

/// Convert comma-separated string to JSON array of quoted strings.
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

/// Helper to get SQLServerDriver from connection registry (query driver)
[[nodiscard]] std::shared_ptr<SQLServerDriver> getDriver(const ConnectionRegistry& registry, std::string_view connectionId) {
    auto driverResult = registry.getQueryDriver(connectionId);
    if (!driverResult)
        return nullptr;
    return std::dynamic_pointer_cast<SQLServerDriver>(*driverResult);
}

/// Helper to get SQLServerDriver for metadata from connection registry
[[nodiscard]] std::shared_ptr<SQLServerDriver> getMetaDriver(const ConnectionRegistry& registry, std::string_view connectionId) {
    auto driverResult = registry.getMetadataDriver(connectionId);
    if (!driverResult)
        return nullptr;
    return std::dynamic_pointer_cast<SQLServerDriver>(*driverResult);
}

struct TableQueryParams {
    std::string tableName;
    std::shared_ptr<SQLServerDriver> driver;
};

/// 8ハンドラ共通: JSON→connectionId/table取得→バリデーション→ドライバ取得
[[nodiscard]] std::expected<TableQueryParams, std::string> extractTableQueryParams(const simdjson::dom::element& doc, ConnectionRegistry& registry) {
    auto connectionIdResult = doc["connectionId"].get_string();
    auto tableNameResult = doc["table"].get_string();
    if (connectionIdResult.error() || tableNameResult.error()) [[unlikely]]
        return std::unexpected("Missing required fields: connectionId or table");

    auto tableName = std::string(tableNameResult.value());
    if (!isValidIdentifier(tableName)) [[unlikely]]
        return std::unexpected("Invalid table name");

    auto connectionId = std::string(connectionIdResult.value());
    auto driver = getMetaDriver(registry, connectionId);
    if (!driver) [[unlikely]]
        return std::unexpected(std::format("Connection not found: {}", connectionId));

    return TableQueryParams{std::move(tableName), std::move(driver)};
}

}  // namespace

DatabaseContext::DatabaseContext()
    : m_registry(std::make_unique<ConnectionRegistry>())
    , m_schemaInspector(std::make_unique<SchemaInspector>())
    , m_asyncExecutor(std::make_unique<AsyncQueryExecutor>())
    , m_resultCache(std::make_unique<ResultCache>())
    , m_queryHistory(std::make_unique<QueryHistory>()) {}

DatabaseContext::~DatabaseContext() = default;

// ─── Driver access ──────────────────────────────────────────────────

std::shared_ptr<SQLServerDriver> DatabaseContext::getQueryDriver(std::string_view connectionId) {
    return getDriver(*m_registry, connectionId);
}

std::shared_ptr<SQLServerDriver> DatabaseContext::getMetadataDriver(std::string_view connectionId) {
    return getMetaDriver(*m_registry, connectionId);
}

// ─── Connection lifecycle ───────────────────────────────────────────

std::string DatabaseContext::handleConnect(std::string_view params) {
    auto connectionParams = extractConnectionParams(params);
    if (!connectionParams) {
        return JsonUtils::errorResponse(connectionParams.error());
    }

    std::unique_ptr<SshTunnel> sshTunnel;
    DatabaseConnectionParams effectiveParams = *connectionParams;

    if (connectionParams->ssh.enabled) {
        auto tunnelResult = establishSshTunnel(*connectionParams);
        if (!tunnelResult) {
            return JsonUtils::errorResponse(tunnelResult.error());
        }
        sshTunnel = std::move(*tunnelResult);
        effectiveParams.server = std::format("127.0.0.1,{}", sshTunnel->getLocalPort());
        log<LogLevel::DEBUG>(std::format("[DB] SSH tunnel established, redirecting to: {}", effectiveParams.server));
    }

    auto odbcString = buildODBCConnectionString(effectiveParams);
    log<LogLevel::DEBUG>(std::format("[DB] ODBC connection target: {}", effectiveParams.server));
    log<LogLevel::DEBUG>("[DB] Attempting ODBC connection...");
    log_flush();

    auto queryDriverPtr = std::make_shared<SQLServerDriver>();
    if (!queryDriverPtr->connect(odbcString)) {
        return JsonUtils::errorResponse(std::format("Connection failed: {}", queryDriverPtr->getLastError()));
    }

    auto metadataDriverPtr = std::make_shared<SQLServerDriver>();
    if (!metadataDriverPtr->connect(odbcString)) {
        queryDriverPtr->disconnect();
        return JsonUtils::errorResponse(std::format("Metadata connection failed: {}", metadataDriverPtr->getLastError()));
    }

    auto connectionId = m_registry->add(queryDriverPtr, metadataDriverPtr);
    if (sshTunnel) {
        m_registry->attachTunnel(connectionId, std::move(sshTunnel));
    }

    return JsonUtils::successResponse(std::format(R"({{"connectionId":"{}"}})", connectionId));
}

std::string DatabaseContext::handleDisconnect(std::string_view params) {
    auto connectionIdResult = extractConnectionId(params);
    if (!connectionIdResult) {
        return JsonUtils::errorResponse(connectionIdResult.error());
    }
    auto connectionId = *connectionIdResult;

    {
        std::lock_guard lock(m_txMutex);
        m_transactionManagers.erase(connectionId);
    }
    m_registry->remove(connectionId);

    return JsonUtils::successResponse("{}");
}

std::string DatabaseContext::handleTestConnection(std::string_view params) {
    auto connectionParams = extractConnectionParams(params);
    if (!connectionParams) {
        return JsonUtils::errorResponse(connectionParams.error());
    }

    std::unique_ptr<SshTunnel> sshTunnel;
    DatabaseConnectionParams effectiveParams = *connectionParams;

    if (connectionParams->ssh.enabled) {
        auto tunnelResult = establishSshTunnel(*connectionParams);
        if (!tunnelResult) {
            return JsonUtils::successResponse(std::format(R"({{"success":false,"message":"{}"}})", JsonUtils::escapeString(tunnelResult.error())));
        }
        sshTunnel = std::move(*tunnelResult);
        effectiveParams.server = std::format("127.0.0.1,{}", sshTunnel->getLocalPort());
        log<LogLevel::DEBUG>(std::format("[DB] SSH tunnel established, redirecting to: {}", effectiveParams.server));
    }

    auto odbcString = buildODBCConnectionString(effectiveParams);
    log<LogLevel::DEBUG>(std::format("[DB] ODBC connection target: {}", effectiveParams.server));
    log<LogLevel::DEBUG>("[DB] Attempting ODBC connection...");
    log_flush();

    SQLServerDriver driver{};
    if (driver.connect(odbcString)) {
        driver.disconnect();
        return JsonUtils::successResponse(R"({"success":true,"message":"Connection successful"})");
    }

    return JsonUtils::successResponse(std::format(R"({{"success":false,"message":"{}"}})", JsonUtils::escapeString(driver.getLastError())));
}

// ─── Query execution ────────────────────────────────────────────────

std::string DatabaseContext::handleExecuteQuery(std::string_view params) {
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

        auto driver = getDriver(*m_registry, connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        auto statements = SQLParser::splitStatements(sqlQuery);
        log<LogLevel::INFO>(std::format("Split SQL into {} statements", statements.size()));

        // Multiple statements
        if (statements.size() > 1) {
            struct StatementResult {
                std::string statement;
                ResultSet result;
            };
            std::vector<StatementResult> allResults;

            size_t stmtIdx = 0;
            try {
                for (const auto& stmt : statements) {
                    auto stmtStart = std::chrono::high_resolution_clock::now();
                    ResultSet currentResult;
                    if (SQLParser::isUseStatement(stmt)) {
                        std::string dbName = SQLParser::extractDatabaseName(stmt);
                        [[maybe_unused]] auto _ = driver->execute(stmt);
                        currentResult.columns.push_back({.name = "Message", .type = "VARCHAR", .size = 255, .nullable = false, .isPrimaryKey = false});
                        ResultRow messageRow;
                        messageRow.values.push_back(std::format("Database changed to {}", dbName));
                        currentResult.rows.push_back(messageRow);
                        currentResult.affectedRows = 0;
                    } else {
                        currentResult = driver->execute(stmt);
                    }
                    auto stmtEnd = std::chrono::high_resolution_clock::now();
                    currentResult.executionTimeMs = std::chrono::duration<double, std::milli>(stmtEnd - stmtStart).count();
                    allResults.push_back({.statement = stmt, .result = std::move(currentResult)});
                    ++stmtIdx;
                }

                std::string jsonResponse = R"({"multipleResults":true,"results":[)";
                for (size_t i = 0; i < allResults.size(); ++i) {
                    if (i > 0)
                        jsonResponse += ",";
                    jsonResponse += R"({"statement":")";
                    jsonResponse += JsonUtils::escapeString(allResults[i].statement);
                    jsonResponse += R"(","data":)";
                    jsonResponse += JsonUtils::serializeResultSet(allResults[i].result, false);
                    jsonResponse += "}";
                }
                jsonResponse += "]}";
                return JsonUtils::successResponse(jsonResponse);
            } catch (const std::exception& e) {
                return JsonUtils::errorResponse(std::format("Statement {} of {}: {}", stmtIdx + 1, statements.size(), e.what()));
            }
        }

        // Single USE statement
        if (SQLParser::isUseStatement(sqlQuery)) {
            std::string dbName = SQLParser::extractDatabaseName(sqlQuery);
            try {
                [[maybe_unused]] auto _ = driver->execute(sqlQuery);
                ResultSet useResult;
                useResult.columns.push_back({.name = "Message", .type = "VARCHAR", .size = 255, .nullable = false, .isPrimaryKey = false});
                ResultRow messageRow;
                messageRow.values.push_back(std::format("Database changed to {}", dbName));
                useResult.rows.push_back(messageRow);
                useResult.affectedRows = 0;
                useResult.executionTimeMs = 0.0;
                return JsonUtils::successResponse(JsonUtils::serializeResultSet(useResult, false));
            } catch (const std::exception& e) {
                return JsonUtils::errorResponse(std::format("Failed to switch database: {}", e.what()));
            }
        }

        // Cache check
        bool useCache = true;
        if (auto useCacheOpt = doc["useCache"].get_bool(); !useCacheOpt.error()) {
            useCache = useCacheOpt.value();
        }
        std::string cacheKey;
        cacheKey.reserve(connectionId.size() + 1 + sqlQuery.size());
        cacheKey.append(connectionId);
        cacheKey.push_back('\0');
        cacheKey.append(sqlQuery);
        bool selectQuery = SQLParser::isReadOnlyQuery(sqlQuery);
        if (useCache && selectQuery) {
            if (auto cachedResult = m_resultCache->get(cacheKey); cachedResult.has_value()) {
                return JsonUtils::successResponse(JsonUtils::serializeResultSet(*cachedResult, true));
            }
        }

        auto queryResult = driver->execute(sqlQuery);

        if (useCache && selectQuery) {
            m_resultCache->put(cacheKey, queryResult);
        }

        std::string jsonResponse = JsonUtils::serializeResultSet(queryResult, false);

        HistoryItem historyEntry{.id = std::format("hist_{}", std::chrono::system_clock::now().time_since_epoch().count()),
                                 .sql = sqlQuery,
                                 .executionTimeMs = queryResult.executionTimeMs,
                                 .success = true,
                                 .affectedRows = static_cast<int64_t>(queryResult.affectedRows),
                                 .isFavorite = false};
        m_queryHistory->add(historyEntry);

        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleExecuteQueryPaginated(std::string_view params) {
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

        int64_t startRow = 0;
        int64_t endRow = 100;
        if (auto startRowOpt = doc["startRow"].get_int64(); !startRowOpt.error())
            startRow = startRowOpt.value();
        if (auto endRowOpt = doc["endRow"].get_int64(); !endRowOpt.error())
            endRow = endRowOpt.value();

        std::string orderByClause;
        if (auto sortModel = doc["sortModel"].get_array(); !sortModel.error()) {
            std::string sortClauses;
            for (auto item : sortModel.value()) {
                auto colId = item["colId"].get_string();
                auto sort = item["sort"].get_string();
                if (!colId.error() && !sort.error()) {
                    if (!sortClauses.empty())
                        sortClauses += ", ";
                    sortClauses += sanitizeIdentifier(std::string(colId.value())) + " " + (sort.value() == std::string_view("asc") ? "ASC" : "DESC");
                }
            }
            if (!sortClauses.empty())
                orderByClause = " ORDER BY " + sortClauses;
        }

        auto driver = getDriver(*m_registry, connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        std::string paginatedQuery;
        if (orderByClause.empty()) {
            paginatedQuery = std::format("{} ORDER BY (SELECT NULL) OFFSET {} ROWS FETCH NEXT {} ROWS ONLY", sqlQuery, startRow, endRow - startRow);
        } else {
            paginatedQuery = std::format("{}{} OFFSET {} ROWS FETCH NEXT {} ROWS ONLY", sqlQuery, orderByClause, startRow, endRow - startRow);
        }

        auto queryResult = driver->execute(paginatedQuery);
        return JsonUtils::successResponse(JsonUtils::serializeResultSet(queryResult, false));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetRowCount(std::string_view params) {
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

        auto driver = getDriver(*m_registry, connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        auto countQuery = std::format("SELECT COUNT_BIG(*) AS total_rows FROM ({}) AS subquery WITH(NOLOCK)", sqlQuery);
        auto queryResult = driver->execute(countQuery);

        if (queryResult.rows.empty() || queryResult.rows[0].values.empty()) {
            return JsonUtils::errorResponse("Failed to get row count");
        }

        auto rowCount = queryResult.rows[0].values[0];
        return JsonUtils::successResponse(std::format("{{\"rowCount\":{}}}", rowCount));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleCancelQuery(std::string_view params) {
    auto connectionIdResult = extractConnectionId(params);
    if (!connectionIdResult) {
        return JsonUtils::errorResponse(connectionIdResult.error());
    }
    if (auto driver = getDriver(*m_registry, *connectionIdResult)) {
        driver->cancel();
    }
    return JsonUtils::successResponse("{}");
}

// ─── Async queries ──────────────────────────────────────────────────

std::string DatabaseContext::handleExecuteAsyncQuery(std::string_view params) {
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

        auto driver = getDriver(*m_registry, connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        std::string queryId = m_asyncExecutor->submitQuery(driver, sqlQuery);
        return JsonUtils::successResponse(std::format(R"({{"queryId":"{}"}})", queryId));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetAsyncQueryResult(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto queryIdResult = doc["queryId"].get_string();
        if (queryIdResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: queryId");
        }
        auto queryId = std::string(queryIdResult.value());

        // Safety net: evict completed queries older than 5 minutes
        [[maybe_unused]] auto evicted = m_asyncExecutor->evictStaleQueries();

        AsyncQueryResult asyncResult = m_asyncExecutor->getQueryResult(queryId);

        std::string statusStr;
        switch (asyncResult.status) {
            case QueryStatus::Pending:
                statusStr = "pending";
                break;
            case QueryStatus::Running:
                statusStr = "running";
                break;
            case QueryStatus::Completed:
                statusStr = "completed";
                break;
            case QueryStatus::Cancelled:
                statusStr = "cancelled";
                break;
            case QueryStatus::Failed:
                statusStr = "failed";
                break;
        }

        std::string jsonResponse = "{";
        jsonResponse += std::format(R"("queryId":"{}","status":"{}")", asyncResult.queryId, statusStr);

        if (!asyncResult.errorMessage.empty()) {
            jsonResponse += std::format(R"(,"error":"{}")", JsonUtils::escapeString(asyncResult.errorMessage));
        }

        if (asyncResult.multipleResults && !asyncResult.results.empty()) {
            jsonResponse += R"(,"multipleResults":true,"results":[)";
            for (size_t i = 0; i < asyncResult.results.size(); ++i) {
                if (i > 0)
                    jsonResponse += ",";
                const auto& stmtResult = asyncResult.results[i];
                jsonResponse += R"({"statement":")";
                jsonResponse += JsonUtils::escapeString(stmtResult.statement);
                jsonResponse += R"(","data":)";
                jsonResponse += JsonUtils::serializeResultSet(stmtResult.result, false);
                jsonResponse += "}";
            }
            jsonResponse += "]";
        } else if (asyncResult.result.has_value()) {
            const auto& queryResult = *asyncResult.result;
            jsonResponse += R"(,"columns":[)";
            for (size_t i = 0; i < queryResult.columns.size(); ++i) {
                if (i > 0)
                    jsonResponse += ',';
                jsonResponse += std::format(R"({{"name":"{}","type":"{}"}})", JsonUtils::escapeString(queryResult.columns[i].name), queryResult.columns[i].type);
            }
            jsonResponse += "],";
            jsonResponse += R"("rows":[)";
            for (size_t rowIndex = 0; rowIndex < queryResult.rows.size(); ++rowIndex) {
                if (rowIndex > 0)
                    jsonResponse += ',';
                jsonResponse += '[';
                for (size_t colIndex = 0; colIndex < queryResult.rows[rowIndex].values.size(); ++colIndex) {
                    if (colIndex > 0)
                        jsonResponse += ',';
                    jsonResponse += std::format(R"("{}")", JsonUtils::escapeString(queryResult.rows[rowIndex].values[colIndex]));
                }
                jsonResponse += ']';
            }
            jsonResponse += "],";
            jsonResponse += std::format(R"("affectedRows":{},"executionTimeMs":{})", queryResult.affectedRows, queryResult.executionTimeMs);
        }

        jsonResponse += "}";
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleCancelAsyncQuery(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto queryIdResult = doc["queryId"].get_string();
        if (queryIdResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: queryId");
        }
        bool cancelled = m_asyncExecutor->cancelQuery(std::string(queryIdResult.value()));
        return JsonUtils::successResponse(std::format(R"({{"cancelled":{}}})", cancelled ? "true" : "false"));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleRemoveAsyncQuery(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto queryIdResult = doc["queryId"].get_string();
        if (queryIdResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: queryId");
        }
        bool removed = m_asyncExecutor->removeQuery(std::string(queryIdResult.value()));
        return JsonUtils::successResponse(std::format(R"({{"removed":{}}})", removed ? "true" : "false"));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetActiveQueries(std::string_view) {
    auto activeIds = m_asyncExecutor->getActiveQueryIds();
    std::string jsonResponse = "[";
    bool first = true;
    for (const auto& id : activeIds) {
        if (!first)
            jsonResponse += ',';
        jsonResponse += std::format(R"("{}")", id);
        first = false;
    }
    jsonResponse += "]";
    return JsonUtils::successResponse(jsonResponse);
}

// ─── Schema ─────────────────────────────────────────────────────────

std::string DatabaseContext::handleGetDatabases(std::string_view params) {
    auto connectionIdResult = extractConnectionId(params);
    if (!connectionIdResult) {
        return JsonUtils::errorResponse(connectionIdResult.error());
    }
    try {
        auto driver = getMetaDriver(*m_registry, *connectionIdResult);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", *connectionIdResult));
        }
        auto queryResult = driver->execute("SELECT name FROM sys.databases ORDER BY name");
        std::string jsonResponse = "[";
        for (size_t i = 0; i < queryResult.rows.size(); ++i) {
            if (queryResult.rows[i].values.empty())
                continue;
            if (i > 0)
                jsonResponse += ',';
            jsonResponse += std::format(R"("{}")", JsonUtils::escapeString(queryResult.rows[i].values[0]));
        }
        jsonResponse += ']';
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetTables(std::string_view params) {
    log<LogLevel::DEBUG>(std::format("DatabaseContext::handleGetTables called with params: {}", params));
    auto connectionIdResult = extractConnectionId(params);
    if (!connectionIdResult) {
        return JsonUtils::errorResponse(connectionIdResult.error());
    }
    auto connectionId = *connectionIdResult;
    try {
        auto driver = getMetaDriver(*m_registry, connectionId);
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
        std::string jsonResponse = "[";
        for (size_t i = 0; i < queryResult.rows.size(); ++i) {
            if (i > 0)
                jsonResponse += ',';
            std::string comment = queryResult.rows[i].values.size() >= 4 ? queryResult.rows[i].values[3] : "";
            jsonResponse += std::format(R"({{"schema":"{}","name":"{}","type":"{}","comment":"{}"}})", JsonUtils::escapeString(queryResult.rows[i].values[0]),
                                        JsonUtils::escapeString(queryResult.rows[i].values[1]), JsonUtils::escapeString(queryResult.rows[i].values[2]), JsonUtils::escapeString(comment));
        }
        jsonResponse += ']';
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetColumns(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

        auto tbl = unquoteBracketIdentifier(tableName);
        std::string schema = "dbo";
        if (auto dotPos = tbl.find('.'); dotPos != std::string::npos) {
            schema = tbl.substr(0, dotPos);
            tbl = tbl.substr(dotPos + 1);
        }

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

        std::string jsonResponse = "[";
        for (size_t i = 0; i < columnResult.rows.size(); ++i) {
            if (i > 0)
                jsonResponse += ',';
            const auto& row = columnResult.rows[i];
            if (row.values.size() >= 5) {
                std::string_view sizeStr = row.values[2];
                int colSize = 0;
                std::from_chars(sizeStr.data(), sizeStr.data() + sizeStr.size(), colSize);
                auto comment = row.values.size() >= 6 ? row.values[5] : std::string{};
                auto nullable = row.values[3] == "1" ? "true" : "false";
                auto isPk = row.values[4] == "1" ? "true" : "false";
                jsonResponse += std::format(R"({{"name":"{}","type":"{}","size":{},"nullable":{},"isPrimaryKey":{},"comment":"{}"}})", JsonUtils::escapeString(row.values[0]),
                                            JsonUtils::escapeString(row.values[1]), colSize, nullable, isPk, JsonUtils::escapeString(comment));
            }
        }
        jsonResponse += ']';
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetIndexes(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
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

        std::string json = "[";
        for (size_t i = 0; i < queryResult.rows.size(); ++i) {
            if (i > 0)
                json += ',';
            const auto& row = queryResult.rows[i];
            json += "{";
            json += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            json += std::format("\"type\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            json += std::format("\"isUnique\":{},", row.values[2] == "1" ? "true" : "false");
            json += std::format("\"isPrimaryKey\":{},", row.values[3] == "1" ? "true" : "false");
            json += "\"columns\":";
            json += splitCsvToJsonArray(row.values[4]);
            json += "}";
        }
        json += "]";
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetConstraints(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

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
            WHERE tc.TABLE_NAME = '{}'
            ORDER BY tc.CONSTRAINT_TYPE, tc.CONSTRAINT_NAME
        )",
                                           escapeSqlString(tableName));

        auto queryResult = driver->execute(constraintQuery);

        std::string json = "[";
        for (size_t i = 0; i < queryResult.rows.size(); ++i) {
            if (i > 0)
                json += ',';
            const auto& row = queryResult.rows[i];
            json += "{";
            json += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            json += std::format("\"type\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            json += "\"columns\":";
            json += splitCsvToJsonArray(row.values[2]);
            json += ",";
            json += std::format("\"definition\":\"{}\"", JsonUtils::escapeString(row.values[3]));
            json += "}";
        }
        json += "]";
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetForeignKeys(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
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

        std::string json = "[";
        for (size_t i = 0; i < queryResult.rows.size(); ++i) {
            if (i > 0)
                json += ',';
            const auto& row = queryResult.rows[i];
            json += "{";
            json += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            json += "\"columns\":";
            json += splitCsvToJsonArray(row.values[1]);
            json += ",";
            json += std::format("\"referencedTable\":\"{}\",", JsonUtils::escapeString(row.values[2]));
            json += "\"referencedColumns\":";
            json += splitCsvToJsonArray(row.values[3]);
            json += ",";
            json += std::format("\"onDelete\":\"{}\",", JsonUtils::escapeString(row.values[4]));
            json += std::format("\"onUpdate\":\"{}\"", JsonUtils::escapeString(row.values[5]));
            json += "}";
        }
        json += "]";
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetReferencingForeignKeys(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
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

        std::string json = "[";
        for (size_t i = 0; i < queryResult.rows.size(); ++i) {
            if (i > 0)
                json += ',';
            const auto& row = queryResult.rows[i];
            json += "{";
            json += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            json += std::format("\"referencingTable\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            json += "\"referencingColumns\":";
            json += splitCsvToJsonArray(row.values[2]);
            json += ",";
            json += "\"columns\":";
            json += splitCsvToJsonArray(row.values[3]);
            json += ",";
            json += std::format("\"onDelete\":\"{}\",", JsonUtils::escapeString(row.values[4]));
            json += std::format("\"onUpdate\":\"{}\"", JsonUtils::escapeString(row.values[5]));
            json += "}";
        }
        json += "]";
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetTriggers(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
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

        std::string json = "[";
        for (size_t i = 0; i < queryResult.rows.size(); ++i) {
            if (i > 0)
                json += ',';
            const auto& row = queryResult.rows[i];
            json += "{";
            json += std::format("\"name\":\"{}\",", JsonUtils::escapeString(row.values[0]));
            json += std::format("\"type\":\"{}\",", JsonUtils::escapeString(row.values[1]));
            json += "\"events\":";
            json += splitCsvToJsonArray(row.values[2]);
            json += ",";
            json += std::format("\"isEnabled\":{},", row.values[3] == "1" ? "true" : "false");
            json += std::format("\"definition\":\"{}\"", JsonUtils::escapeString(row.values[4]));
            json += "}";
        }
        json += "]";
        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetTableMetadata(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
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

std::string DatabaseContext::handleGetTableDDL(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto extracted = extractTableQueryParams(doc, *m_registry);
        if (!extracted) [[unlikely]]
            return JsonUtils::errorResponse(extracted.error());

        auto& [tableName, driver] = *extracted;

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
            WHERE c.TABLE_NAME = '{}'
            ORDER BY c.ORDINAL_POSITION
        )",
                                       escapeSqlString(tableName));

        auto columnResult = driver->execute(columnQuery);

        auto sanitizedTable = sanitizeIdentifier(tableName);
        std::string ddl = "CREATE TABLE " + sanitizedTable + " (\n";
        for (size_t i = 0; i < columnResult.rows.size(); ++i) {
            const auto& row = columnResult.rows[i];
            if (row.values.size() < 7)
                continue;
            if (i > 0)
                ddl += ",\n";
            ddl += "    " + sanitizeIdentifier(row.values[0]) + " " + row.values[1];
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
            WHERE TABLE_NAME = '{}'
              AND CONSTRAINT_NAME = (
                  SELECT CONSTRAINT_NAME
                  FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS
                  WHERE TABLE_NAME = '{}' AND CONSTRAINT_TYPE = 'PRIMARY KEY'
              )
            ORDER BY ORDINAL_POSITION
        )",
                                   escapeSqlString(tableName), escapeSqlString(tableName));

        auto pkResult = driver->execute(pkQuery);
        if (!pkResult.rows.empty()) {
            ddl += ",\n    CONSTRAINT " + sanitizeIdentifier("PK_" + tableName) + " PRIMARY KEY (";
            for (size_t i = 0; i < pkResult.rows.size(); ++i) {
                if (i > 0)
                    ddl += ", ";
                if (!pkResult.rows[i].values.empty())
                    ddl += sanitizeIdentifier(pkResult.rows[i].values[0]);
            }
            ddl += ")";
        }
        ddl += "\n);";

        return JsonUtils::successResponse(std::format("{{\"ddl\":\"{}\"}}", JsonUtils::escapeString(ddl)));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleGetExecutionPlan(std::string_view params) {
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

        auto driver = getDriver(*m_registry, connectionId);
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

// ─── Transactions ───────────────────────────────────────────────────

std::string DatabaseContext::handleBeginTransaction(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        if (connectionIdResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: connectionId");
        }
        auto connectionId = std::string(connectionIdResult.value());

        auto driver = getDriver(*m_registry, connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        {
            std::lock_guard lock(m_txMutex);
            if (!m_transactionManagers.contains(connectionId)) {
                auto txManager = std::make_unique<TransactionManager>();
                txManager->setDriver(driver);
                m_transactionManagers[connectionId] = std::move(txManager);
            }
            m_transactionManagers[connectionId]->begin();
        }
        return JsonUtils::successResponse("{}");
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleCommitTransaction(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        if (connectionIdResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: connectionId");
        }
        auto connectionId = std::string(connectionIdResult.value());

        {
            std::lock_guard lock(m_txMutex);
            auto it = m_transactionManagers.find(connectionId);
            if (it == m_transactionManagers.end()) [[unlikely]] {
                return JsonUtils::errorResponse(std::format("No transaction manager for connection: {}", connectionId));
            }
            it->second->commit();
        }
        return JsonUtils::successResponse("{}");
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string DatabaseContext::handleRollbackTransaction(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        if (connectionIdResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: connectionId");
        }
        auto connectionId = std::string(connectionIdResult.value());

        {
            std::lock_guard lock(m_txMutex);
            auto it = m_transactionManagers.find(connectionId);
            if (it == m_transactionManagers.end()) [[unlikely]] {
                return JsonUtils::errorResponse(std::format("No transaction manager for connection: {}", connectionId));
            }
            it->second->rollback();
        }
        return JsonUtils::successResponse("{}");
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

// ─── Cache & History ────────────────────────────────────────────────

std::string DatabaseContext::handleGetCacheStats(std::string_view) {
    auto currentSize = m_resultCache->getCurrentSize();
    auto maxSize = m_resultCache->getMaxSize();
    std::string jsonResponse = std::format(R"({{"currentSizeBytes":{},"maxSizeBytes":{},"usagePercent":{:.1f}}})", currentSize, maxSize,
                                           maxSize > 0 ? (static_cast<double>(currentSize) / static_cast<double>(maxSize)) * 100.0 : 0.0);
    return JsonUtils::successResponse(jsonResponse);
}

std::string DatabaseContext::handleClearCache(std::string_view) {
    m_resultCache->clear();
    return JsonUtils::successResponse(R"({"cleared":true})");
}

std::string DatabaseContext::handleGetQueryHistory(std::string_view) {
    auto historyEntries = m_queryHistory->getAll();
    std::string jsonResponse = "[";
    for (size_t i = 0; i < historyEntries.size(); ++i) {
        if (i > 0)
            jsonResponse += ',';
        jsonResponse +=
            std::format(R"({{"id":"{}","sql":"{}","executionTimeMs":{},"success":{},"affectedRows":{},"isFavorite":{}}})", historyEntries[i].id, JsonUtils::escapeString(historyEntries[i].sql),
                        historyEntries[i].executionTimeMs, historyEntries[i].success ? "true" : "false", historyEntries[i].affectedRows, historyEntries[i].isFavorite ? "true" : "false");
    }
    jsonResponse += ']';
    return JsonUtils::successResponse(jsonResponse);
}

// ─── Filter ─────────────────────────────────────────────────────────

std::string DatabaseContext::handleFilterResultSet(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        auto sqlQueryResult = doc["sql"].get_string();
        auto columnIndexResult = doc["columnIndex"].get_uint64();
        auto filterTypeResult = doc["filterType"].get_string();
        auto filterValueResult = doc["filterValue"].get_string();
        if (connectionIdResult.error() || sqlQueryResult.error() || columnIndexResult.error() || filterTypeResult.error() || filterValueResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required fields: connectionId, sql, columnIndex, filterType, or filterValue");
        }
        auto connectionId = std::string(connectionIdResult.value());
        auto sqlQuery = std::string(sqlQueryResult.value());
        auto columnIndex = columnIndexResult.value();
        auto filterType = std::string(filterTypeResult.value());
        auto filterValue = std::string(filterValueResult.value());

        auto driver = getDriver(*m_registry, connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        auto queryResult = driver->execute(sqlQuery);

        SIMDFilter simdFilter;
        std::vector<size_t> matchingIndices;
        if (filterType == "equals") {
            matchingIndices = simdFilter.filterEquals(queryResult, columnIndex, filterValue);
        } else if (filterType == "contains") {
            matchingIndices = simdFilter.filterContains(queryResult, columnIndex, filterValue);
        } else if (filterType == "range") {
            std::string minValue = filterValue;
            std::string maxValue;
            if (auto maxVal = doc["filterValueMax"].get_string(); !maxVal.error())
                maxValue = std::string(maxVal.value());
            matchingIndices = simdFilter.filterRange(queryResult, columnIndex, minValue, maxValue);
        } else {
            return JsonUtils::errorResponse(std::format("Unknown filter type: {}", filterType));
        }

        std::string jsonResponse = "{";
        jsonResponse += R"("columns":[)";
        for (size_t i = 0; i < queryResult.columns.size(); ++i) {
            if (i > 0)
                jsonResponse += ',';
            jsonResponse += std::format(R"({{"name":"{}","type":"{}"}})", JsonUtils::escapeString(queryResult.columns[i].name), queryResult.columns[i].type);
        }
        jsonResponse += "],";
        jsonResponse += R"("rows":[)";
        for (size_t i = 0; i < matchingIndices.size(); ++i) {
            if (i > 0)
                jsonResponse += ',';
            jsonResponse += '[';
            const auto& row = queryResult.rows[matchingIndices[i]];
            for (size_t colIndex = 0; colIndex < row.values.size(); ++colIndex) {
                if (colIndex > 0)
                    jsonResponse += ',';
                jsonResponse += std::format(R"("{}")", JsonUtils::escapeString(row.values[colIndex]));
            }
            jsonResponse += ']';
        }
        jsonResponse += "],";
        jsonResponse += std::format(R"("totalRows":{},"filteredRows":{},"simdAvailable":{}}})", queryResult.rows.size(), matchingIndices.size(), SIMDFilter::isAVX2Available() ? "true" : "false");
        return JsonUtils::successResponse(jsonResponse);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

}  // namespace velocitydb
