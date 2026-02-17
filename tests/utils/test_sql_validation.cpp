#include <gtest/gtest.h>

#include "utils/sql_validation.h"

namespace velocitydb {
namespace {

// ─── detail::isUnicodeAlphaNumeric ──────────────────────────────────

TEST(IsUnicodeAlphaNumeric, AsciiLettersAndDigits) {
    EXPECT_TRUE(detail::isUnicodeAlphaNumeric(L'A'));
    EXPECT_TRUE(detail::isUnicodeAlphaNumeric(L'z'));
    EXPECT_TRUE(detail::isUnicodeAlphaNumeric(L'0'));
    EXPECT_TRUE(detail::isUnicodeAlphaNumeric(L'9'));
}

TEST(IsUnicodeAlphaNumeric, SpecialCharsRejected) {
    EXPECT_FALSE(detail::isUnicodeAlphaNumeric(L' '));
    EXPECT_FALSE(detail::isUnicodeAlphaNumeric(L';'));
    EXPECT_FALSE(detail::isUnicodeAlphaNumeric(L'-'));
    EXPECT_FALSE(detail::isUnicodeAlphaNumeric(L'\''));
}

TEST(IsUnicodeAlphaNumeric, UnicodeJapanese) {
    EXPECT_TRUE(detail::isUnicodeAlphaNumeric(L'\u5F97'));   // 得
    EXPECT_TRUE(detail::isUnicodeAlphaNumeric(L'\u610F'));   // 意
    EXPECT_TRUE(detail::isUnicodeAlphaNumeric(L'\u5148'));   // 先
}

// ─── isValidIdentifier: basic ───────────────────────────────────────

TEST(IsValidIdentifier, EmptyStringRejected) {
    EXPECT_FALSE(isValidIdentifier(""));
}

TEST(IsValidIdentifier, SimpleAsciiName) {
    EXPECT_TRUE(isValidIdentifier("Users"));
    EXPECT_TRUE(isValidIdentifier("my_table"));
    EXPECT_TRUE(isValidIdentifier("Table1"));
}

TEST(IsValidIdentifier, DotSeparatedSchemaTable) {
    EXPECT_TRUE(isValidIdentifier("dbo.Users"));
    EXPECT_TRUE(isValidIdentifier("schema1.table2"));
}

TEST(IsValidIdentifier, UnderscoreAllowed) {
    EXPECT_TRUE(isValidIdentifier("_leading"));
    EXPECT_TRUE(isValidIdentifier("a_b_c"));
}

// ─── isValidIdentifier: bracket notation ────────────────────────────

TEST(IsValidIdentifier, BracketedIdentifier) {
    EXPECT_TRUE(isValidIdentifier("[My Table]"));
    EXPECT_TRUE(isValidIdentifier("[Column With Spaces]"));
}

TEST(IsValidIdentifier, BracketedWithSchema) {
    EXPECT_TRUE(isValidIdentifier("[dbo].[My Table]"));
}

TEST(IsValidIdentifier, UnclosedBracketRejected) {
    EXPECT_FALSE(isValidIdentifier("[unclosed"));
    EXPECT_FALSE(isValidIdentifier("dbo.[unclosed"));
}

TEST(IsValidIdentifier, EmptyBracketRejected) {
    EXPECT_FALSE(isValidIdentifier("[]"));
    EXPECT_FALSE(isValidIdentifier("dbo.[]"));
}

// ─── isValidIdentifier: ]] escape ───────────────────────────────────

TEST(IsValidIdentifier, EscapedCloseBracket) {
    EXPECT_TRUE(isValidIdentifier("[col]]name]"));
}

TEST(IsValidIdentifier, MultipleEscapedBrackets) {
    EXPECT_TRUE(isValidIdentifier("[a]]b]]c]"));
}

TEST(IsValidIdentifier, TrailingDoubleCloseBracket) {
    // [name]]] → [name]] (escaped ]) + ] (close) = identifier "name]"
    EXPECT_TRUE(isValidIdentifier("[name]]]"));
}

TEST(IsValidIdentifier, EscapedBracketWithSchema) {
    EXPECT_TRUE(isValidIdentifier("dbo.[col]]name]"));
}

TEST(IsValidIdentifier, OnlyEscapedBracketContent) {
    // []]  → ]] escaped ] のみ、閉じ bracket なし → invalid
    EXPECT_FALSE(isValidIdentifier("[]]"));
}

TEST(IsValidIdentifier, OnlyEscapedBracketThenClose) {
    // []]] → ]] escaped ] + ] close = identifier "]"
    EXPECT_TRUE(isValidIdentifier("[]]]"));
}

// ─── isValidIdentifier: dot edge cases ──────────────────────────────

TEST(IsValidIdentifier, LeadingDotPassesValidation) {
    EXPECT_TRUE(isValidIdentifier(".Users"));
}

TEST(IsValidIdentifier, TrailingDotPassesValidation) {
    EXPECT_TRUE(isValidIdentifier("Users."));
}

TEST(IsValidIdentifier, ConsecutiveDotsPassesValidation) {
    EXPECT_TRUE(isValidIdentifier("a..b"));
}

// ─── isValidIdentifier: Unicode ─────────────────────────────────────

TEST(IsValidIdentifier, JapaneseTableName) {
    EXPECT_TRUE(isValidIdentifier("\xe5\xbe\x97\xe6\x84\x8f\xe5\x85\x88"));  // 得意先
}

TEST(IsValidIdentifier, BracketedJapanese) {
    EXPECT_TRUE(isValidIdentifier("[\xe5\xbe\x97\xe6\x84\x8f\xe5\x85\x88 \xe3\x83\x86\xe3\x83\xbc\xe3\x83\x96\xe3\x83\xab]"));  // [得意先 テーブル]
}

// ─── isValidIdentifier: length limit ────────────────────────────────

TEST(IsValidIdentifier, MaxLengthAccepted) {
    std::string name(128, 'a');
    EXPECT_TRUE(isValidIdentifier(name));
}

TEST(IsValidIdentifier, ExceedsMaxLengthRejected) {
    std::string name(129, 'a');
    EXPECT_FALSE(isValidIdentifier(name));
}

// ─── isValidIdentifier: SQL injection patterns ──────────────────────

TEST(IsValidIdentifier, SqlInjectionRejected) {
    EXPECT_FALSE(isValidIdentifier("Users; DROP TABLE--"));
    EXPECT_FALSE(isValidIdentifier("a' OR '1'='1"));
    EXPECT_FALSE(isValidIdentifier("table\nname"));
}

TEST(IsValidIdentifier, DangerousCharsOutsideBracketRejected) {
    EXPECT_FALSE(isValidIdentifier("col;name"));
    EXPECT_FALSE(isValidIdentifier("col'name"));
    EXPECT_FALSE(isValidIdentifier("col name"));
}

// ─── unquoteBracketIdentifier ───────────────────────────────────────

TEST(UnquoteBracketIdentifier, NoBrackets) {
    EXPECT_EQ(unquoteBracketIdentifier("Users"), "Users");
    EXPECT_EQ(unquoteBracketIdentifier("dbo"), "dbo");
}

TEST(UnquoteBracketIdentifier, SimpleBrackets) {
    EXPECT_EQ(unquoteBracketIdentifier("[My Table]"), "My Table");
    EXPECT_EQ(unquoteBracketIdentifier("[Column]"), "Column");
}

TEST(UnquoteBracketIdentifier, EscapedCloseBracket) {
    EXPECT_EQ(unquoteBracketIdentifier("[col]]name]"), "col]name");
}

TEST(UnquoteBracketIdentifier, MultipleEscapes) {
    EXPECT_EQ(unquoteBracketIdentifier("[a]]b]]c]"), "a]b]c");
}

TEST(UnquoteBracketIdentifier, OnlyEscapedContent) {
    // []]] の外側 [ と最後の ] を剥がすと ]] → unescape で ]
    EXPECT_EQ(unquoteBracketIdentifier("[]]]"), "]");
}

TEST(UnquoteBracketIdentifier, EmptyString) {
    EXPECT_EQ(unquoteBracketIdentifier(""), "");
}

TEST(UnquoteBracketIdentifier, MismatchedBracketsUnchanged) {
    EXPECT_EQ(unquoteBracketIdentifier("[noclose"), "[noclose");
    EXPECT_EQ(unquoteBracketIdentifier("noopen]"), "noopen]");
}

TEST(UnquoteBracketIdentifier, MultiPartBracketed) {
    EXPECT_EQ(unquoteBracketIdentifier("[dbo].[My Table]"), "dbo.My Table");
    EXPECT_EQ(unquoteBracketIdentifier("[schema].[col]]name]"), "schema.col]name");
}

TEST(UnquoteBracketIdentifier, MultiPartMixed) {
    EXPECT_EQ(unquoteBracketIdentifier("dbo.[My Table]"), "dbo.My Table");
    EXPECT_EQ(unquoteBracketIdentifier("[dbo].Users"), "dbo.Users");
}

TEST(UnquoteBracketIdentifier, DotInsideBracketsPreserved) {
    EXPECT_EQ(unquoteBracketIdentifier("[dbo.name]"), "dbo.name");
}

}  // namespace
}  // namespace velocitydb
