#include "utility_provider.h"

#include "../parsers/a5er_parser.h"
#include "../parsers/sql_formatter.h"
#include "../utils/json_utils.h"
#include "simdjson.h"

#include <format>

namespace velocitydb {

namespace {

std::string serializeA5ERModelToJson(const A5ERModel& model, const std::string& ddl = "") {
    auto tablesJson = JsonUtils::buildArray(model.tables, [](std::string& out, const auto& table) {
        auto columnsJson = JsonUtils::buildArray(table.columns, [](std::string& out, const auto& col) {
            out += std::format(R"({{"name":"{}","logicalName":"{}","type":"{}","size":{},"scale":{},"nullable":{},"isPrimaryKey":{},"defaultValue":"{}","comment":"{}"}})",
                               JsonUtils::escapeString(col.name), JsonUtils::escapeString(col.logicalName), JsonUtils::escapeString(col.type), col.size, col.scale, col.nullable ? "true" : "false",
                               col.isPrimaryKey ? "true" : "false", JsonUtils::escapeString(col.defaultValue), JsonUtils::escapeString(col.comment));
        });
        auto indexesJson = JsonUtils::buildArray(table.indexes, [](std::string& out, const auto& idx) {
            auto idxColumnsJson = JsonUtils::buildArray(idx.columns, [](std::string& out, const auto& colName) { out += std::format(R"("{}")", JsonUtils::escapeString(colName)); });
            out += std::format(R"({{"name":"{}","columns":{},"isUnique":{}}})", JsonUtils::escapeString(idx.name), idxColumnsJson, idx.isUnique ? "true" : "false");
        });
        out += std::format(R"({{"name":"{}","logicalName":"{}","comment":"{}","page":"{}","columns":{},"indexes":{},"posX":{},"posY":{}}})", JsonUtils::escapeString(table.name),
                           JsonUtils::escapeString(table.logicalName), JsonUtils::escapeString(table.comment), JsonUtils::escapeString(table.page), columnsJson, indexesJson, table.posX, table.posY);
    });

    auto relationsJson = JsonUtils::buildArray(model.relations, [](std::string& out, const auto& rel) {
        out += std::format(R"({{"name":"{}","parentTable":"{}","childTable":"{}","parentColumn":"{}","childColumn":"{}","cardinality":"{}"}})", JsonUtils::escapeString(rel.name),
                           JsonUtils::escapeString(rel.parentTable), JsonUtils::escapeString(rel.childTable), JsonUtils::escapeString(rel.parentColumn), JsonUtils::escapeString(rel.childColumn),
                           JsonUtils::escapeString(rel.cardinality));
    });

    return std::format(R"({{"name":"{}","databaseType":"{}","tables":{},"relations":{},"ddl":"{}"}})", JsonUtils::escapeString(model.name), JsonUtils::escapeString(model.databaseType), tablesJson,
                       relationsJson, JsonUtils::escapeString(ddl));
}

}  // namespace

UtilityProvider::UtilityProvider() : m_sqlFormatter(std::make_unique<SQLFormatter>()), m_a5erParser(std::make_unique<A5ERParser>()) {}

UtilityProvider::~UtilityProvider() = default;
UtilityProvider::UtilityProvider(UtilityProvider&&) noexcept = default;
UtilityProvider& UtilityProvider::operator=(UtilityProvider&&) noexcept = default;

std::string UtilityProvider::handleUppercaseKeywords(std::string_view params) {
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

std::string UtilityProvider::handleParseA5ER(std::string_view params) {
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

std::string UtilityProvider::handleParseA5ERContent(std::string_view params) {
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

}  // namespace velocitydb
