#include "utility_context.h"

#include "../database/sqlserver_driver.h"
#include "../interfaces/database_context.h"
#include "../parsers/a5er_parser.h"
#include "../parsers/sql_formatter.h"
#include "../utils/global_search.h"
#include "../utils/json_utils.h"
#include "../utils/simd_filter.h"
#include "simdjson.h"

#include <format>

namespace velocitydb {

UtilityContext::UtilityContext()
    : m_sqlFormatter(std::make_unique<SQLFormatter>()), m_globalSearch(std::make_unique<GlobalSearch>()), m_a5erParser(std::make_unique<A5ERParser>()), m_simdFilter(std::make_unique<SIMDFilter>()) {}

UtilityContext::~UtilityContext() = default;
UtilityContext::UtilityContext(UtilityContext&&) noexcept = default;
UtilityContext& UtilityContext::operator=(UtilityContext&&) noexcept = default;

// ─── IPC handle methods ─────────────────────────────────────────────

namespace {

std::string serializeA5ERModelToJson(const A5ERModel& model, const std::string& ddl = "") {
    auto tablesJson = JsonUtils::buildArray(model.tables, [](std::string& out, const auto& table) {
        auto columnsJson = JsonUtils::buildArray(table.columns, [](std::string& out, const auto& col) {
            out += std::format(R"({{"name":"{}","logicalName":"{}","type":"{}","size":{},"scale":{},"nullable":{},"isPrimaryKey":{},"defaultValue":"{}","comment":"{}"}})",
                               JsonUtils::escapeString(col.name), JsonUtils::escapeString(col.logicalName), JsonUtils::escapeString(col.type), col.size, col.scale,
                               col.nullable ? "true" : "false", col.isPrimaryKey ? "true" : "false", JsonUtils::escapeString(col.defaultValue), JsonUtils::escapeString(col.comment));
        });
        auto indexesJson = JsonUtils::buildArray(table.indexes, [](std::string& out, const auto& idx) {
            auto idxColumnsJson = JsonUtils::buildArray(idx.columns, [](std::string& out, const auto& colName) { out += std::format(R"("{}")", JsonUtils::escapeString(colName)); });
            out += std::format(R"({{"name":"{}","columns":{},"isUnique":{}}})", JsonUtils::escapeString(idx.name), idxColumnsJson, idx.isUnique ? "true" : "false");
        });
        out += std::format(R"({{"name":"{}","logicalName":"{}","comment":"{}","page":"{}","columns":{},"indexes":{},"posX":{},"posY":{}}})", JsonUtils::escapeString(table.name),
                           JsonUtils::escapeString(table.logicalName), JsonUtils::escapeString(table.comment), JsonUtils::escapeString(table.page), columnsJson, indexesJson, table.posX,
                           table.posY);
    });

    auto relationsJson = JsonUtils::buildArray(model.relations, [](std::string& out, const auto& rel) {
        out += std::format(R"({{"name":"{}","parentTable":"{}","childTable":"{}","parentColumn":"{}","childColumn":"{}","cardinality":"{}"}})", JsonUtils::escapeString(rel.name),
                           JsonUtils::escapeString(rel.parentTable), JsonUtils::escapeString(rel.childTable), JsonUtils::escapeString(rel.parentColumn),
                           JsonUtils::escapeString(rel.childColumn), JsonUtils::escapeString(rel.cardinality));
    });

    return std::format(R"({{"name":"{}","databaseType":"{}","tables":{},"relations":{},"ddl":"{}"}})", JsonUtils::escapeString(model.name), JsonUtils::escapeString(model.databaseType),
                       tablesJson, relationsJson, JsonUtils::escapeString(ddl));
}

}  // namespace

std::string UtilityContext::handleUppercaseKeywords(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto sqlResult = doc["sql"].get_string();
        if (sqlResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing sql field");
        }
        std::string sqlQuery = std::string(sqlResult.value());

        auto uppercasedSQL = m_sqlFormatter->uppercaseKeywords(sqlQuery);
        return JsonUtils::successResponse(std::format(R"({{"sql":"{}"}})", JsonUtils::escapeString(uppercasedSQL)));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string UtilityContext::handleParseA5ER(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto filepathResult = doc["filepath"].get_string();
        if (filepathResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing filepath field");
        }

        A5ERModel model = m_a5erParser->parse(std::string(filepathResult.value()));
        return JsonUtils::successResponse(serializeA5ERModelToJson(model, m_a5erParser->generateDDL(model)));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string UtilityContext::handleParseA5ERContent(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto contentResult = doc["content"].get_string();
        if (contentResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing content field");
        }

        A5ERModel model = m_a5erParser->parseFromString(std::string(contentResult.value()));

        auto filenameResult = doc["filename"].get_string();
        if (!filenameResult.error() && model.name.empty()) {
            model.name = std::string(filenameResult.value());
        }

        return JsonUtils::successResponse(serializeA5ERModelToJson(model, m_a5erParser->generateDDL(model)));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string UtilityContext::handleSearchObjects(IDatabaseContext& db, std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        auto patternResult = doc["pattern"].get_string();
        if (connectionIdResult.error() || patternResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required fields: connectionId or pattern");
        }
        auto connectionId = std::string(connectionIdResult.value());
        auto pattern = std::string(patternResult.value());

        auto driver = db.getMetadataDriver(connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        SearchOptions options{};
        if (auto val = doc["searchTables"].get_bool(); !val.error())
            options.searchTables = val.value();
        if (auto val = doc["searchViews"].get_bool(); !val.error())
            options.searchViews = val.value();
        if (auto val = doc["searchProcedures"].get_bool(); !val.error())
            options.searchProcedures = val.value();
        if (auto val = doc["searchFunctions"].get_bool(); !val.error())
            options.searchFunctions = val.value();
        if (auto val = doc["searchColumns"].get_bool(); !val.error())
            options.searchColumns = val.value();
        if (auto val = doc["caseSensitive"].get_bool(); !val.error())
            options.caseSensitive = val.value();
        if (auto val = doc["maxResults"].get_int64(); !val.error())
            options.maxResults = static_cast<int>(val.value());

        auto results = m_globalSearch->searchObjects(driver.get(), pattern, options);

        auto json = JsonUtils::buildArray(results, [](std::string& out, const SearchResult& r) {
            out += std::format(R"({{"objectType":"{}","schemaName":"{}","objectName":"{}","parentName":"{}"}})", JsonUtils::escapeString(r.objectType), JsonUtils::escapeString(r.schemaName),
                               JsonUtils::escapeString(r.objectName), JsonUtils::escapeString(r.parentName));
        });

        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string UtilityContext::handleQuickSearch(IDatabaseContext& db, std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto connectionIdResult = doc["connectionId"].get_string();
        auto prefixResult = doc["prefix"].get_string();
        if (connectionIdResult.error() || prefixResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required fields: connectionId or prefix");
        }
        auto connectionId = std::string(connectionIdResult.value());
        auto prefix = std::string(prefixResult.value());
        int limit = 20;
        if (auto val = doc["limit"].get_int64(); !val.error())
            limit = static_cast<int>(val.value());

        auto driver = db.getMetadataDriver(connectionId);
        if (!driver) [[unlikely]] {
            return JsonUtils::errorResponse(std::format("Connection not found: {}", connectionId));
        }

        auto results = m_globalSearch->quickSearch(driver.get(), prefix, limit);

        auto json = JsonUtils::buildArray(results, [](std::string& out, const std::string& r) { out += std::format(R"("{}")", JsonUtils::escapeString(r)); });

        return JsonUtils::successResponse(json);
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

}  // namespace velocitydb
