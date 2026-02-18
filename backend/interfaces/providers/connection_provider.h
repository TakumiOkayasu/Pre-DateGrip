#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class SQLServerDriver;

/// Interface for database connection lifecycle and driver access
class IConnectionProvider {
public:
    virtual ~IConnectionProvider() = default;

    [[nodiscard]] virtual std::string handleConnect(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleDisconnect(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleTestConnection(std::string_view params) = 0;

    [[nodiscard]] virtual std::shared_ptr<SQLServerDriver> getQueryDriver(std::string_view connectionId) = 0;
    [[nodiscard]] virtual std::shared_ptr<SQLServerDriver> getMetadataDriver(std::string_view connectionId) = 0;
};

}  // namespace velocitydb
