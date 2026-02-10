#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class SQLServerDriver;

/// Interface for database connection and query operations
class IDatabaseContext {
public:
    virtual ~IDatabaseContext() = default;

    // Connection lifecycle
    [[nodiscard]] virtual std::string handleConnect(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleDisconnect(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleTestConnection(std::string_view params) = 0;

    // Query execution
    [[nodiscard]] virtual std::string handleExecuteQuery(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleExecuteQueryPaginated(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetRowCount(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleCancelQuery(std::string_view params) = 0;

    // Async queries
    [[nodiscard]] virtual std::string handleExecuteAsyncQuery(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetAsyncQueryResult(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleCancelAsyncQuery(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetActiveQueries(std::string_view params) = 0;

    // Schema
    [[nodiscard]] virtual std::string handleGetDatabases(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTables(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetColumns(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetIndexes(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetConstraints(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetForeignKeys(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetReferencingForeignKeys(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTriggers(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTableMetadata(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTableDDL(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetExecutionPlan(std::string_view params) = 0;

    // Transactions
    [[nodiscard]] virtual std::string handleBeginTransaction(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleCommitTransaction(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleRollbackTransaction(std::string_view params) = 0;

    // Cache & History
    [[nodiscard]] virtual std::string handleGetCacheStats(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleClearCache(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetQueryHistory(std::string_view params) = 0;

    // Filter
    [[nodiscard]] virtual std::string handleFilterResultSet(std::string_view params) = 0;

    // Driver access (for cross-context use: Export, Utility)
    [[nodiscard]] virtual std::shared_ptr<SQLServerDriver> getQueryDriver(std::string_view connectionId) = 0;
    [[nodiscard]] virtual std::shared_ptr<SQLServerDriver> getMetadataDriver(std::string_view connectionId) = 0;
};

}  // namespace velocitydb
