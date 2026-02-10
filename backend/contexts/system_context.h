#pragma once

#include "../interfaces/database_context.h"
#include "../interfaces/export_context.h"
#include "../interfaces/io_context.h"
#include "../interfaces/settings_context.h"
#include "../interfaces/system_context.h"
#include "../interfaces/utility_context.h"

#include <memory>

namespace velocitydb {

/// Concrete implementation of ISystemContext, owning all context instances.
/// Contexts are injected as they are migrated to implement their interfaces.
class SystemContext : public ISystemContext {
public:
    SystemContext();
    ~SystemContext() override;

    SystemContext(const SystemContext&) = delete;
    SystemContext& operator=(const SystemContext&) = delete;
    SystemContext(SystemContext&&) = delete;
    SystemContext& operator=(SystemContext&&) = delete;

    [[nodiscard]] IDatabaseContext& database() noexcept override;
    [[nodiscard]] IExportContext& export_ctx() noexcept override;
    [[nodiscard]] ISettingsContext& settings() noexcept override;
    [[nodiscard]] IUtilityContext& utility() noexcept override;
    [[nodiscard]] IIOContext& io() noexcept override;

private:
    std::unique_ptr<IDatabaseContext> m_database;
    std::unique_ptr<IExportContext> m_export;
    std::unique_ptr<ISettingsContext> m_settings;
    std::unique_ptr<IUtilityContext> m_utility;
    std::unique_ptr<IIOContext> m_io;
};

}  // namespace velocitydb
