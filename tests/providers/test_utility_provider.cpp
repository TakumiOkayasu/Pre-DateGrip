#include <gtest/gtest.h>
#include <simdjson.h>

#include "providers/utility_provider.h"

namespace velocitydb {
namespace test {

class UtilityProviderTest : public ::testing::Test {
protected:
    UtilityProvider provider;
};

// --- uppercaseKeywords ---

TEST_F(UtilityProviderTest, UppercaseKeywords) {
    auto result = provider.uppercaseKeywords(R"({"sql":"select * from users"})");
    EXPECT_NE(result.find("SELECT"), std::string::npos);
    EXPECT_NE(result.find("FROM"), std::string::npos);
}

TEST_F(UtilityProviderTest, UppercaseKeywordsMissingSql) {
    auto result = provider.uppercaseKeywords(R"({"query":"select"})");
    EXPECT_NE(result.find("error"), std::string::npos);
}

// --- parseERDiagram ---

TEST_F(UtilityProviderTest, ParseERDiagramWithContent) {
    auto result = provider.parseERDiagram(R"({
        "content": "# A5:ER FORMAT:19\n\n[Entity]\nPName=users\nLName=User\nField=\"id\",\"id\",\"INT\",\"NOT NULL\",0,\"\",\"\"\nDEL\n",
        "filename": "test.a5er"
    })");

    EXPECT_NE(result.find("\"success\":true"), std::string::npos);
    EXPECT_NE(result.find("\"name\":\"users\""), std::string::npos);
    EXPECT_NE(result.find("\"ddl\":\""), std::string::npos);
}

TEST_F(UtilityProviderTest, ParseERDiagramJsonStructure) {
    auto result = provider.parseERDiagram(R"({
        "content": "# A5:ER FORMAT:19\n\n[Entity]\nPName=users\nLName=User\nColor=$00FF00\nField=\"id\",\"id\",\"INT\",\"NOT NULL\",0,\"\",\"\"\nField=\"name\",\"name\",\"VARCHAR\",\"\",,,\"\",\"$FF008040\"\nDEL\n\n[Shape]\nShapeType=Rectangle\nText=memo\nBrushColor=$00FF00\nLeft=100\nTop=200\nWidth=300\nHeight=150\nPage=Main\nDEL\n",
        "filename": "test.a5er"
    })");

    simdjson::dom::parser parser;
    auto doc = parser.parse(result);
    EXPECT_TRUE(doc["success"].get_bool().value());

    auto data = doc["data"];
    EXPECT_EQ(data["name"].get_string().value(), "test.a5er");

    // Tables array
    auto tables = data["tables"].get_array().value();
    EXPECT_EQ(tables.size(), 1);

    auto table = tables.at(0);
    EXPECT_EQ(table["name"].get_string().value(), "users");
    EXPECT_EQ(table["logicalName"].get_string().value(), "User");
    EXPECT_EQ(table["color"].get_string().value(), "#00FF00");

    // Columns
    auto columns = table["columns"].get_array().value();
    EXPECT_EQ(columns.size(), 2);
    EXPECT_EQ(columns.at(0)["name"].get_string().value(), "id");
    EXPECT_TRUE(columns.at(0)["isPrimaryKey"].get_bool().value());
    EXPECT_EQ(columns.at(1)["name"].get_string().value(), "name");

    // Shapes
    auto shapes = data["shapes"].get_array().value();
    EXPECT_EQ(shapes.size(), 1);
    EXPECT_EQ(shapes.at(0)["shapeType"].get_string().value(), "rectangle");
    EXPECT_EQ(shapes.at(0)["text"].get_string().value(), "memo");
    EXPECT_EQ(shapes.at(0)["page"].get_string().value(), "Main");

    // DDL present and non-empty
    auto ddl = data["ddl"].get_string().value();
    EXPECT_FALSE(std::string(ddl).empty());
    EXPECT_NE(std::string(ddl).find("CREATE TABLE"), std::string::npos);
}

TEST_F(UtilityProviderTest, ParseERDiagramMissingContent) {
    simdjson::dom::parser p;
    auto result = provider.parseERDiagram(R"({})");
    auto doc = p.parse(result);
    EXPECT_FALSE(doc["success"].get_bool().value());
    EXPECT_FALSE(doc["error"].get_string().error());
}

// --- filepath branch ---

TEST_F(UtilityProviderTest, ParseERDiagramPathTraversal) {
    simdjson::dom::parser p;
    auto result = provider.parseERDiagram(R"({"filepath":"../etc/passwd"})");
    auto doc = p.parse(result);
    EXPECT_FALSE(doc["success"].get_bool().value());
}

TEST_F(UtilityProviderTest, ParseERDiagramFileNotFound) {
    simdjson::dom::parser p;
    auto result = provider.parseERDiagram(R"({"filepath":"C:/nonexistent_dir/test.a5er"})");
    auto doc = p.parse(result);
    EXPECT_FALSE(doc["success"].get_bool().value());
}

TEST_F(UtilityProviderTest, ParseERDiagramUnrecognisedFormat) {
    simdjson::dom::parser p;
    auto result = provider.parseERDiagram(R"({"content":"not an ER format","filename":"test.txt"})");
    auto doc = p.parse(result);
    EXPECT_FALSE(doc["success"].get_bool().value());
}

}  // namespace test
}  // namespace velocitydb
