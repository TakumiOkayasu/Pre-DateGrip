#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace velocitydb {

class ConnectionRegistry;
class IDatabaseDriver;
class ISchemaProvider;
class SchemaInspector;
class AsyncQueryExecutor;
class TransactionManager;
class ResultCache;
class QueryHistory;
struct ResultSet;
struct TableInfo;
struct ColumnInfo;

/// Context for database connection and query operations
class DatabaseContext {
public:
    DatabaseContext();
    ~DatabaseContext();

    DatabaseContext(const DatabaseContext&) = delete;
    DatabaseContext& operator=(const DatabaseContext&) = delete;
    DatabaseContext(DatabaseContext&&) noexcept;
    DatabaseContext& operator=(DatabaseContext&&) noexcept;

    // Connection management
    // Note: connect() and testConnection() are handled by IPCHandler during migration
    void disconnect(std::string_view connectionId);

    // Query execution
    [[nodiscard]] std::expected<ResultSet, std::string> execute(std::string_view connectionId, std::string_view sql);

    [[nodiscard]] std::expected<ResultSet, std::string> executePaginated(std::string_view connectionId, std::string_view sql, int offset, int limit);

    [[nodiscard]] std::expected<int64_t, std::string> getRowCount(std::string_view connectionId, std::string_view tableName);

    void cancelQuery(std::string_view connectionId);

    // Async query operations
    [[nodiscard]] std::expected<std::string, std::string> executeAsync(std::string_view connectionId, std::string_view sql);

    [[nodiscard]] std::expected<std::string, std::string> getAsyncResult(std::string_view queryId);
    void cancelAsyncQuery(std::string_view queryId);
    [[nodiscard]] std::string getActiveQueries();

    // Schema operations
    [[nodiscard]] std::expected<std::vector<std::string>, std::string> getDatabases(std::string_view connectionId);

    [[nodiscard]] std::expected<std::vector<TableInfo>, std::string> getTables(std::string_view connectionId, std::string_view database);

    [[nodiscard]] std::expected<std::vector<ColumnInfo>, std::string> getColumns(std::string_view connectionId, std::string_view tableName);

    [[nodiscard]] std::expected<std::string, std::string> getTableDDL(std::string_view connectionId, std::string_view tableName);

    // Transaction operations
    [[nodiscard]] std::expected<void, std::string> beginTransaction(std::string_view connectionId);
    [[nodiscard]] std::expected<void, std::string> commitTransaction(std::string_view connectionId);
    [[nodiscard]] std::expected<void, std::string> rollbackTransaction(std::string_view connectionId);

    // Cache operations
    [[nodiscard]] std::string getCacheStats();
    void clearCache();

    // Query history
    [[nodiscard]] std::string getQueryHistory();
    void addToHistory(std::string_view sql, bool success, int64_t executionTimeMs);

    // Connection registry access (for migration)
    [[nodiscard]] ConnectionRegistry& getRegistry() { return *m_registry; }
    [[nodiscard]] const ConnectionRegistry& getRegistry() const { return *m_registry; }

private:
    std::unique_ptr<ConnectionRegistry> m_registry;
    std::unique_ptr<SchemaInspector> m_schemaInspector;
    std::unique_ptr<AsyncQueryExecutor> m_asyncExecutor;
    std::unique_ptr<ResultCache> m_resultCache;
    std::unique_ptr<QueryHistory> m_queryHistory;
    std::unordered_map<std::string, std::unique_ptr<TransactionManager>> m_transactionManagers;
};

}  // namespace velocitydb
