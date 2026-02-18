#pragma once

#include "../interfaces/providers/async_query_provider.h"

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class IConnectionProvider;
class AsyncQueryExecutor;

/// Provider for asynchronous query execution
class AsyncQueryProvider : public IAsyncQueryProvider {
public:
    explicit AsyncQueryProvider(IConnectionProvider& connections);
    ~AsyncQueryProvider() override;

    AsyncQueryProvider(const AsyncQueryProvider&) = delete;
    AsyncQueryProvider& operator=(const AsyncQueryProvider&) = delete;
    AsyncQueryProvider(AsyncQueryProvider&&) = delete;
    AsyncQueryProvider& operator=(AsyncQueryProvider&&) = delete;

    [[nodiscard]] std::string handleExecuteAsyncQuery(std::string_view params) override;
    [[nodiscard]] std::string handleGetAsyncQueryResult(std::string_view params) override;
    [[nodiscard]] std::string handleCancelAsyncQuery(std::string_view params) override;
    [[nodiscard]] std::string handleGetActiveQueries(std::string_view params) override;
    [[nodiscard]] std::string handleRemoveAsyncQuery(std::string_view params) override;

private:
    IConnectionProvider& m_connections;
    std::unique_ptr<AsyncQueryExecutor> m_asyncExecutor;
};

}  // namespace velocitydb
