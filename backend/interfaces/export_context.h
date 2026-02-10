#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace velocitydb {

class IDatabaseContext;

/// Interface for data export operations
class IExportContext {
public:
    virtual ~IExportContext() = default;

    [[nodiscard]] virtual std::string handleExportCSV(IDatabaseContext& db, std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleExportJSON(IDatabaseContext& db, std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleExportExcel(IDatabaseContext& db, std::string_view params) = 0;
    [[nodiscard]] virtual std::vector<std::string> getSupportedFormats() const = 0;
};

}  // namespace velocitydb
