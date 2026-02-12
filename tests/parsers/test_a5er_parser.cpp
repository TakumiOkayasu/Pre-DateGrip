#include <gtest/gtest.h>
#include "parsers/a5er_parser.h"

namespace velocitydb {
namespace test {

class A5ERParserTest : public ::testing::Test {
protected:
    A5ERParser parser;
};

TEST_F(A5ERParserTest, GeneratesTableDDL) {
    A5ERTable table;
    table.name = "Users";

    A5ERColumn col1;
    col1.name = "id";
    col1.type = "INT";
    col1.nullable = false;
    col1.isPrimaryKey = true;
    table.columns.push_back(col1);

    A5ERColumn col2;
    col2.name = "name";
    col2.type = "VARCHAR";
    col2.size = 100;
    col2.nullable = true;
    col2.isPrimaryKey = false;
    table.columns.push_back(col2);

    std::string ddl = parser.generateTableDDL(table);

    EXPECT_NE(ddl.find("CREATE TABLE [Users]"), std::string::npos);
    EXPECT_NE(ddl.find("[id]"), std::string::npos);
    EXPECT_NE(ddl.find("[name]"), std::string::npos);
    EXPECT_NE(ddl.find("PRIMARY KEY"), std::string::npos);
}

TEST_F(A5ERParserTest, MapsTypesToSQLServer) {
    A5ERTable table;
    table.name = "TestTypes";

    A5ERColumn col1;
    col1.name = "text_col";
    col1.type = "string";  // A5:ER Japanese type name
    col1.size = 50;
    table.columns.push_back(col1);

    A5ERColumn col2;
    col2.name = "int_col";
    col2.type = "integer";  // A5:ER Japanese type name
    table.columns.push_back(col2);

    std::string ddl = parser.generateTableDDL(table);

    EXPECT_NE(ddl.find("NVARCHAR(50)"), std::string::npos);
    EXPECT_NE(ddl.find("INT"), std::string::npos);
}

TEST_F(A5ERParserTest, GeneratesIndexes) {
    A5ERTable table;
    table.name = "Users";

    A5ERColumn col;
    col.name = "email";
    col.type = "VARCHAR";
    col.size = 255;
    table.columns.push_back(col);

    A5ERIndex idx;
    idx.name = "IX_Users_Email";
    idx.columns.push_back("email");
    idx.isUnique = true;
    table.indexes.push_back(idx);

    std::string ddl = parser.generateTableDDL(table);

    EXPECT_NE(ddl.find("CREATE UNIQUE INDEX"), std::string::npos);
    EXPECT_NE(ddl.find("[IX_Users_Email]"), std::string::npos);
}

// === Text format parser tests ===

static const char* BASIC_TEXT_INPUT = R"a5er(# A5:ER FORMAT:19
# A5:ER ENCODING:UTF-8

[Entity]
PName=users
LName=User
Comment=User master
Page=MAIN
Left=100
Top=200
Field="id","id","INT","NOT NULL",1,"",""
Field="name","display_name","NVARCHAR(100)","NOT NULL","","",""
Field="email","email_addr","NVARCHAR(255)","NULL","","",""
DEL

[Entity]
PName=orders
LName=Order
Comment=
Page=MAIN
Left=400
Top=200
Field="id","id","INT","NOT NULL",1,"",""
Field="user_id","user_ref","INT","NOT NULL","","",""
Field="amount","price","DECIMAL(10,2)","NOT NULL","","0",""
DEL

[Relation]
Entity1=users
Entity2=orders
RelationType1=2
RelationType2=3
Fields1=id
Fields2=user_id
Dependence=0
DEL
)a5er";

TEST_F(A5ERParserTest, TextFormatDetection) {
    EXPECT_EQ(parser.parseFromString("# A5:ER FORMAT:19\n[Entity]\nPName=t\nDEL\n").tables.size(), 1);
    // XML format should not be detected as text
    EXPECT_NO_THROW(parser.parseFromString("<?xml version=\"1.0\"?><A5ER Name=\"test\"></A5ER>"));
}

TEST_F(A5ERParserTest, TextFormatBasicParse) {
    auto model = parser.parseFromString(BASIC_TEXT_INPUT);

    EXPECT_EQ(model.tables.size(), 2);
    EXPECT_EQ(model.relations.size(), 1);
}

TEST_F(A5ERParserTest, TextFormatTableProperties) {
    auto model = parser.parseFromString(BASIC_TEXT_INPUT);
    const auto& users = model.tables[0];

    EXPECT_EQ(users.name, "users");
    EXPECT_EQ(users.logicalName, "User");
    EXPECT_EQ(users.comment, "User master");
    EXPECT_EQ(users.posX, 100.0);
    EXPECT_EQ(users.posY, 200.0);
}

TEST_F(A5ERParserTest, TextFormatPageProperty) {
    auto model = parser.parseFromString(BASIC_TEXT_INPUT);

    EXPECT_EQ(model.tables[0].page, "MAIN");
    EXPECT_EQ(model.tables[1].page, "MAIN");
}

TEST_F(A5ERParserTest, TextFormatMultiplePages) {
    const char* input = R"a5er(# A5:ER FORMAT:19

[Entity]
PName=users
LName=User
Page=MAIN
Left=100
Top=200
Field="id","id","INT","NOT NULL",0,"",""
DEL

[Entity]
PName=logs
LName=Log
Page=SUB
Left=100
Top=200
Field="id","id","INT","NOT NULL",0,"",""
DEL

[Entity]
PName=orders
LName=Order
Page=MAIN
Left=400
Top=200
Field="id","id","INT","NOT NULL",0,"",""
DEL
)a5er";
    auto model = parser.parseFromString(input);

