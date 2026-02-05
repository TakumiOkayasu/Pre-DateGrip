#include "settings_context.h"

#include "../utils/session_manager.h"
#include "../utils/settings_manager.h"

namespace velocitydb {

SettingsContext::SettingsContext() : m_settingsManager(std::make_unique<SettingsManager>()), m_sessionManager(std::make_unique<SessionManager>()) {
    m_settingsManager->load();
    m_sessionManager->load();
}

SettingsContext::~SettingsContext() = default;
SettingsContext::SettingsContext(SettingsContext&&) noexcept = default;
SettingsContext& SettingsContext::operator=(SettingsContext&&) noexcept = default;

std::string SettingsContext::getSettings() {
    // Serialize settings to JSON - delegated to IPCHandler for now
    return "{}";
}

void SettingsContext::updateSettings(std::string_view settingsJson) {
    // Parse and update settings - delegated to IPCHandler for now
}

std::string SettingsContext::getConnectionProfiles() {
    // Serialize profiles to JSON - delegated to IPCHandler for now
    return "[]";
}

std::expected<void, std::string> SettingsContext::saveConnectionProfile(std::string_view profileJson) {
    // Parse and save profile - delegated to IPCHandler for now
    return {};
}

std::expected<void, std::string> SettingsContext::deleteConnectionProfile(std::string_view profileId) {
    m_settingsManager->removeConnectionProfile(std::string(profileId));
    m_settingsManager->save();
    return {};
}

std::expected<std::string, std::string> SettingsContext::getProfilePassword(std::string_view profileId) {
    return m_settingsManager->getProfilePassword(std::string(profileId));
}

std::expected<std::string, std::string> SettingsContext::getSshPassword(std::string_view profileId) {
    return m_settingsManager->getSshPassword(std::string(profileId));
}

std::expected<std::string, std::string> SettingsContext::getSshKeyPassphrase(std::string_view profileId) {
    return m_settingsManager->getSshKeyPassphrase(std::string(profileId));
}

std::string SettingsContext::getSessionState() {
    // Serialize session to JSON - delegated to IPCHandler for now
    return "{}";
}

void SettingsContext::saveSessionState(std::string_view stateJson) {
    // Parse and save session - delegated to IPCHandler for now
}

std::string SettingsContext::getBookmarks() {
    // Serialize bookmarks to JSON - delegated to IPCHandler for now
    return "[]";
}

std::expected<void, std::string> SettingsContext::saveBookmark(std::string_view bookmarkJson) {
    // Parse and save bookmark - delegated to IPCHandler for now
    return {};
}

std::expected<void, std::string> SettingsContext::deleteBookmark(std::string_view bookmarkId) {
    // Delete bookmark - delegated to IPCHandler for now
    return {};
}

}  // namespace velocitydb
