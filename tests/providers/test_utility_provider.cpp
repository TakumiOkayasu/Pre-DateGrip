#include <gtest/gtest.h>

#include "providers/utility_provider.h"

namespace velocitydb {
namespace {

class UtilityProviderTest : public ::testing::Test {
protected:
    UtilityProvider provider;
};

TEST_F(UtilityProviderTest, HandleUppercaseKeywords) {
    auto result = provider.handleUppercaseKeywords(R"({"sql":"select * from users"})");
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("FROM"), std::string::npos);
}

}  // namespace
}  // namespace velocitydb
