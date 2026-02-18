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

    [[nodiscard]] std::string getSettings() override;
    [[nodiscard]] std::string updateSettings(std::string_view params) override;
    [[nodiscard]] std::string getConnectionProfiles() override;
    [[nodiscard]] std::string saveConnectionProfile(std::string_view params) override;
    [[nodiscard]] std::string deleteConnectionProfile(std::string_view params) override;
    [[nodiscard]] std::string getProfilePassword(std::string_view params) override;
    [[nodiscard]] std::string getSshPassword(std::string_view params) override;
    [[nodiscard]] std::string getSshKeyPassphrase(std::string_view params) override;
    [[nodiscard]] std::string getSessionState() override;
    [[nodiscard]] std::string saveSessionState(std::string_view params) override;

    [[nodiscard]] SettingsManager& settingsManager() { return *m_settingsManager; }
    [[nodiscard]] const SettingsManager& settingsManager() const { return *m_settingsManager; }
    [[nodiscard]] SessionManager& sessionManager() { return *m_sessionManager; }
    [[nodiscard]] const SessionManager& sessionManager() const { return *m_sessionManager; }

private:
    std::unique_ptr<SettingsManager> m_settingsManager;
    std::unique_ptr<SessionManager> m_sessionManager;
};

}  // namespace velocitydb
