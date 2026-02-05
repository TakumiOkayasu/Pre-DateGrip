#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace velocitydb {

class SettingsManager;
class SessionManager;

/// Context for application settings and session management
class SettingsContext {
public:
    SettingsContext();
    ~SettingsContext();

    SettingsContext(const SettingsContext&) = delete;
    SettingsContext& operator=(const SettingsContext&) = delete;
    SettingsContext(SettingsContext&&) noexcept;
    SettingsContext& operator=(SettingsContext&&) noexcept;

    // Direct access to managers (for migration)
    [[nodiscard]] SettingsManager& settingsManager() { return *m_settingsManager; }
    [[nodiscard]] const SettingsManager& settingsManager() const { return *m_settingsManager; }
    [[nodiscard]] SessionManager& sessionManager() { return *m_sessionManager; }
    [[nodiscard]] const SessionManager& sessionManager() const { return *m_sessionManager; }

    // Application settings
    [[nodiscard]] std::string getSettings();
    void updateSettings(std::string_view settingsJson);

    // Connection profiles
    [[nodiscard]] std::string getConnectionProfiles();
    [[nodiscard]] std::expected<void, std::string> saveConnectionProfile(std::string_view profileJson);
    [[nodiscard]] std::expected<void, std::string> deleteConnectionProfile(std::string_view profileId);

    // Credential retrieval
    [[nodiscard]] std::expected<std::string, std::string> getProfilePassword(std::string_view profileId);
    [[nodiscard]] std::expected<std::string, std::string> getSshPassword(std::string_view profileId);
    [[nodiscard]] std::expected<std::string, std::string> getSshKeyPassphrase(std::string_view profileId);

    // Session state
    [[nodiscard]] std::string getSessionState();
    void saveSessionState(std::string_view stateJson);

    // Bookmarks
    [[nodiscard]] std::string getBookmarks();
    [[nodiscard]] std::expected<void, std::string> saveBookmark(std::string_view bookmarkJson);
    [[nodiscard]] std::expected<void, std::string> deleteBookmark(std::string_view bookmarkId);

private:
    std::unique_ptr<SettingsManager> m_settingsManager;
    std::unique_ptr<SessionManager> m_sessionManager;
};

}  // namespace velocitydb
