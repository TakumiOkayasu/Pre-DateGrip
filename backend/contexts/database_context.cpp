#include "database_context.h"

#include "../database/async_query_executor.h"
#include "../database/connection_registry.h"
#include "../database/driver_interface.h"
#include "../database/query_history.h"
#include "../database/result_cache.h"
#include "../database/schema_inspector.h"
#include "../database/sqlserver_driver.h"
#include "../database/transaction_manager.h"
#include "../utils/json_utils.h"

#include <format>
#include <stdexcept>

namespace velocitydb {

namespace {

/// Sanitize a single SQL Server identifier part (no dots).
/// Wraps in brackets and escapes any existing brackets.
[[nodiscard]] std::string sanitizeIdentifierPart(std::string_view part) {
    if (part.empty()) {
        return {};
    }

    std::string result;
    result.reserve(part.size() + 2);
    result += '[';
    for (char c : part) {
        if (c == ']') {
            result += "]]";  // Escape closing bracket
        } else {
            result += c;
        }
    }
    result += ']';
    return result;
}

/// Sanitize SQL Server identifier (table/column name) to prevent SQL injection.
/// Handles schema-qualified names like "dbo.users" -> "[dbo].[users]"
[[nodiscard]] std::string sanitizeIdentifier(std::string_view identifier) {
    if (identifier.empty()) {
        return {};
    }

    std::string result;
    result.reserve(identifier.size() + 4);

    size_t start = 0;
    size_t pos = identifier.find('.');

    while (pos != std::string_view::npos) {
        auto part = sanitizeIdentifierPart(identifier.substr(start, pos - start));
        if (!part.empty()) {
            if (!result.empty()) {
                result += '.';
            }
            result += part;
        }
        start = pos + 1;
        pos = identifier.find('.', start);
    }

    // Handle last (or only) part
    auto lastPart = sanitizeIdentifierPart(identifier.substr(start));
    if (!lastPart.empty()) {
        if (!result.empty()) {
            result += '.';
        }
        result += lastPart;
    }

    return result;
}

/// Helper to get SQLServerDriver from connection registry
[[nodiscard]] std::expected<std::shared_ptr<SQLServerDriver>, std::string> getDriver(const ConnectionRegistry& registry, std::string_view connectionId) {
    auto driverResult = registry.get(connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    auto driver = std::dynamic_pointer_cast<SQLServerDriver>(*driverResult);
    if (!driver) {
        return std::unexpected("Invalid driver type");
    }
    return driver;
}

}  // namespace

DatabaseContext::DatabaseContext()
    : m_registry(std::make_unique<ConnectionRegistry>())
    , m_schemaInspector(std::make_unique<SchemaInspector>())
    , m_asyncExecutor(std::make_unique<AsyncQueryExecutor>())
    , m_resultCache(std::make_unique<ResultCache>())
    , m_queryHistory(std::make_unique<QueryHistory>()) {}

DatabaseContext::~DatabaseContext() = default;
DatabaseContext::DatabaseContext(DatabaseContext&&) noexcept = default;
DatabaseContext& DatabaseContext::operator=(DatabaseContext&&) noexcept = default;

void DatabaseContext::disconnect(std::string_view connectionId) {
    m_transactionManagers.erase(std::string(connectionId));
    m_registry->remove(connectionId);
}

std::expected<ResultSet, std::string> DatabaseContext::execute(std::string_view connectionId, std::string_view sql) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    auto result = (*driverResult)->execute(sql);

    // Add to history
    HistoryItem item;
    item.sql = std::string(sql);
    item.connectionId = std::string(connectionId);
    item.success = true;
    m_queryHistory->add(item);

    return result;
}

std::expected<ResultSet, std::string> DatabaseContext::executePaginated(std::string_view connectionId, std::string_view sql, int offset, int limit) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    // Validate pagination parameters
    if (offset < 0 || limit <= 0) {
        return std::unexpected("Invalid pagination parameters");
    }

    // Note: The sql parameter comes from trusted internal sources (user's query editor).
    // Pagination wrapping is safe as it doesn't interpolate user data into the query structure.
    auto paginatedSql = std::format("SELECT * FROM ({}) AS __paginated_query OFFSET {} ROWS FETCH NEXT {} ROWS ONLY", sql, offset, limit);

    return (*driverResult)->execute(paginatedSql);
}

