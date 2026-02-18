#pragma once

#include "../interfaces/providers/query_provider.h"

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class IConnectionProvider;
class ResultCache;
class QueryHistory;

/// Provider for query execution, cache, history, and filtering
class QueryProvider : public IQueryProvider {
public:
    explicit QueryProvider(IConnectionProvider& connections);
    ~QueryProvider() override;

    QueryProvider(const QueryProvider&) = delete;
    QueryProvider& operator=(const QueryProvider&) = delete;
    QueryProvider(QueryProvider&&) = delete;
    QueryProvider& operator=(QueryProvider&&) = delete;

    [[nodiscard]] std::string handleExecuteQuery(std::string_view params) override;
    [[nodiscard]] std::string handleExecuteQueryPaginated(std::string_view params) override;
    [[nodiscard]] std::string handleGetRowCount(std::string_view params) override;
    [[nodiscard]] std::string handleCancelQuery(std::string_view params) override;
    [[nodiscard]] std::string handleFilterResultSet(std::string_view params) override;
    [[nodiscard]] std::string handleGetCacheStats(std::string_view params) override;
    [[nodiscard]] std::string handleClearCache(std::string_view params) override;
    [[nodiscard]] std::string handleGetQueryHistory(std::string_view params) override;

private:
    IConnectionProvider& m_connections;
    std::unique_ptr<ResultCache> m_resultCache;
    std::unique_ptr<QueryHistory> m_queryHistory;
};

}  // namespace velocitydb
