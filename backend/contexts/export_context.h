#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace velocitydb {

class IExportable;
struct ResultSet;

/// Context for data export operations
class ExportContext {
public:
    ExportContext();
    ~ExportContext();

    ExportContext(const ExportContext&) = delete;
    ExportContext& operator=(const ExportContext&) = delete;
    ExportContext(ExportContext&&) noexcept;
    ExportContext& operator=(ExportContext&&) noexcept;

    /// Register an exporter for a specific format
    void registerExporter(std::string_view format, std::unique_ptr<IExportable> exporter);

    /// Export data to the specified format and path
    [[nodiscard]] std::expected<void, std::string> exportTo(std::string_view format, const ResultSet& data, std::string_view filePath);

    /// Get list of supported export formats
    [[nodiscard]] std::vector<std::string> getSupportedFormats() const;

    /// Check if a format is supported
    [[nodiscard]] bool isFormatSupported(std::string_view format) const;

private:
    std::unordered_map<std::string, std::unique_ptr<IExportable>> m_exporters;
};

}  // namespace velocitydb
