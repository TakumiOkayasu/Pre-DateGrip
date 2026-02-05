#include "connection_registry.h"

#include "../network/ssh_tunnel.h"
#include "driver_interface.h"

#include <format>

namespace velocitydb {

ConnectionRegistry::~ConnectionRegistry() {
    clear();
}

std::string ConnectionRegistry::add(DriverPtr driver) {
    std::lock_guard lock(m_mutex);
    auto id = std::format("conn_{}", m_counter.fetch_add(1));
    m_connections[id] = std::move(driver);
    return id;
}

void ConnectionRegistry::remove(std::string_view id) {
    std::lock_guard lock(m_mutex);
    auto idStr = std::string(id);

    // Close and remove SSH tunnel first
    if (auto tunnelIt = m_tunnels.find(idStr); tunnelIt != m_tunnels.end()) {
        m_tunnels.erase(tunnelIt);
    }

    // Disconnect and remove driver
    if (auto connIt = m_connections.find(idStr); connIt != m_connections.end()) {
        if (connIt->second && connIt->second->isConnected()) {
            connIt->second->disconnect();
        }
        m_connections.erase(connIt);
    }
}

std::expected<ConnectionRegistry::DriverPtr, std::string> ConnectionRegistry::get(std::string_view id) const {
    std::shared_lock lock(m_mutex);

    if (auto it = m_connections.find(std::string(id)); it != m_connections.end()) {
        return it->second;
    }
    return std::unexpected(std::format("Connection '{}' not found", id));
}

bool ConnectionRegistry::exists(std::string_view id) const {
    std::shared_lock lock(m_mutex);
    return m_connections.contains(std::string(id));
}

size_t ConnectionRegistry::count() const {
    std::shared_lock lock(m_mutex);
    return m_connections.size();
}

void ConnectionRegistry::attachTunnel(std::string_view connectionId, std::unique_ptr<SshTunnel> tunnel) {
    std::lock_guard lock(m_mutex);
    m_tunnels[std::string(connectionId)] = std::move(tunnel);
}

SshTunnel* ConnectionRegistry::getTunnel(std::string_view connectionId) const {
    std::shared_lock lock(m_mutex);

    if (auto it = m_tunnels.find(std::string(connectionId)); it != m_tunnels.end()) {
        return it->second.get();
    }
    return nullptr;
}

void ConnectionRegistry::clear() {
    std::lock_guard lock(m_mutex);

    // Close all tunnels
    m_tunnels.clear();

    // Disconnect all connections
    for (auto& [id, driver] : m_connections) {
        if (driver && driver->isConnected()) {
            driver->disconnect();
        }
    }
    m_connections.clear();
}

}  // namespace velocitydb
