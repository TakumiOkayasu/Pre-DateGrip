#include "ipc_handler.h"

#include "interfaces/database_context.h"
#include "interfaces/export_context.h"
#include "interfaces/io_context.h"
#include "interfaces/settings_context.h"
#include "interfaces/system_context.h"
#include "interfaces/utility_context.h"
#include "simdjson.h"
#include "utils/json_utils.h"

#include <format>

namespace velocitydb {

IPCHandler::IPCHandler(ISystemContext& ctx) : m_ctx(ctx) {
    registerRoutes();
}

IPCHandler::~IPCHandler() = default;

void IPCHandler::registerRoutes() {
    // Connection lifecycle
    m_routes["connect"] = [this](auto p) { return m_ctx.database().handleConnect(p); };
    m_routes["disconnect"] = [this](auto p) { return m_ctx.database().handleDisconnect(p); };
    m_routes["testConnection"] = [this](auto p) { return m_ctx.database().handleTestConnection(p); };

    // Query execution
    m_routes["executeQuery"] = [this](auto p) { return m_ctx.database().handleExecuteQuery(p); };
    m_routes["executeQueryPaginated"] = [this](auto p) { return m_ctx.database().handleExecuteQueryPaginated(p); };
    m_routes["getRowCount"] = [this](auto p) { return m_ctx.database().handleGetRowCount(p); };
    m_routes["cancelQuery"] = [this](auto p) { return m_ctx.database().handleCancelQuery(p); };

    // Async queries
    m_routes["executeAsyncQuery"] = [this](auto p) { return m_ctx.database().handleExecuteAsyncQuery(p); };
    m_routes["getAsyncQueryResult"] = [this](auto p) { return m_ctx.database().handleGetAsyncQueryResult(p); };
    m_routes["cancelAsyncQuery"] = [this](auto p) { return m_ctx.database().handleCancelAsyncQuery(p); };
    m_routes["getActiveQueries"] = [this](auto p) { return m_ctx.database().handleGetActiveQueries(p); };
    m_routes["removeAsyncQuery"] = [this](auto p) { return m_ctx.database().handleRemoveAsyncQuery(p); };

    // Schema
    m_routes["getDatabases"] = [this](auto p) { return m_ctx.database().handleGetDatabases(p); };
    m_routes["getTables"] = [this](auto p) { return m_ctx.database().handleGetTables(p); };
    m_routes["getColumns"] = [this](auto p) { return m_ctx.database().handleGetColumns(p); };
    m_routes["getIndexes"] = [this](auto p) { return m_ctx.database().handleGetIndexes(p); };
    m_routes["getConstraints"] = [this](auto p) { return m_ctx.database().handleGetConstraints(p); };
    m_routes["getForeignKeys"] = [this](auto p) { return m_ctx.database().handleGetForeignKeys(p); };
    m_routes["getReferencingForeignKeys"] = [this](auto p) { return m_ctx.database().handleGetReferencingForeignKeys(p); };
    m_routes["getTriggers"] = [this](auto p) { return m_ctx.database().handleGetTriggers(p); };
    m_routes["getTableMetadata"] = [this](auto p) { return m_ctx.database().handleGetTableMetadata(p); };
    m_routes["getTableDDL"] = [this](auto p) { return m_ctx.database().handleGetTableDDL(p); };
    m_routes["getExecutionPlan"] = [this](auto p) { return m_ctx.database().handleGetExecutionPlan(p); };

    // Transactions
    m_routes["beginTransaction"] = [this](auto p) { return m_ctx.database().handleBeginTransaction(p); };
    m_routes["commit"] = [this](auto p) { return m_ctx.database().handleCommitTransaction(p); };
    m_routes["rollback"] = [this](auto p) { return m_ctx.database().handleRollbackTransaction(p); };

    // Cache & History
    m_routes["getCacheStats"] = [this](auto p) { return m_ctx.database().handleGetCacheStats(p); };
    m_routes["clearCache"] = [this](auto p) { return m_ctx.database().handleClearCache(p); };
    m_routes["getQueryHistory"] = [this](auto p) { return m_ctx.database().handleGetQueryHistory(p); };

    // Filter
    m_routes["filterResultSet"] = [this](auto p) { return m_ctx.database().handleFilterResultSet(p); };

    // Export (cross-context: export uses database for driver access)
    m_routes["exportCSV"] = [this](auto p) { return m_ctx.export_ctx().handleExportCSV(m_ctx.database(), p); };
    m_routes["exportJSON"] = [this](auto p) { return m_ctx.export_ctx().handleExportJSON(m_ctx.database(), p); };
    m_routes["exportExcel"] = [this](auto p) { return m_ctx.export_ctx().handleExportExcel(m_ctx.database(), p); };

    // Utility
    m_routes["uppercaseKeywords"] = [this](auto p) { return m_ctx.utility().handleUppercaseKeywords(p); };
    m_routes["parseA5ER"] = [this](auto p) { return m_ctx.utility().handleParseA5ER(p); };
    m_routes["parseA5ERContent"] = [this](auto p) { return m_ctx.utility().handleParseA5ERContent(p); };

    // Search (cross-context: utility uses database for driver access)
    m_routes["searchObjects"] = [this](auto p) { return m_ctx.utility().handleSearchObjects(m_ctx.database(), p); };
    m_routes["quickSearch"] = [this](auto p) { return m_ctx.utility().handleQuickSearch(m_ctx.database(), p); };

    // Settings
    m_routes["getSettings"] = [this](auto) { return m_ctx.settings().handleGetSettings(); };
    m_routes["updateSettings"] = [this](auto p) { return m_ctx.settings().handleUpdateSettings(p); };
    m_routes["getConnectionProfiles"] = [this](auto) { return m_ctx.settings().handleGetConnectionProfiles(); };
    m_routes["saveConnectionProfile"] = [this](auto p) { return m_ctx.settings().handleSaveConnectionProfile(p); };
    m_routes["deleteConnectionProfile"] = [this](auto p) { return m_ctx.settings().handleDeleteConnectionProfile(p); };
    m_routes["getProfilePassword"] = [this](auto p) { return m_ctx.settings().handleGetProfilePassword(p); };
    m_routes["getSshPassword"] = [this](auto p) { return m_ctx.settings().handleGetSshPassword(p); };
    m_routes["getSshKeyPassphrase"] = [this](auto p) { return m_ctx.settings().handleGetSshKeyPassphrase(p); };
    m_routes["getSessionState"] = [this](auto) { return m_ctx.settings().handleGetSessionState(); };
    m_routes["saveSessionState"] = [this](auto p) { return m_ctx.settings().handleSaveSessionState(p); };

    // IO
    m_routes["writeFrontendLog"] = [this](auto p) { return m_ctx.io().handleWriteFrontendLog(p); };
    m_routes["saveQueryToFile"] = [this](auto p) { return m_ctx.io().handleSaveQueryToFile(p); };
    m_routes["loadQueryFromFile"] = [this](auto p) { return m_ctx.io().handleLoadQueryFromFile(p); };
    m_routes["browseFile"] = [this](auto p) { return m_ctx.io().handleBrowseFile(p); };
    m_routes["getBookmarks"] = [this](auto p) { return m_ctx.io().handleGetBookmarks(p); };
    m_routes["saveBookmark"] = [this](auto p) { return m_ctx.io().handleSaveBookmark(p); };
    m_routes["deleteBookmark"] = [this](auto p) { return m_ctx.io().handleDeleteBookmark(p); };
}

std::string IPCHandler::dispatchRequest(std::string_view request) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(request);

        auto methodResult = doc["method"].get_string();
        if (methodResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing method field");
        }
        auto method = methodResult.value();

        std::string params;
        if (auto paramsResult = doc["params"].get_string(); !paramsResult.error()) {
            params = std::string(paramsResult.value());
        }

        if (auto route = m_routes.find(std::string(method)); route != m_routes.end()) [[likely]] {
            return route->second(params);
        }

        return JsonUtils::errorResponse(std::format("Unknown method: {}", method));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

}  // namespace velocitydb
