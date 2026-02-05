#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for persistent storage capability
class IStorable {
public:
    virtual ~IStorable() = default;

    IStorable(const IStorable&) = delete;
    IStorable& operator=(const IStorable&) = delete;

    /// Save data to storage (data is JSON string)
    [[nodiscard]] virtual std::expected<void, std::string> save(std::string_view key, std::string_view data) = 0;

    /// Load data from storage (returns JSON string)
    [[nodiscard]] virtual std::expected<std::string, std::string> load(std::string_view key) = 0;

    /// Delete data from storage
    [[nodiscard]] virtual std::expected<void, std::string> remove(std::string_view key) = 0;

    /// Check if a key exists in storage
    [[nodiscard]] virtual bool exists(std::string_view key) const = 0;

protected:
    IStorable() = default;
    IStorable(IStorable&&) = default;
    IStorable& operator=(IStorable&&) = default;
};

}  // namespace velocitydb