    ASSERT_EQ(model.tables.size(), 3);
    EXPECT_EQ(model.tables[0].page, "MAIN");
    EXPECT_EQ(model.tables[1].page, "SUB");
    EXPECT_EQ(model.tables[2].page, "MAIN");
}

TEST_F(A5ERParserTest, TextFormatEmptyPage) {
    const char* input = "# A5:ER FORMAT:19\n\n[Entity]\nPName=test\nLName=Test\nDEL\n";
    auto model = parser.parseFromString(input);

    ASSERT_EQ(model.tables.size(), 1);
    EXPECT_EQ(model.tables[0].page, "");
}

TEST_F(A5ERParserTest, TextFormatColumns) {
    auto model = parser.parseFromString(BASIC_TEXT_INPUT);
    const auto& users = model.tables[0];

    ASSERT_EQ(users.columns.size(), 3);

    EXPECT_EQ(users.columns[0].name, "id");
    EXPECT_EQ(users.columns[0].logicalName, "id");
    EXPECT_EQ(users.columns[0].type, "INT");
    EXPECT_FALSE(users.columns[0].nullable);
    EXPECT_TRUE(users.columns[0].isPrimaryKey);

    EXPECT_EQ(users.columns[1].name, "name");
    EXPECT_EQ(users.columns[1].logicalName, "display_name");
    EXPECT_EQ(users.columns[1].type, "NVARCHAR(100)");
    EXPECT_FALSE(users.columns[1].nullable);
    EXPECT_FALSE(users.columns[1].isPrimaryKey);

    EXPECT_TRUE(users.columns[2].nullable);
}

TEST_F(A5ERParserTest, TextFormatDefaultValues) {
    auto model = parser.parseFromString(BASIC_TEXT_INPUT);
    const auto& orders = model.tables[1];

    EXPECT_EQ(orders.columns[2].defaultValue, "0");
    EXPECT_EQ(orders.columns[0].defaultValue, "");
}

TEST_F(A5ERParserTest, TextFormatRelation) {
    auto model = parser.parseFromString(BASIC_TEXT_INPUT);

    ASSERT_EQ(model.relations.size(), 1);
    const auto& rel = model.relations[0];

    EXPECT_EQ(rel.parentTable, "users");
    EXPECT_EQ(rel.childTable, "orders");
    EXPECT_EQ(rel.parentColumn, "id");
    EXPECT_EQ(rel.childColumn, "user_id");
    EXPECT_EQ(rel.cardinality, "1:N");
}

