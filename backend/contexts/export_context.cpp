#include "export_context.h"

#include "../database/sqlserver_driver.h"
#include "../interfaces/exportable.h"

#include <algorithm>
#include <format>

namespace velocitydb {

ExportContext::ExportContext() = default;
ExportContext::~ExportContext() = default;
ExportContext::ExportContext(ExportContext&&) noexcept = default;
ExportContext& ExportContext::operator=(ExportContext&&) noexcept = default;

void ExportContext::registerExporter(std::string_view format, std::unique_ptr<IExportable> exporter) {
    m_exporters[std::string(format)] = std::move(exporter);
}

std::expected<void, std::string> ExportContext::exportTo(std::string_view format, const ResultSet& data, std::string_view filePath) {
    auto formatStr = std::string(format);
    std::ranges::transform(formatStr, formatStr.begin(), ::tolower);

    auto it = m_exporters.find(formatStr);
    if (it == m_exporters.end()) {
        return std::unexpected(std::format("Unsupported export format: {}", format));
    }

    return it->second->exportTo(data, filePath);
}

std::vector<std::string> ExportContext::getSupportedFormats() const {
    std::vector<std::string> formats;
    formats.reserve(m_exporters.size());
    for (const auto& [format, _] : m_exporters) {
        formats.push_back(format);
    }
    return formats;
}

bool ExportContext::isFormatSupported(std::string_view format) const {
    auto formatStr = std::string(format);
    std::ranges::transform(formatStr, formatStr.begin(), ::tolower);
    return m_exporters.contains(formatStr);
}

}  // namespace velocitydb
