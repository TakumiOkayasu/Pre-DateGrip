#pragma once

namespace velocitydb {

class IDatabaseContext;
class IExportContext;
class ISettingsContext;
class IUtilityContext;
class IIOContext;

/// Top-level interface aggregating all context interfaces
class ISystemContext {
public:
    virtual ~ISystemContext() = default;

    [[nodiscard]] virtual IDatabaseContext& database() noexcept = 0;
    [[nodiscard]] virtual IExportContext& export_ctx() noexcept = 0;
    [[nodiscard]] virtual ISettingsContext& settings() noexcept = 0;
    [[nodiscard]] virtual IUtilityContext& utility() noexcept = 0;
    [[nodiscard]] virtual IIOContext& io() noexcept = 0;
};

}  // namespace velocitydb