std::expected<int64_t, std::string> DatabaseContext::getRowCount(std::string_view connectionId, std::string_view tableName) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    // Sanitize table name to prevent SQL injection
    auto countSql = std::format("SELECT COUNT(*) FROM {}", sanitizeIdentifier(tableName));
    auto result = (*driverResult)->execute(countSql);

    if (result.rows.empty() || result.rows[0].values.empty()) {
        return std::unexpected("Failed to get row count");
    }

    try {
        return std::stoll(result.rows[0].values[0]);
    } catch (const std::invalid_argument&) {
        return std::unexpected("Invalid row count result: not a number");
    } catch (const std::out_of_range&) {
        return std::unexpected("Invalid row count result: value out of range");
    }
}

void DatabaseContext::cancelQuery(std::string_view connectionId) {
    if (auto driverResult = getDriver(*m_registry, connectionId)) {
        (*driverResult)->cancel();
    }
}

std::expected<std::string, std::string> DatabaseContext::executeAsync(std::string_view connectionId, std::string_view sql) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    return m_asyncExecutor->submitQuery(*driverResult, sql);
}

std::expected<std::string, std::string> DatabaseContext::getAsyncResult(std::string_view queryId) {
    auto result = m_asyncExecutor->getQueryResult(queryId);
    // Convert to JSON string - simplified for now
    return std::format(R"({{"status":"{}"}})", static_cast<int>(result.status));
}

void DatabaseContext::cancelAsyncQuery(std::string_view queryId) {
    m_asyncExecutor->cancelQuery(queryId);
}

std::string DatabaseContext::getActiveQueries() {
    auto ids = m_asyncExecutor->getActiveQueryIds();
    std::string json = "[";
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0)
            json += ",";
        json += std::format(R"("{}")", ids[i]);
    }
    json += "]";
    return json;
}

std::expected<std::vector<std::string>, std::string> DatabaseContext::getDatabases(std::string_view connectionId) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    m_schemaInspector->setDriver(*driverResult);
    return m_schemaInspector->getDatabases();
}

std::expected<std::vector<TableInfo>, std::string> DatabaseContext::getTables(std::string_view connectionId, std::string_view database) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    m_schemaInspector->setDriver(*driverResult);
    return m_schemaInspector->getTables(database);
}

std::expected<std::vector<ColumnInfo>, std::string> DatabaseContext::getColumns(std::string_view connectionId, std::string_view tableName) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    m_schemaInspector->setDriver(*driverResult);
    return m_schemaInspector->getColumns(tableName);
}

std::expected<std::string, std::string> DatabaseContext::getTableDDL(std::string_view connectionId, std::string_view tableName) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    m_schemaInspector->setDriver(*driverResult);
    return m_schemaInspector->generateDDL(tableName);
}

std::expected<void, std::string> DatabaseContext::beginTransaction(std::string_view connectionId) {
    auto driverResult = getDriver(*m_registry, connectionId);
    if (!driverResult) {
        return std::unexpected(driverResult.error());
    }

    auto connId = std::string(connectionId);
    if (!m_transactionManagers.contains(connId)) {
        m_transactionManagers[connId] = std::make_unique<TransactionManager>();
        m_transactionManagers[connId]->setDriver(*driverResult);
    }

    m_transactionManagers[connId]->begin();
    return {};
}

std::expected<void, std::string> DatabaseContext::commitTransaction(std::string_view connectionId) {
    auto connId = std::string(connectionId);
    auto it = m_transactionManagers.find(connId);
    if (it == m_transactionManagers.end()) {
        return std::unexpected("No active transaction");
    }

    it->second->commit();
    return {};
}

std::expected<void, std::string> DatabaseContext::rollbackTransaction(std::string_view connectionId) {
    auto connId = std::string(connectionId);
    auto it = m_transactionManagers.find(connId);
    if (it == m_transactionManagers.end()) {
        return std::unexpected("No active transaction");
    }

    it->second->rollback();
    return {};
}

std::string DatabaseContext::getCacheStats() {
    return std::format(R"({{"currentSize":{},"maxSize":{}}})", m_resultCache->getCurrentSize(), m_resultCache->getMaxSize());
}

void DatabaseContext::clearCache() {
    m_resultCache->clear();
}

std::string DatabaseContext::getQueryHistory() {
    auto items = m_queryHistory->getAll();
    // Simplified JSON conversion
    std::string json = "[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0)
            json += ",";
        json += std::format(R"({{"id":"{}","sql":"{}","success":{}}})", items[i].id, JsonUtils::escapeString(items[i].sql), items[i].success ? "true" : "false");
    }
    json += "]";
    return json;
}

void DatabaseContext::addToHistory(std::string_view sql, bool success, int64_t executionTimeMs) {
    HistoryItem item;
    item.sql = std::string(sql);
    item.success = success;
    item.executionTimeMs = static_cast<double>(executionTimeMs);
    m_queryHistory->add(item);
}

}  // namespace velocitydb
