#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for connection management capability
class IConnectable {
public:
    virtual ~IConnectable() = default;

    IConnectable(const IConnectable&) = delete;
    IConnectable& operator=(const IConnectable&) = delete;

    /// Establish a connection using the provided connection string
    [[nodiscard]] virtual std::expected<void, std::string> connect(std::string_view connectionString) = 0;

    /// Close the active connection
    virtual void disconnect() = 0;

    /// Check if currently connected
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;

    /// Get the last error message
    [[nodiscard]] virtual std::string_view getLastError() const noexcept = 0;

protected:
    IConnectable() = default;
    IConnectable(IConnectable&&) = default;
    IConnectable& operator=(IConnectable&&) = default;
};

}  // namespace velocitydb
