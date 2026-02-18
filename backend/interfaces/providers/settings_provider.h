#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for application settings and session management
class ISettingsProvider {
public:
    virtual ~ISettingsProvider() = default;

    [[nodiscard]] virtual std::string getSettings() = 0;
    [[nodiscard]] virtual std::string updateSettings(std::string_view params) = 0;
    [[nodiscard]] virtual std::string getConnectionProfiles() = 0;
    [[nodiscard]] virtual std::string saveConnectionProfile(std::string_view params) = 0;
    [[nodiscard]] virtual std::string deleteConnectionProfile(std::string_view params) = 0;
    [[nodiscard]] virtual std::string getProfilePassword(std::string_view params) = 0;
    [[nodiscard]] virtual std::string getSshPassword(std::string_view params) = 0;
    [[nodiscard]] virtual std::string getSshKeyPassphrase(std::string_view params) = 0;
    [[nodiscard]] virtual std::string getSessionState() = 0;
    [[nodiscard]] virtual std::string saveSessionState(std::string_view params) = 0;
};

}  // namespace velocitydb
