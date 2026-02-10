#pragma once

#include "../interfaces/settings_context.h"

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace velocitydb {

class SettingsManager;
class SessionManager;

/// Context for application settings and session management
class SettingsContext : public ISettingsContext {
public:
    SettingsContext();
    ~SettingsContext() override;

    SettingsContext(const SettingsContext&) = delete;
    SettingsContext& operator=(const SettingsContext&) = delete;
    SettingsContext(SettingsContext&&) noexcept;
    SettingsContext& operator=(SettingsContext&&) noexcept;

    // ISettingsContext implementation (IPC handle methods)
    [[nodiscard]] std::string handleGetSettings() override;
    [[nodiscard]] std::string handleUpdateSettings(std::string_view params) override;
    [[nodiscard]] std::string handleGetConnectionProfiles() override;
    [[nodiscard]] std::string handleSaveConnectionProfile(std::string_view params) override;
    [[nodiscard]] std::string handleDeleteConnectionProfile(std::string_view params) override;
    [[nodiscard]] std::string handleGetProfilePassword(std::string_view params) override;
    [[nodiscard]] std::string handleGetSshPassword(std::string_view params) override;
    [[nodiscard]] std::string handleGetSshKeyPassphrase(std::string_view params) override;
    [[nodiscard]] std::string handleGetSessionState() override;
    [[nodiscard]] std::string handleSaveSessionState(std::string_view params) override;

    // Direct access to managers (for legacy/migration use)
    [[nodiscard]] SettingsManager& settingsManager() { return *m_settingsManager; }
    [[nodiscard]] const SettingsManager& settingsManager() const { return *m_settingsManager; }
    [[nodiscard]] SessionManager& sessionManager() { return *m_sessionManager; }
    [[nodiscard]] const SessionManager& sessionManager() const { return *m_sessionManager; }

private:
    std::unique_ptr<SettingsManager> m_settingsManager;
    std::unique_ptr<SessionManager> m_sessionManager;
};

}  // namespace velocitydb
