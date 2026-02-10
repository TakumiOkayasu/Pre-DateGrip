#pragma once

#include "../interfaces/export_context.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace velocitydb {

class IExportable;
struct ResultSet;

/// Context for data export operations
class ExportContext : public IExportContext {
public:
    ExportContext();
    ~ExportContext() override;

    ExportContext(const ExportContext&) = delete;
    ExportContext& operator=(const ExportContext&) = delete;
    ExportContext(ExportContext&&) noexcept;
    ExportContext& operator=(ExportContext&&) noexcept;

    // IExportContext implementation
    [[nodiscard]] std::string handleExportCSV(IDatabaseContext& db, std::string_view params) override;
    [[nodiscard]] std::string handleExportJSON(IDatabaseContext& db, std::string_view params) override;
    [[nodiscard]] std::string handleExportExcel(IDatabaseContext& db, std::string_view params) override;
    [[nodiscard]] std::vector<std::string> getSupportedFormats() const override;

private:
    // Internal helper: execute query via IDatabaseContext driver and export
    [[nodiscard]] std::string exportWithDriver(IDatabaseContext& db, std::string_view params, std::string_view format);
};

}  // namespace velocitydb
