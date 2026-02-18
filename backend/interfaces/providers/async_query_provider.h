#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for asynchronous query execution
class IAsyncQueryProvider {
public:
    virtual ~IAsyncQueryProvider() = default;

    [[nodiscard]] virtual std::string handleExecuteAsyncQuery(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetAsyncQueryResult(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleCancelAsyncQuery(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetActiveQueries(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleRemoveAsyncQuery(std::string_view params) = 0;
};

}  // namespace velocitydb
