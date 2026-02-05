#include <gtest/gtest.h>

#include "contexts/utility_context.h"

namespace velocitydb {
namespace {

class UtilityContextTest : public ::testing::Test {
protected:
    UtilityContext ctx;
};

TEST_F(UtilityContextTest, FormatSQLBasic) {
    auto result = ctx.formatSQL("select * from users where id=1");
    EXPECT_FALSE(result.empty());
    // Should contain uppercase keywords
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("FROM"), std::string::npos);
    EXPECT_NE(result.find("WHERE"), std::string::npos);
}

TEST_F(UtilityContextTest, UppercaseKeywords) {
    auto result = ctx.uppercaseKeywords("select * from users");
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("FROM"), std::string::npos);
}

TEST_F(UtilityContextTest, FormatSQLPreservesIdentifiers) {
    auto result = ctx.formatSQL("select userName, email from users");
    // Identifiers should be preserved
    EXPECT_NE(result.find("userName"), std::string::npos);
    EXPECT_NE(result.find("email"), std::string::npos);
    EXPECT_NE(result.find("users"), std::string::npos);
}

TEST_F(UtilityContextTest, FormatSQLComplex) {
    // Test more complex SQL formatting
    auto result = ctx.formatSQL("select a, b from t1 inner join t2 on t1.id = t2.id where a > 1");
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("INNER JOIN"), std::string::npos);
    EXPECT_NE(result.find("WHERE"), std::string::npos);
}

}  // namespace
}  // namespace velocitydb
