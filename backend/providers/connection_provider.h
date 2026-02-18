#pragma once

#include "../interfaces/providers/connection_provider.h"

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class ConnectionRegistry;

/// Provider for database connection lifecycle and driver access
class ConnectionProvider : public IConnectionProvider {
public:
    ConnectionProvider();
    ~ConnectionProvider() override;

    ConnectionProvider(const ConnectionProvider&) = delete;
    ConnectionProvider& operator=(const ConnectionProvider&) = delete;
    ConnectionProvider(ConnectionProvider&&) = delete;
    ConnectionProvider& operator=(ConnectionProvider&&) = delete;

    [[nodiscard]] std::string handleConnect(std::string_view params) override;
    [[nodiscard]] std::string handleDisconnect(std::string_view params) override;
    [[nodiscard]] std::string handleTestConnection(std::string_view params) override;

    [[nodiscard]] std::shared_ptr<SQLServerDriver> getQueryDriver(std::string_view connectionId) override;
    [[nodiscard]] std::shared_ptr<SQLServerDriver> getMetadataDriver(std::string_view connectionId) override;

private:
    std::unique_ptr<ConnectionRegistry> m_registry;
};

}  // namespace velocitydb
