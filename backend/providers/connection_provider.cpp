#include "connection_provider.h"

#include "../database/connection_registry.h"
#include "../database/connection_utils.h"
#include "../database/sqlserver_driver.h"
#include "../network/ssh_tunnel.h"
#include "../utils/json_utils.h"
#include "../utils/logger.h"

#include <format>

namespace velocitydb {

namespace {

[[nodiscard]] std::shared_ptr<SQLServerDriver> getDriver(const ConnectionRegistry& registry, std::string_view connectionId) {
    auto driverResult = registry.getQueryDriver(connectionId);
    if (!driverResult)
        return nullptr;
    return std::dynamic_pointer_cast<SQLServerDriver>(*driverResult);
}

[[nodiscard]] std::shared_ptr<SQLServerDriver> getMetaDriver(const ConnectionRegistry& registry, std::string_view connectionId) {
    auto driverResult = registry.getMetadataDriver(connectionId);
    if (!driverResult)
        return nullptr;
    return std::dynamic_pointer_cast<SQLServerDriver>(*driverResult);
}

struct PreparedConnection {
    std::string odbcString;
    std::unique_ptr<SshTunnel> tunnel;
};

/// SSH tunnel確立 + ODBC接続文字列構築の共通処理
[[nodiscard]] std::expected<PreparedConnection, std::string> prepareConnection(const DatabaseConnectionParams& params) {
    DatabaseConnectionParams effectiveParams = params;
    std::unique_ptr<SshTunnel> tunnel;

    if (params.ssh.enabled) {
        auto tunnelResult = establishSshTunnel(params);
        if (!tunnelResult)
            return std::unexpected(tunnelResult.error());
        tunnel = std::move(*tunnelResult);
        effectiveParams.server = std::format("127.0.0.1,{}", tunnel->getLocalPort());
        log<LogLevel::DEBUG>(std::format("[DB] SSH tunnel established, redirecting to: {}", effectiveParams.server));
    }

    auto odbcString = buildODBCConnectionString(effectiveParams);
    log<LogLevel::DEBUG>(std::format("[DB] ODBC connection target: {}", effectiveParams.server));
    log<LogLevel::DEBUG>("[DB] Attempting ODBC connection...");
    log_flush();

    return PreparedConnection{std::move(odbcString), std::move(tunnel)};
}

}  // namespace

ConnectionProvider::ConnectionProvider() : m_registry(std::make_unique<ConnectionRegistry>()) {}

ConnectionProvider::~ConnectionProvider() = default;

std::shared_ptr<SQLServerDriver> ConnectionProvider::getQueryDriver(std::string_view connectionId) {
    return getDriver(*m_registry, connectionId);
}

std::shared_ptr<SQLServerDriver> ConnectionProvider::getMetadataDriver(std::string_view connectionId) {
    return getMetaDriver(*m_registry, connectionId);
}

std::string ConnectionProvider::handleConnect(std::string_view params) {
    auto connectionParams = extractConnectionParams(params);
    if (!connectionParams) {
        return JsonUtils::errorResponse(connectionParams.error());
    }

    auto prepared = prepareConnection(*connectionParams);
    if (!prepared) {
        return JsonUtils::errorResponse(prepared.error());
    }

    auto queryDriverPtr = std::make_shared<SQLServerDriver>();
    if (!queryDriverPtr->connect(prepared->odbcString)) {
        return JsonUtils::errorResponse(std::format("Connection failed: {}", queryDriverPtr->getLastError()));
    }

    auto metadataDriverPtr = std::make_shared<SQLServerDriver>();
    if (!metadataDriverPtr->connect(prepared->odbcString)) {
        queryDriverPtr->disconnect();
        return JsonUtils::errorResponse(std::format("Metadata connection failed: {}", metadataDriverPtr->getLastError()));
    }

    auto connectionId = m_registry->add(queryDriverPtr, metadataDriverPtr);
    if (prepared->tunnel) {
        m_registry->attachTunnel(connectionId, std::move(prepared->tunnel));
    }

    return JsonUtils::successResponse(std::format(R"({{"connectionId":"{}"}})", connectionId));
}

std::string ConnectionProvider::handleDisconnect(std::string_view params) {
    auto connectionIdResult = extractConnectionId(params);
    if (!connectionIdResult) {
        return JsonUtils::errorResponse(connectionIdResult.error());
    }

    m_registry->remove(*connectionIdResult);
    return JsonUtils::successResponse("{}");
}

std::string ConnectionProvider::handleTestConnection(std::string_view params) {
    auto connectionParams = extractConnectionParams(params);
    if (!connectionParams) {
        return JsonUtils::errorResponse(connectionParams.error());
    }

    auto prepared = prepareConnection(*connectionParams);
    if (!prepared) {
        return JsonUtils::successResponse(std::format(R"({{"success":false,"message":"{}"}})", JsonUtils::escapeString(prepared.error())));
    }

    SQLServerDriver driver{};
    if (driver.connect(prepared->odbcString)) {
        driver.disconnect();
        return JsonUtils::successResponse(R"({"success":true,"message":"Connection successful"})");
    }

    return JsonUtils::successResponse(std::format(R"({{"success":false,"message":"{}"}})", JsonUtils::escapeString(driver.getLastError())));
}

}  // namespace velocitydb
