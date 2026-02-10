#include "system_context.h"

#include "database_context.h"
#include "export_context.h"
#include "io_context.h"
#include "settings_context.h"
#include "utility_context.h"

namespace velocitydb {

SystemContext::SystemContext()
    : m_database(std::make_unique<DatabaseContext>())
    , m_export(std::make_unique<ExportContext>())
    , m_settings(std::make_unique<SettingsContext>())
    , m_utility(std::make_unique<UtilityContext>())
    , m_io(std::make_unique<IOContext>()) {}

SystemContext::~SystemContext() = default;

IDatabaseContext& SystemContext::database() noexcept {
    return *m_database;
}

IExportContext& SystemContext::export_ctx() noexcept {
    return *m_export;
}

ISettingsContext& SystemContext::settings() noexcept {
    return *m_settings;
}

IUtilityContext& SystemContext::utility() noexcept {
    return *m_utility;
}

IIOContext& SystemContext::io() noexcept {
    return *m_io;
}

}  // namespace velocitydb
