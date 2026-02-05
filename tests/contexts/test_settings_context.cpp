#include <gtest/gtest.h>

#include "contexts/settings_context.h"
#include "utils/settings_manager.h"
#include "utils/session_manager.h"

namespace velocitydb {
namespace {

class SettingsContextTest : public ::testing::Test {
protected:
    SettingsContext ctx;
};

TEST_F(SettingsContextTest, AccessSettingsManager) {
    // Verify direct access to SettingsManager works
    auto& manager = ctx.settingsManager();
    const auto& settings = manager.getSettings();

    // Default settings should have reasonable values
    EXPECT_GT(settings.editor.fontSize, 0);
    EXPECT_FALSE(settings.editor.fontFamily.empty());
}

TEST_F(SettingsContextTest, AccessSessionManager) {
    // Verify direct access to SessionManager works
    auto& manager = ctx.sessionManager();
    const auto& state = manager.getState();

    // Default session state should have reasonable values
    EXPECT_GE(state.windowWidth, 0);
    EXPECT_GE(state.windowHeight, 0);
}

TEST_F(SettingsContextTest, GetProfilePasswordNotFound) {
    // Non-existent profile should return error
    auto result = ctx.getProfilePassword("non_existent_profile_id");
    EXPECT_FALSE(result.has_value());
}

TEST_F(SettingsContextTest, GetSshPasswordNotFound) {
    // Non-existent profile should return error
    auto result = ctx.getSshPassword("non_existent_profile_id");
    EXPECT_FALSE(result.has_value());
}

TEST_F(SettingsContextTest, DeleteNonExistentProfile) {
    // Deleting non-existent profile should succeed (idempotent)
    auto result = ctx.deleteConnectionProfile("non_existent_profile_id");
    EXPECT_TRUE(result.has_value());
}

}  // namespace
}  // namespace velocitydb
