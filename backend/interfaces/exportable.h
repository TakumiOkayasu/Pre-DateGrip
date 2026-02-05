#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace velocitydb {

struct ResultSet;

/// Interface for data export capability
class IExportable {
public:
    virtual ~IExportable() = default;

    IExportable(const IExportable&) = delete;
    IExportable& operator=(const IExportable&) = delete;

    /// Export result set to the specified file path
    [[nodiscard]] virtual std::expected<void, std::string> exportTo(const ResultSet& data, std::string_view filePath) = 0;

    /// Get the supported file extension (e.g., "csv", "json", "xlsx")
    [[nodiscard]] virtual std::string_view getExtension() const noexcept = 0;

    /// Get the format name for display
    [[nodiscard]] virtual std::string_view getFormatName() const noexcept = 0;

protected:
    IExportable() = default;
    IExportable(IExportable&&) = default;
    IExportable& operator=(IExportable&&) = default;
};

}  // namespace velocitydb
