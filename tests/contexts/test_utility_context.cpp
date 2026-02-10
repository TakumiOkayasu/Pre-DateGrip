#include <gtest/gtest.h>

#include "contexts/utility_context.h"

namespace velocitydb {
namespace {

class UtilityContextTest : public ::testing::Test {
protected:
    UtilityContext ctx;
};

TEST_F(UtilityContextTest, HandleFormatSQLBasic) {
    auto result = ctx.handleFormatSQL(R"({"sql":"select * from users where id=1"})");
    EXPECT_FALSE(result.empty());
    // Should contain uppercase keywords in JSON response
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("FROM"), std::string::npos);
    EXPECT_NE(result.find("WHERE"), std::string::npos);
}

TEST_F(UtilityContextTest, HandleUppercaseKeywords) {
    auto result = ctx.handleUppercaseKeywords(R"({"sql":"select * from users"})");
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("FROM"), std::string::npos);
}

TEST_F(UtilityContextTest, HandleFormatSQLPreservesIdentifiers) {
    auto result = ctx.handleFormatSQL(R"({"sql":"select userName, email from users"})");
    // Identifiers should be preserved
    EXPECT_NE(result.find("userName"), std::string::npos);
    EXPECT_NE(result.find("email"), std::string::npos);
    EXPECT_NE(result.find("users"), std::string::npos);
}

TEST_F(UtilityContextTest, HandleFormatSQLComplex) {
    auto result = ctx.handleFormatSQL(R"({"sql":"select a, b from t1 inner join t2 on t1.id = t2.id where a > 1"})");
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("INNER JOIN"), std::string::npos);
    EXPECT_NE(result.find("WHERE"), std::string::npos);
}

}  // namespace
}  // namespace velocitydb
