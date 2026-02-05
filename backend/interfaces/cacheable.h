#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace velocitydb {

/// Cache statistics
struct CacheStats {
    size_t hitCount{0};
    size_t missCount{0};
    size_t entryCount{0};
    size_t memoryUsageBytes{0};
    size_t maxMemoryBytes{0};
};

/// Interface for caching capability
template <typename T>
class ICacheable {
public:
    virtual ~ICacheable() = default;

    ICacheable(const ICacheable&) = delete;
    ICacheable& operator=(const ICacheable&) = delete;

    /// Get a cached value by key
    [[nodiscard]] virtual std::optional<T> get(std::string_view key) = 0;

    /// Store a value in the cache
    virtual void put(std::string_view key, T value) = 0;

    /// Remove a specific entry from the cache
    virtual void remove(std::string_view key) = 0;

    /// Clear all cached entries
    virtual void clear() = 0;

    /// Get cache statistics
    [[nodiscard]] virtual CacheStats getStats() const noexcept = 0;

protected:
    ICacheable() = default;
    ICacheable(ICacheable&&) = default;
    ICacheable& operator=(ICacheable&&) = default;
};

}  // namespace velocitydb
