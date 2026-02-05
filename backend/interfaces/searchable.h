#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace velocitydb {

/// Search result item
struct SearchResult {
    std::string objectType;
    std::string objectName;
    std::string schemaName;
    std::string databaseName;
    double relevanceScore{0.0};
};

/// Interface for search capability
class ISearchable {
public:
    virtual ~ISearchable() = default;

    ISearchable(const ISearchable&) = delete;
    ISearchable& operator=(const ISearchable&) = delete;

    /// Perform a full search with all filters
    [[nodiscard]] virtual std::vector<SearchResult> search(std::string_view query, std::string_view objectType = "") = 0;

    /// Perform a quick search with limited results
    [[nodiscard]] virtual std::vector<SearchResult> quickSearch(std::string_view query, size_t maxResults = 10) = 0;

protected:
    ISearchable() = default;
    ISearchable(ISearchable&&) = default;
    ISearchable& operator=(ISearchable&&) = default;
};

}  // namespace velocitydb
