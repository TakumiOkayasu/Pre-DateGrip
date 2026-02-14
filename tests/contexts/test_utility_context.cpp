#include <gtest/gtest.h>

#include "contexts/utility_context.h"

namespace velocitydb {
namespace {

class UtilityContextTest : public ::testing::Test {
protected:
    UtilityContext ctx;
};

TEST_F(UtilityContextTest, HandleUppercaseKeywords) {
    auto result = ctx.handleUppercaseKeywords(R"({"sql":"select * from users"})");
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("FROM"), std::string::npos);
}

}  // namespace
}  // namespace velocitydb
