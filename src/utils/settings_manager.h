#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace predategrip {

using SettingValue = std::variant<bool, int, double, std::string>;

struct ConnectionProfile {
    std::string id;
    std::string name;
    std::string server;
    std::string database;
    std::string username;
    bool useWindowsAuth = true;
    bool savePassword = false;
    std::string encryptedPassword;
};

struct EditorSettings {
    int fontSize = 14;
    std::string fontFamily = "Consolas";
    bool wordWrap = false;
    int tabSize = 4;
    bool insertSpaces = true;
    bool showLineNumbers = true;
    bool showMinimap = true;
    std::string theme = "vs-dark";
};

struct GridSettings {
    int defaultPageSize = 100;
    bool showRowNumbers = true;
    bool enableCellEditing = false;
    std::string dateFormat = "yyyy-MM-dd HH:mm:ss";
    std::string nullDisplay = "(NULL)";
};

struct GeneralSettings {
    bool autoConnect = false;
    std::string lastConnectionId;
    bool confirmOnExit = true;
    int maxQueryHistory = 1000;
    int maxRecentConnections = 10;
    std::string language = "en";
};

struct AppSettings {
    GeneralSettings general;
    EditorSettings editor;
    GridSettings grid;
    std::vector<ConnectionProfile> connectionProfiles;
};

class SettingsManager {
public:
    SettingsManager();
    ~SettingsManager() = default;

    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    /// Load settings from disk
    bool load();

    /// Save settings to disk
    bool save();

    /// Get current settings
    [[nodiscard]] const AppSettings& getSettings() const { return m_settings; }

    /// Update settings
    void updateSettings(const AppSettings& settings);

    /// Connection profile management
    void addConnectionProfile(const ConnectionProfile& profile);
    void updateConnectionProfile(const ConnectionProfile& profile);
    void removeConnectionProfile(const std::string& id);
    [[nodiscard]] std::optional<ConnectionProfile> getConnectionProfile(const std::string& id) const;
    [[nodiscard]] const std::vector<ConnectionProfile>& getConnectionProfiles() const;

    /// Get settings file path
    [[nodiscard]] std::filesystem::path getSettingsPath() const;

private:
    [[nodiscard]] std::string serializeSettings() const;
    bool deserializeSettings(std::string_view json);

    AppSettings m_settings;
    std::filesystem::path m_settingsPath;
    mutable std::mutex m_mutex;
};

}  // namespace predategrip
