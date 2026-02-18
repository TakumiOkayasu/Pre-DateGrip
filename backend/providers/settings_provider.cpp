#include "settings_provider.h"

#include "../utils/json_utils.h"
#include "../utils/session_manager.h"
#include "../utils/settings_manager.h"
#include "simdjson.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <format>

namespace velocitydb {

namespace {

[[nodiscard]] constexpr int narrowToInt(int64_t val) noexcept {
    return static_cast<int>(std::clamp(val, static_cast<int64_t>(INT_MIN), static_cast<int64_t>(INT_MAX)));
}

}  // namespace

SettingsProvider::SettingsProvider() : m_settingsManager(std::make_unique<SettingsManager>()), m_sessionManager(std::make_unique<SessionManager>()) {
    m_settingsManager->load();
    m_sessionManager->load();
}

SettingsProvider::~SettingsProvider() = default;
SettingsProvider::SettingsProvider(SettingsProvider&&) noexcept = default;
SettingsProvider& SettingsProvider::operator=(SettingsProvider&&) noexcept = default;

std::string SettingsProvider::handleGetSettings() {
    const auto& settings = m_settingsManager->getSettings();

    std::string json = "{";

    json += "\"general\":{";
    json += std::format("\"autoConnect\":{},", settings.general.autoConnect ? "true" : "false");
    json += std::format("\"lastConnectionId\":\"{}\",", JsonUtils::escapeString(settings.general.lastConnectionId));
    json += std::format("\"confirmOnExit\":{},", settings.general.confirmOnExit ? "true" : "false");
    json += std::format("\"maxQueryHistory\":{},", settings.general.maxQueryHistory);
    json += std::format("\"maxRecentConnections\":{},", settings.general.maxRecentConnections);
    json += std::format("\"language\":\"{}\"", JsonUtils::escapeString(settings.general.language));
    json += "},";

    json += "\"editor\":{";
    json += std::format("\"fontSize\":{},", settings.editor.fontSize);
    json += std::format("\"fontFamily\":\"{}\",", JsonUtils::escapeString(settings.editor.fontFamily));
    json += std::format("\"wordWrap\":{},", settings.editor.wordWrap ? "true" : "false");
    json += std::format("\"tabSize\":{},", settings.editor.tabSize);
    json += std::format("\"insertSpaces\":{},", settings.editor.insertSpaces ? "true" : "false");
    json += std::format("\"showLineNumbers\":{},", settings.editor.showLineNumbers ? "true" : "false");
    json += std::format("\"showMinimap\":{},", settings.editor.showMinimap ? "true" : "false");
    json += std::format("\"theme\":\"{}\"", JsonUtils::escapeString(settings.editor.theme));
    json += "},";

    json += "\"grid\":{";
    json += std::format("\"defaultPageSize\":{},", settings.grid.defaultPageSize);
    json += std::format("\"showRowNumbers\":{},", settings.grid.showRowNumbers ? "true" : "false");
    json += std::format("\"enableCellEditing\":{},", settings.grid.enableCellEditing ? "true" : "false");
    json += std::format("\"dateFormat\":\"{}\",", JsonUtils::escapeString(settings.grid.dateFormat));
    json += std::format("\"nullDisplay\":\"{}\"", JsonUtils::escapeString(settings.grid.nullDisplay));
    json += "}";

    json += "}";

    return JsonUtils::successResponse(json);
}

std::string SettingsProvider::handleUpdateSettings(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        AppSettings settings = m_settingsManager->getSettings();

        if (auto general = doc["general"]; !general.error()) {
            if (auto val = general["autoConnect"].get_bool(); !val.error())
                settings.general.autoConnect = val.value();
            if (auto val = general["confirmOnExit"].get_bool(); !val.error())
                settings.general.confirmOnExit = val.value();
            if (auto val = general["maxQueryHistory"].get_int64(); !val.error())
                settings.general.maxQueryHistory = narrowToInt(val.value());
            if (auto val = general["language"].get_string(); !val.error())
                settings.general.language = std::string(val.value());
        }

        if (auto editor = doc["editor"]; !editor.error()) {
            if (auto val = editor["fontSize"].get_int64(); !val.error())
                settings.editor.fontSize = narrowToInt(val.value());
            if (auto val = editor["fontFamily"].get_string(); !val.error())
                settings.editor.fontFamily = std::string(val.value());
            if (auto val = editor["wordWrap"].get_bool(); !val.error())
                settings.editor.wordWrap = val.value();
            if (auto val = editor["tabSize"].get_int64(); !val.error())
                settings.editor.tabSize = narrowToInt(val.value());
            if (auto val = editor["theme"].get_string(); !val.error())
                settings.editor.theme = std::string(val.value());
        }

        if (auto grid = doc["grid"]; !grid.error()) {
            if (auto val = grid["defaultPageSize"].get_int64(); !val.error())
                settings.grid.defaultPageSize = narrowToInt(val.value());
            if (auto val = grid["showRowNumbers"].get_bool(); !val.error())
                settings.grid.showRowNumbers = val.value();
            if (auto val = grid["nullDisplay"].get_string(); !val.error())
                settings.grid.nullDisplay = std::string(val.value());
        }

        if (auto window = doc["window"]; !window.error()) {
            if (auto val = window["width"].get_int64(); !val.error())
                settings.window.width = narrowToInt(val.value());
            if (auto val = window["height"].get_int64(); !val.error())
                settings.window.height = narrowToInt(val.value());
            if (auto val = window["x"].get_int64(); !val.error())
                settings.window.x = narrowToInt(val.value());
            if (auto val = window["y"].get_int64(); !val.error())
                settings.window.y = narrowToInt(val.value());
            if (auto val = window["isMaximized"].get_bool(); !val.error())
                settings.window.isMaximized = val.value();
        }

        m_settingsManager->updateSettings(settings);
        m_settingsManager->save();

        return JsonUtils::successResponse(R"({"saved":true})");
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SettingsProvider::handleGetConnectionProfiles() {
    const auto& profiles = m_settingsManager->getConnectionProfiles();

    auto profilesJson = JsonUtils::buildArray(profiles, [](std::string& out, const auto& p) {
        out += "{";
        out += std::format("\"id\":\"{}\",", JsonUtils::escapeString(p.id));
        out += std::format("\"name\":\"{}\",", JsonUtils::escapeString(p.name));
        out += std::format("\"server\":\"{}\",", JsonUtils::escapeString(p.server));
        out += std::format("\"port\":{},", p.port);
        out += std::format("\"database\":\"{}\",", JsonUtils::escapeString(p.database));
        out += std::format("\"username\":\"{}\",", JsonUtils::escapeString(p.username));
        out += std::format("\"useWindowsAuth\":{},", p.useWindowsAuth ? "true" : "false");
        out += std::format("\"savePassword\":{},", p.savePassword ? "true" : "false");
        out += std::format("\"isProduction\":{},", p.isProduction ? "true" : "false");
        out += std::format("\"isReadOnly\":{},", p.isReadOnly ? "true" : "false");
        out += std::format("\"environment\":\"{}\",", JsonUtils::escapeString(p.environment));
        out += std::format("\"dbType\":\"{}\",", JsonUtils::escapeString(p.dbType));
        out += "\"ssh\":{";
        out += std::format("\"enabled\":{},", p.ssh.enabled ? "true" : "false");
        out += std::format("\"host\":\"{}\",", JsonUtils::escapeString(p.ssh.host));
        out += std::format("\"port\":{},", p.ssh.port);
        out += std::format("\"username\":\"{}\",", JsonUtils::escapeString(p.ssh.username));
        out += std::format("\"authType\":\"{}\",", p.ssh.authType == SshAuthType::Password ? "password" : "privateKey");
        out += std::format("\"privateKeyPath\":\"{}\",", JsonUtils::escapeString(p.ssh.privateKeyPath));
        out += std::format("\"savePassword\":{}", !p.ssh.encryptedPassword.empty() || !p.ssh.encryptedKeyPassphrase.empty() ? "true" : "false");
        out += "}}";
    });
    std::string json = std::format(R"({{"profiles":{}}})", profilesJson);

    return JsonUtils::successResponse(json);
}

std::string SettingsProvider::handleSaveConnectionProfile(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        ConnectionProfile profile;
        if (auto val = doc["id"].get_string(); !val.error())
            profile.id = std::string(val.value());
        if (auto val = doc["name"].get_string(); !val.error())
            profile.name = std::string(val.value());
        if (auto val = doc["server"].get_string(); !val.error())
            profile.server = std::string(val.value());
        if (auto val = doc["port"].get_int64(); !val.error())
            profile.port = narrowToInt(val.value());
        if (auto val = doc["database"].get_string(); !val.error())
            profile.database = std::string(val.value());
        if (auto val = doc["username"].get_string(); !val.error())
            profile.username = std::string(val.value());
        if (auto val = doc["useWindowsAuth"].get_bool(); !val.error())
            profile.useWindowsAuth = val.value();
        if (auto val = doc["savePassword"].get_bool(); !val.error())
            profile.savePassword = val.value();
        if (auto val = doc["isProduction"].get_bool(); !val.error())
            profile.isProduction = val.value();
        if (auto val = doc["isReadOnly"].get_bool(); !val.error())
            profile.isReadOnly = val.value();
        if (auto val = doc["environment"].get_string(); !val.error())
            profile.environment = std::string(val.value());
        if (auto val = doc["dbType"].get_string(); !val.error())
            profile.dbType = std::string(val.value());

        if (auto ssh = doc["ssh"]; !ssh.error()) {
            if (auto val = ssh["enabled"].get_bool(); !val.error())
                profile.ssh.enabled = val.value();
            if (auto val = ssh["host"].get_string(); !val.error())
                profile.ssh.host = std::string(val.value());
            if (auto val = ssh["port"].get_int64(); !val.error())
                profile.ssh.port = narrowToInt(val.value());
            if (auto val = ssh["username"].get_string(); !val.error())
                profile.ssh.username = std::string(val.value());
            if (auto val = ssh["authType"].get_string(); !val.error())
                profile.ssh.authType = (val.value() == "privateKey") ? SshAuthType::PrivateKey : SshAuthType::Password;
            if (auto val = ssh["privateKeyPath"].get_string(); !val.error())
                profile.ssh.privateKeyPath = std::string(val.value());
        }

        if (profile.id.empty()) {
            profile.id = std::format("profile_{}", std::chrono::system_clock::now().time_since_epoch().count());
        }

        if (m_settingsManager->getConnectionProfile(profile.id).has_value()) {
            m_settingsManager->updateConnectionProfile(profile);
        } else {
            m_settingsManager->addConnectionProfile(profile);
        }

        if (profile.savePassword) {
            if (auto val = doc["password"].get_string(); !val.error()) {
                auto password = std::string(val.value());
                if (!password.empty()) {
                    (void)m_settingsManager->setProfilePassword(profile.id, password);
                }
            }
        } else {
            (void)m_settingsManager->setProfilePassword(profile.id, "");
        }

        if (auto ssh = doc["ssh"]; !ssh.error()) {
            if (auto savePass = ssh["savePassword"].get_bool(); !savePass.error() && savePass.value()) {
                if (auto val = ssh["password"].get_string(); !val.error()) {
                    auto sshPassword = std::string(val.value());
                    if (!sshPassword.empty()) {
                        (void)m_settingsManager->setSshPassword(profile.id, sshPassword);
                    }
                }
                if (auto val = ssh["keyPassphrase"].get_string(); !val.error()) {
                    auto keyPassphrase = std::string(val.value());
                    if (!keyPassphrase.empty()) {
                        (void)m_settingsManager->setSshKeyPassphrase(profile.id, keyPassphrase);
                    }
                }
            } else {
                (void)m_settingsManager->setSshPassword(profile.id, "");
                (void)m_settingsManager->setSshKeyPassphrase(profile.id, "");
            }
        }

        m_settingsManager->save();

        return JsonUtils::successResponse(std::format(R"({{"id":"{}"}})", JsonUtils::escapeString(profile.id)));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SettingsProvider::handleDeleteConnectionProfile(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto profileIdResult = doc["id"].get_string();
        if (profileIdResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: id");
        }
        auto profileId = std::string(profileIdResult.value());
        m_settingsManager->removeConnectionProfile(profileId);
        m_settingsManager->save();

        return JsonUtils::successResponse(R"({"deleted":true})");
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SettingsProvider::handleGetProfilePassword(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto idResult = doc["id"].get_string();
        if (idResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: id");
        }
        auto profileId = std::string(idResult.value());

        auto passwordResult = m_settingsManager->getProfilePassword(profileId);
        if (!passwordResult) {
            return JsonUtils::errorResponse(passwordResult.error());
        }

        return JsonUtils::successResponse(std::format(R"({{"password":"{}"}})", JsonUtils::escapeString(passwordResult.value())));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SettingsProvider::handleGetSshPassword(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto idResult = doc["id"].get_string();
        if (idResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: id");
        }
        auto profileId = std::string(idResult.value());

        auto passwordResult = m_settingsManager->getSshPassword(profileId);
        if (!passwordResult) {
            return JsonUtils::errorResponse(passwordResult.error());
        }

        return JsonUtils::successResponse(std::format(R"({{"password":"{}"}})", JsonUtils::escapeString(passwordResult.value())));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SettingsProvider::handleGetSshKeyPassphrase(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        auto idResult = doc["id"].get_string();
        if (idResult.error()) [[unlikely]] {
            return JsonUtils::errorResponse("Missing required field: id");
        }
        auto profileId = std::string(idResult.value());

        auto passphraseResult = m_settingsManager->getSshKeyPassphrase(profileId);
        if (!passphraseResult) {
            return JsonUtils::errorResponse(passphraseResult.error());
        }

        return JsonUtils::successResponse(std::format(R"({{"passphrase":"{}"}})", JsonUtils::escapeString(passphraseResult.value())));
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

std::string SettingsProvider::handleGetSessionState() {
    const auto& state = m_sessionManager->getState();

    std::string json = "{";
    json += std::format("\"activeConnectionId\":\"{}\",", JsonUtils::escapeString(state.activeConnectionId));
    json += std::format("\"activeTabId\":\"{}\",", JsonUtils::escapeString(state.activeTabId));
    json += std::format("\"windowX\":{},", state.windowX);
    json += std::format("\"windowY\":{},", state.windowY);
    json += std::format("\"windowWidth\":{},", state.windowWidth);
    json += std::format("\"windowHeight\":{},", state.windowHeight);
    json += std::format("\"isMaximized\":{},", state.isMaximized ? "true" : "false");
    json += std::format("\"leftPanelWidth\":{},", state.leftPanelWidth);
    json += std::format("\"bottomPanelHeight\":{},", state.bottomPanelHeight);

    auto openTabsJson = JsonUtils::buildArray(state.openTabs, [](std::string& out, const auto& tab) {
        out += "{";
        out += std::format("\"id\":\"{}\",", JsonUtils::escapeString(tab.id));
        out += std::format("\"title\":\"{}\",", JsonUtils::escapeString(tab.title));
        out += std::format("\"content\":\"{}\",", JsonUtils::escapeString(tab.content));
        out += std::format("\"filePath\":\"{}\",", JsonUtils::escapeString(tab.filePath));
        out += std::format("\"isDirty\":{},", tab.isDirty ? "true" : "false");
        out += std::format("\"cursorLine\":{},", tab.cursorLine);
        out += std::format("\"cursorColumn\":{}", tab.cursorColumn);
        out += "}";
    });
    json += "\"openTabs\":";
    json += openTabsJson;
    json += ",";

    auto expandedNodesJson = JsonUtils::buildArray(state.expandedTreeNodes, [](std::string& out, const auto& node) { out += std::format(R"("{}")", JsonUtils::escapeString(node)); });
    json += "\"expandedTreeNodes\":";
    json += expandedNodesJson;

    json += "}";

    return JsonUtils::successResponse(json);
}

std::string SettingsProvider::handleSaveSessionState(std::string_view params) {
    try {
        simdjson::dom::parser parser;
        auto doc = parser.parse(params);

        SessionState state = m_sessionManager->getState();

        if (auto val = doc["activeConnectionId"].get_string(); !val.error())
            state.activeConnectionId = std::string(val.value());
        if (auto val = doc["activeTabId"].get_string(); !val.error())
            state.activeTabId = std::string(val.value());
        if (auto val = doc["windowX"].get_int64(); !val.error())
            state.windowX = narrowToInt(val.value());
        if (auto val = doc["windowY"].get_int64(); !val.error())
            state.windowY = narrowToInt(val.value());
        if (auto val = doc["windowWidth"].get_int64(); !val.error())
            state.windowWidth = narrowToInt(val.value());
        if (auto val = doc["windowHeight"].get_int64(); !val.error())
            state.windowHeight = narrowToInt(val.value());
        if (auto val = doc["isMaximized"].get_bool(); !val.error())
            state.isMaximized = val.value();
        if (auto val = doc["leftPanelWidth"].get_int64(); !val.error())
            state.leftPanelWidth = narrowToInt(val.value());
        if (auto val = doc["bottomPanelHeight"].get_int64(); !val.error())
            state.bottomPanelHeight = narrowToInt(val.value());

        state.openTabs.clear();
        if (auto tabs = doc["openTabs"].get_array(); !tabs.error()) {
            for (auto tabEl : tabs.value()) {
                EditorTab tab;
                if (auto val = tabEl["id"].get_string(); !val.error())
                    tab.id = std::string(val.value());
                if (auto val = tabEl["title"].get_string(); !val.error())
                    tab.title = std::string(val.value());
                if (auto val = tabEl["content"].get_string(); !val.error())
                    tab.content = std::string(val.value());
                if (auto val = tabEl["filePath"].get_string(); !val.error())
                    tab.filePath = std::string(val.value());
                if (auto val = tabEl["isDirty"].get_bool(); !val.error())
                    tab.isDirty = val.value();
                if (auto val = tabEl["cursorLine"].get_int64(); !val.error())
                    tab.cursorLine = narrowToInt(val.value());
                if (auto val = tabEl["cursorColumn"].get_int64(); !val.error())
                    tab.cursorColumn = narrowToInt(val.value());
                state.openTabs.push_back(tab);
            }
        }

        state.expandedTreeNodes.clear();
        if (auto nodes = doc["expandedTreeNodes"].get_array(); !nodes.error()) {
            for (auto nodeEl : nodes.value()) {
                if (auto val = nodeEl.get_string(); !val.error()) {
                    state.expandedTreeNodes.push_back(std::string(val.value()));
                }
            }
        }

        m_sessionManager->updateState(state);
        m_sessionManager->save();

        return JsonUtils::successResponse(R"({"saved":true})");
    } catch (const std::exception& e) {
        return JsonUtils::errorResponse(e.what());
    }
}

}  // namespace velocitydb
