#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for application settings and session management
class ISettingsProvider {
public:
    virtual ~ISettingsProvider() = default;

    [[nodiscard]] virtual std::string handleGetSettings() = 0;
    [[nodiscard]] virtual std::string handleUpdateSettings(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetConnectionProfiles() = 0;
    [[nodiscard]] virtual std::string handleSaveConnectionProfile(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleDeleteConnectionProfile(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetProfilePassword(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetSshPassword(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetSshKeyPassphrase(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetSessionState() = 0;
    [[nodiscard]] virtual std::string handleSaveSessionState(std::string_view params) = 0;
};

}  // namespace velocitydb
