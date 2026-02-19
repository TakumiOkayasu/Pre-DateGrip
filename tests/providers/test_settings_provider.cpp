#include <gtest/gtest.h>

#include "providers/settings_provider.h"
#include "utils/settings_manager.h"
#include "utils/session_manager.h"

namespace velocitydb {
namespace {

class SettingsProviderTest : public ::testing::Test {
protected:
    SettingsProvider provider;
};

TEST_F(SettingsProviderTest, AccessSettingsManager) {
    // Verify direct access to SettingsManager works
    auto& manager = provider.settingsManager();
    const auto& settings = manager.getSettings();

    // Default settings should have reasonable values
    EXPECT_GT(settings.editor.fontSize, 0);
    EXPECT_FALSE(settings.editor.fontFamily.empty());
}

TEST_F(SettingsProviderTest, AccessSessionManager) {
    // Verify direct access to SessionManager works
    auto& manager = provider.sessionManager();
    const auto& state = manager.getState();

    // Default session state should have reasonable values
    EXPECT_GE(state.windowWidth, 0);
    EXPECT_GE(state.windowHeight, 0);
}

TEST_F(SettingsProviderTest, GetProfilePasswordNotFound) {
    // Non-existent profile should return error JSON
    auto result = provider.getProfilePassword(R"({"id":"non_existent_profile_id"})");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("error"), std::string::npos);
}

TEST_F(SettingsProviderTest, GetSshPasswordNotFound) {
    // Non-existent profile should return error JSON
    auto result = provider.getSshPassword(R"({"id":"non_existent_profile_id"})");
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("error"), std::string::npos);
}

TEST_F(SettingsProviderTest, DeleteNonExistentProfile) {
    // Deleting non-existent profile should succeed (idempotent)
    auto result = provider.deleteConnectionProfile(R"({"id":"non_existent_profile_id"})");
    EXPECT_FALSE(result.empty());
}

}  // namespace
}  // namespace velocitydb
