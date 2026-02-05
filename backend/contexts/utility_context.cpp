#include "utility_context.h"

#include "../database/sqlserver_driver.h"
#include "../parsers/a5er_parser.h"
#include "../parsers/sql_formatter.h"
#include "../utils/global_search.h"
#include "../utils/simd_filter.h"

#include <format>

namespace velocitydb {

UtilityContext::UtilityContext()
    : m_sqlFormatter(std::make_unique<SQLFormatter>()), m_globalSearch(std::make_unique<GlobalSearch>()), m_a5erParser(std::make_unique<A5ERParser>()), m_simdFilter(std::make_unique<SIMDFilter>()) {}

UtilityContext::~UtilityContext() = default;
UtilityContext::UtilityContext(UtilityContext&&) noexcept = default;
UtilityContext& UtilityContext::operator=(UtilityContext&&) noexcept = default;

std::string UtilityContext::formatSQL(std::string_view sql) {
    return m_sqlFormatter->format(std::string(sql));
}

std::string UtilityContext::uppercaseKeywords(std::string_view sql) {
    return m_sqlFormatter->uppercaseKeywords(std::string(sql));
}

std::string UtilityContext::searchObjects(std::string_view connectionId, std::string_view query, std::string_view objectType) {
    // GlobalSearch needs driver - implementation deferred to IPCHandler migration
    return "[]";
}

std::string UtilityContext::quickSearch(std::string_view connectionId, std::string_view query, size_t maxResults) {
    return "[]";
}

std::expected<std::string, std::string> UtilityContext::parseA5ERFile(std::string_view filePath) {
    try {
        auto model = m_a5erParser->parse(std::string(filePath));
        // Simplified JSON serialization
        std::string json = std::format(R"({{"name":"{}","databaseType":"{}","tableCount":{}}})", model.name, model.databaseType, model.tables.size());
        return json;
    } catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<ResultSet, std::string> UtilityContext::filterResultSet(const ResultSet& data, std::string_view filterText, const std::vector<int>& columnIndices) {
    // Use SIMDFilter to filter rows containing the filter text
    ResultSet filtered;
    filtered.columns = data.columns;

    // For each specified column, find rows containing the filter text
    std::vector<size_t> matchingRows;
    for (int colIdx : columnIndices) {
        if (colIdx >= 0 && colIdx < static_cast<int>(data.columns.size())) {
            auto matches = m_simdFilter->filterContains(data, static_cast<size_t>(colIdx), std::string(filterText));
            matchingRows.insert(matchingRows.end(), matches.begin(), matches.end());
        }
    }

    // Remove duplicates and add matching rows to result
    std::ranges::sort(matchingRows);
    auto last = std::unique(matchingRows.begin(), matchingRows.end());
    matchingRows.erase(last, matchingRows.end());

    for (size_t rowIdx : matchingRows) {
        if (rowIdx < data.rows.size()) {
            filtered.rows.push_back(data.rows[rowIdx]);
        }
    }

    return filtered;
}

}  // namespace velocitydb
