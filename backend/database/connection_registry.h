#pragma once

#include "../network/ssh_tunnel.h"

#include <atomic>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace velocitydb {

class IDatabaseDriver;

/// Manages active database connections and their associated resources
class ConnectionRegistry {
public:
    using DriverPtr = std::shared_ptr<IDatabaseDriver>;

    ConnectionRegistry() = default;
    ~ConnectionRegistry();

    ConnectionRegistry(const ConnectionRegistry&) = delete;
    ConnectionRegistry& operator=(const ConnectionRegistry&) = delete;
    ConnectionRegistry(ConnectionRegistry&&) = delete;
    ConnectionRegistry& operator=(ConnectionRegistry&&) = delete;

    /// Add a new connection and return its unique ID
    [[nodiscard]] std::string add(DriverPtr driver);

    /// Remove a connection by ID
    void remove(std::string_view id);

    /// Get a connection by ID
    [[nodiscard]] std::expected<DriverPtr, std::string> get(std::string_view id) const;

    /// Check if a connection exists
    [[nodiscard]] bool exists(std::string_view id) const;

    /// Get the number of active connections
    [[nodiscard]] size_t count() const;

    /// Attach an SSH tunnel to a connection
    void attachTunnel(std::string_view connectionId, std::unique_ptr<SshTunnel> tunnel);

    /// Get the SSH tunnel for a connection (may be nullptr)
    [[nodiscard]] SshTunnel* getTunnel(std::string_view connectionId) const;

    /// Remove and close all connections
    void clear();

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, DriverPtr> m_connections;
    std::unordered_map<std::string, std::unique_ptr<SshTunnel>> m_tunnels;
    std::atomic<int> m_counter{1};
};

}  // namespace velocitydb
