#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace velocitydb {

class SQLFormatter;
class GlobalSearch;
class A5ERParser;
class SIMDFilter;
struct ResultSet;

/// Context for utility operations (formatting, search, parsing, filtering)
class UtilityContext {
public:
    UtilityContext();
    ~UtilityContext();

    UtilityContext(const UtilityContext&) = delete;
    UtilityContext& operator=(const UtilityContext&) = delete;
    UtilityContext(UtilityContext&&) noexcept;
    UtilityContext& operator=(UtilityContext&&) noexcept;

    // SQL formatting
    [[nodiscard]] std::string formatSQL(std::string_view sql);
    [[nodiscard]] std::string uppercaseKeywords(std::string_view sql);

    // Global search
    [[nodiscard]] std::string searchObjects(std::string_view connectionId, std::string_view query, std::string_view objectType = "");

    [[nodiscard]] std::string quickSearch(std::string_view connectionId, std::string_view query, size_t maxResults = 10);

    // A5:SQL parser
    [[nodiscard]] std::expected<std::string, std::string> parseA5ERFile(std::string_view filePath);

    // SIMD filtering
    [[nodiscard]] std::expected<ResultSet, std::string> filterResultSet(const ResultSet& data, std::string_view filterText, const std::vector<int>& columnIndices);

private:
    std::unique_ptr<SQLFormatter> m_sqlFormatter;
    std::unique_ptr<GlobalSearch> m_globalSearch;
    std::unique_ptr<A5ERParser> m_a5erParser;
    std::unique_ptr<SIMDFilter> m_simdFilter;
};

}  // namespace velocitydb
