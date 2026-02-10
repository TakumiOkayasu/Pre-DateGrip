#pragma once

#include "../interfaces/database_context.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace velocitydb {

class ConnectionRegistry;
class SchemaInspector;
class AsyncQueryExecutor;
class TransactionManager;
class ResultCache;
class QueryHistory;
struct ResultSet;
struct TableInfo;
struct ColumnInfo;

/// Context for database connection and query operations
class DatabaseContext : public IDatabaseContext {
public:
    DatabaseContext();
    ~DatabaseContext() override;

    DatabaseContext(const DatabaseContext&) = delete;
    DatabaseContext& operator=(const DatabaseContext&) = delete;
    DatabaseContext(DatabaseContext&&) noexcept;
    DatabaseContext& operator=(DatabaseContext&&) noexcept;

    // IDatabaseContext implementation (IPC handle methods)
    // Connection lifecycle
    [[nodiscard]] std::string handleConnect(std::string_view params) override;
    [[nodiscard]] std::string handleDisconnect(std::string_view params) override;
    [[nodiscard]] std::string handleTestConnection(std::string_view params) override;

    // Query execution
    [[nodiscard]] std::string handleExecuteQuery(std::string_view params) override;
    [[nodiscard]] std::string handleExecuteQueryPaginated(std::string_view params) override;
    [[nodiscard]] std::string handleGetRowCount(std::string_view params) override;
    [[nodiscard]] std::string handleCancelQuery(std::string_view params) override;

    // Async queries
    [[nodiscard]] std::string handleExecuteAsyncQuery(std::string_view params) override;
    [[nodiscard]] std::string handleGetAsyncQueryResult(std::string_view params) override;
    [[nodiscard]] std::string handleCancelAsyncQuery(std::string_view params) override;
    [[nodiscard]] std::string handleGetActiveQueries(std::string_view params) override;

    // Schema
    [[nodiscard]] std::string handleGetDatabases(std::string_view params) override;
    [[nodiscard]] std::string handleGetTables(std::string_view params) override;
    [[nodiscard]] std::string handleGetColumns(std::string_view params) override;
    [[nodiscard]] std::string handleGetIndexes(std::string_view params) override;
    [[nodiscard]] std::string handleGetConstraints(std::string_view params) override;
    [[nodiscard]] std::string handleGetForeignKeys(std::string_view params) override;
    [[nodiscard]] std::string handleGetReferencingForeignKeys(std::string_view params) override;
    [[nodiscard]] std::string handleGetTriggers(std::string_view params) override;
    [[nodiscard]] std::string handleGetTableMetadata(std::string_view params) override;
    [[nodiscard]] std::string handleGetTableDDL(std::string_view params) override;
    [[nodiscard]] std::string handleGetExecutionPlan(std::string_view params) override;

    // Transactions
    [[nodiscard]] std::string handleBeginTransaction(std::string_view params) override;
    [[nodiscard]] std::string handleCommitTransaction(std::string_view params) override;
    [[nodiscard]] std::string handleRollbackTransaction(std::string_view params) override;

    // Cache & History
    [[nodiscard]] std::string handleGetCacheStats(std::string_view params) override;
    [[nodiscard]] std::string handleClearCache(std::string_view params) override;
    [[nodiscard]] std::string handleGetQueryHistory(std::string_view params) override;

    // Filter
    [[nodiscard]] std::string handleFilterResultSet(std::string_view params) override;

    // Driver access (for cross-context use)
    [[nodiscard]] std::shared_ptr<SQLServerDriver> getQueryDriver(std::string_view connectionId) override;
    [[nodiscard]] std::shared_ptr<SQLServerDriver> getMetadataDriver(std::string_view connectionId) override;

private:
    std::unique_ptr<ConnectionRegistry> m_registry;
    std::unique_ptr<SchemaInspector> m_schemaInspector;
    std::unique_ptr<AsyncQueryExecutor> m_asyncExecutor;
    std::unique_ptr<ResultCache> m_resultCache;
    std::unique_ptr<QueryHistory> m_queryHistory;
    std::mutex m_txMutex;
    std::unordered_map<std::string, std::unique_ptr<TransactionManager>> m_transactionManagers;
};

}  // namespace velocitydb