TEST_F(A5ERParserTest, TextFormatCardinalityConversions) {
    auto makeInput = [](int type1, int type2) {
        return "# A5:ER FORMAT:19\n"
               "[Entity]\nPName=a\nLName=A\nField=\"id\",\"id\",\"INT\",\"NOT NULL\",1,\"\",\"\"\nDEL\n"
               "[Entity]\nPName=b\nLName=B\nField=\"id\",\"id\",\"INT\",\"NOT NULL\",1,\"\",\"\"\nDEL\n"
               "[Relation]\nEntity1=a\nEntity2=b\nRelationType1=" +
               std::to_string(type1) + "\nRelationType2=" + std::to_string(type2) + "\nFields1=id\nFields2=id\nDEL\n";
    };

    EXPECT_EQ(parser.parseFromString(makeInput(2, 3)).relations[0].cardinality, "1:N");
    EXPECT_EQ(parser.parseFromString(makeInput(2, 4)).relations[0].cardinality, "1:N");
    EXPECT_EQ(parser.parseFromString(makeInput(3, 2)).relations[0].cardinality, "1:N");
    EXPECT_EQ(parser.parseFromString(makeInput(2, 2)).relations[0].cardinality, "1:1");
    EXPECT_EQ(parser.parseFromString(makeInput(1, 2)).relations[0].cardinality, "1:1");
    EXPECT_EQ(parser.parseFromString(makeInput(3, 3)).relations[0].cardinality, "N:M");
    EXPECT_EQ(parser.parseFromString(makeInput(4, 4)).relations[0].cardinality, "N:M");
    EXPECT_EQ(parser.parseFromString(makeInput(3, 4)).relations[0].cardinality, "N:M");
}

TEST_F(A5ERParserTest, TextFormatIndexes) {
    const char* input = R"a5er(# A5:ER FORMAT:19

[Entity]
PName=users
LName=User
Field="id","id","INT","NOT NULL",1,"",""
Field="email","email_addr","NVARCHAR(255)","NOT NULL","","",""
Field="name","display_name","NVARCHAR(100)","NULL","","",""
Index=IX_users_email=0,email
Index=UQ_users_name=1,name
DEL
)a5er";
    auto model = parser.parseFromString(input);
    const auto& table = model.tables[0];

    ASSERT_EQ(table.indexes.size(), 2);

    EXPECT_EQ(table.indexes[0].name, "IX_users_email");
    EXPECT_FALSE(table.indexes[0].isUnique);
    ASSERT_EQ(table.indexes[0].columns.size(), 1);
    EXPECT_EQ(table.indexes[0].columns[0], "email");

    EXPECT_EQ(table.indexes[1].name, "UQ_users_name");
    EXPECT_TRUE(table.indexes[1].isUnique);
}

TEST_F(A5ERParserTest, TextFormatEmptyEntity) {
    const char* input = "# A5:ER FORMAT:19\n\n[Entity]\nPName=empty\nLName=Empty\nDEL\n";
    auto model = parser.parseFromString(input);

    ASSERT_EQ(model.tables.size(), 1);
    EXPECT_EQ(model.tables[0].columns.size(), 0);
}

TEST_F(A5ERParserTest, TextFormatQuotedCSVWithCommaInType) {
    const char* input = R"a5er(# A5:ER FORMAT:19

[Entity]
PName=test
LName=Test
Field="price","unit_price","DECIMAL(10,2)","NOT NULL","","",""
DEL
)a5er";
    auto model = parser.parseFromString(input);

    ASSERT_EQ(model.tables[0].columns.size(), 1);
    EXPECT_EQ(model.tables[0].columns[0].type, "DECIMAL(10,2)");
}

TEST_F(A5ERParserTest, TextFormatNoEntities) {
    auto model = parser.parseFromString("# A5:ER FORMAT:19\n");

    EXPECT_EQ(model.tables.size(), 0);
    EXPECT_EQ(model.relations.size(), 0);
}

// === DELなし形式（FORMAT:19 実ファイル互換）===

static const char* NO_DEL_TEXT_INPUT = R"a5er(# A5:ER FORMAT:19
# A5:ER ENCODING:UTF-8

