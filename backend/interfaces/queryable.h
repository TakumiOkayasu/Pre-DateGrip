#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace velocitydb {

struct ResultSet;

/// Interface for query execution capability
class IQueryable {
public:
    virtual ~IQueryable() = default;

    // Non-copyable (stateful resource), but movable for ownership transfer
    IQueryable(const IQueryable&) = delete;
    IQueryable& operator=(const IQueryable&) = delete;

    /// Execute a SQL query and return results
    [[nodiscard]] virtual std::expected<ResultSet, std::string> execute(std::string_view sql) = 0;

    /// Cancel the currently running query
    virtual void cancel() = 0;

    /// Check if a query is currently executing
    [[nodiscard]] virtual bool isExecuting() const noexcept = 0;

protected:
    IQueryable() = default;
    IQueryable(IQueryable&&) = default;
    IQueryable& operator=(IQueryable&&) = default;
};

}  // namespace velocitydb
