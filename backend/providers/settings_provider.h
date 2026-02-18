#pragma once

#include "../interfaces/providers/settings_provider.h"

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class SettingsManager;
class SessionManager;

/// Provider for application settings and session management
class SettingsProvider : public ISettingsProvider {
public:
    SettingsProvider();
    ~SettingsProvider() override;

    SettingsProvider(const SettingsProvider&) = delete;
    SettingsProvider& operator=(const SettingsProvider&) = delete;
    SettingsProvider(SettingsProvider&&) noexcept;
    SettingsProvider& operator=(SettingsProvider&&) noexcept;

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

    [[nodiscard]] SettingsManager& settingsManager() { return *m_settingsManager; }
    [[nodiscard]] const SettingsManager& settingsManager() const { return *m_settingsManager; }
    [[nodiscard]] SessionManager& sessionManager() { return *m_sessionManager; }
    [[nodiscard]] const SessionManager& sessionManager() const { return *m_sessionManager; }

private:
    std::unique_ptr<SettingsManager> m_settingsManager;
    std::unique_ptr<SessionManager> m_sessionManager;
};

}  // namespace velocitydb