[Entity]
PName=users
LName=User
Comment=User master
Page=MAIN
Left=100
Top=200
Field="id","id","INT","NOT NULL",0,"",""
Field="name","display_name","NVARCHAR(100)","NOT NULL","","",""
Field="email","email_addr","NVARCHAR(255)","NULL","","",""

[Entity]
PName=orders
LName=Order
Comment=
Page=MAIN
Left=400
Top=200
Field="id","id","INT","NOT NULL",0,"",""
Field="user_id","user_ref","INT","NOT NULL","","",""
Field="amount","price","DECIMAL(10,2)","NOT NULL","","0",""

[Relation]
Entity1=users
Entity2=orders
RelationType1=2
RelationType2=3
Fields1=id
Fields2=user_id
Dependence=0
)a5er";

TEST_F(A5ERParserTest, TextFormatNoDEL) {
    auto model = parser.parseFromString(NO_DEL_TEXT_INPUT);

    EXPECT_EQ(model.tables.size(), 2);
    EXPECT_EQ(model.relations.size(), 1);
    EXPECT_EQ(model.tables[0].name, "users");
    EXPECT_EQ(model.tables[1].name, "orders");
    EXPECT_EQ(model.relations[0].parentTable, "users");
    EXPECT_EQ(model.relations[0].cardinality, "1:N");
}

TEST_F(A5ERParserTest, TextFormatPKOrderZero) {
    const char* input = R"a5er(# A5:ER FORMAT:19

[Entity]
PName=test
LName=Test
Field="id","id","INT","NOT NULL",0,"",""
Field="name","name","NVARCHAR(100)","NULL","","",""
DEL
)a5er";
    auto model = parser.parseFromString(input);
    const auto& cols = model.tables[0].columns;

    ASSERT_EQ(cols.size(), 2);
    EXPECT_TRUE(cols[0].isPrimaryKey);   // PKorder=0 → PK
    EXPECT_FALSE(cols[1].isPrimaryKey);  // 空文字 → 非PK
}

TEST_F(A5ERParserTest, TextFormatUTF8BOM) {
    std::string input = "\xEF\xBB\xBF# A5:ER FORMAT:19\n\n[Entity]\nPName=bom_test\nLName=BOM\nDEL\n";
    auto model = parser.parseFromString(input);

    ASSERT_EQ(model.tables.size(), 1);
    EXPECT_EQ(model.tables[0].name, "bom_test");
}

TEST_F(A5ERParserTest, TextFormatCompositePK) {
    const char* input = R"a5er(# A5:ER FORMAT:19

[Entity]
PName=order_items
LName=OrderItem
Field="order_id","order_id","INT","NOT NULL",0,"",""
Field="item_id","item_id","INT","NOT NULL",1,"",""
Field="quantity","quantity","INT","NOT NULL","","",""
DEL
)a5er";
    auto model = parser.parseFromString(input);
    const auto& cols = model.tables[0].columns;

    ASSERT_EQ(cols.size(), 3);
    EXPECT_TRUE(cols[0].isPrimaryKey);   // PKorder=0
    EXPECT_TRUE(cols[1].isPrimaryKey);   // PKorder=1
    EXPECT_FALSE(cols[2].isPrimaryKey);  // 空文字 → 非PK
}

TEST_F(A5ERParserTest, TextFormatMixedDELAndNoDEL) {
    const char* input = R"a5er(# A5:ER FORMAT:19

[Entity]
PName=table_a
LName=A
Field="id","id","INT","NOT NULL",0,"",""
DEL

[Entity]
PName=table_b
LName=B
Field="id","id","INT","NOT NULL",0,"",""

[Entity]
PName=table_c
LName=C
Field="id","id","INT","NOT NULL",0,"",""
)a5er";
    auto model = parser.parseFromString(input);

    ASSERT_EQ(model.tables.size(), 3);
    EXPECT_EQ(model.tables[0].name, "table_a");
    EXPECT_EQ(model.tables[1].name, "table_b");
    EXPECT_EQ(model.tables[2].name, "table_c");
}

}  // namespace test
}  // namespace velocitydb
