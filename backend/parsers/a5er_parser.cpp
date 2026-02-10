#include "a5er_parser.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

#include "pugixml.hpp"

namespace velocitydb {

bool A5ERParser::isTextFormat(const std::string& content) const {
    // XML形式は除外
    std::string_view sv(content);
    auto pos = sv.find_first_not_of(" \t\r\n");
    if (pos != std::string_view::npos) {
        if (sv.substr(pos).starts_with("<?xml") || sv[pos] == '<') {
            return false;
        }
    }
    return content.starts_with("# A5:ER") || content.find("[Entity]") != std::string::npos;
}

A5ERModel A5ERParser::parse(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return parseFromString(content);
}

A5ERModel A5ERParser::parseFromString(const std::string& content) {
    if (isTextFormat(content)) {
        return parseTextFormat(content);
    }
    return parseXmlFormat(content);
}

A5ERModel A5ERParser::parseXmlFormat(const std::string& content) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(content.c_str());

    if (!result) {
        throw std::runtime_error("Failed to parse A5:ER content: " + std::string(result.description()));
    }

    A5ERModel model;
    auto root = doc.child("A5ER");
    if (!root) {
        throw std::runtime_error("Invalid A5:ER format");
    }

    model.name = root.attribute("Name").as_string();
    model.databaseType = root.attribute("DatabaseType").as_string();

    for (auto entityNode : root.children("Entity")) {
        A5ERTable table;
        table.name = entityNode.attribute("Name").as_string();
        table.logicalName = entityNode.attribute("LogicalName").as_string();
        table.comment = entityNode.attribute("Comment").as_string();
        table.posX = entityNode.attribute("X").as_double();
        table.posY = entityNode.attribute("Y").as_double();

        for (auto attrNode : entityNode.children("Attribute")) {
            A5ERColumn col;
            col.name = attrNode.attribute("Name").as_string();
            col.logicalName = attrNode.attribute("LogicalName").as_string();
            col.type = attrNode.attribute("Type").as_string();
            col.size = attrNode.attribute("Size").as_int();
            col.scale = attrNode.attribute("Scale").as_int();
            col.nullable = attrNode.attribute("Nullable").as_bool(true);
            col.isPrimaryKey = attrNode.attribute("PK").as_bool(false);
            col.defaultValue = attrNode.attribute("Default").as_string();
            col.comment = attrNode.attribute("Comment").as_string();
            table.columns.push_back(col);
        }

        for (auto indexNode : entityNode.children("Index")) {
            A5ERIndex idx;
            idx.name = indexNode.attribute("Name").as_string();
            idx.isUnique = indexNode.attribute("Unique").as_bool(false);

            std::string cols = indexNode.attribute("Columns").as_string();
            std::istringstream iss(cols);
            std::string colName;
            while (std::getline(iss, colName, ',')) {
                idx.columns.push_back(colName);
            }
            table.indexes.push_back(idx);
        }

        model.tables.push_back(table);
    }

    for (auto relNode : root.children("Relation")) {
        A5ERRelation rel;
        rel.name = relNode.attribute("Name").as_string();
        rel.parentTable = relNode.attribute("ParentEntity").as_string();
        rel.childTable = relNode.attribute("ChildEntity").as_string();
        rel.parentColumn = relNode.attribute("ParentAttribute").as_string();
        rel.childColumn = relNode.attribute("ChildAttribute").as_string();
        rel.cardinality = relNode.attribute("Cardinality").as_string("1:N");
        model.relations.push_back(rel);
    }

    return model;
}

// === テキスト形式パース ===

std::vector<std::string> A5ERParser::parseQuotedCSV(const std::string& raw) {
    std::vector<std::string> result;
    size_t i = 0;
    const size_t len = raw.size();

    while (i < len) {
        // 先頭空白スキップ
        while (i < len && raw[i] == ' ')
            ++i;
        if (i >= len)
            break;

        if (raw[i] == '"') {
            ++i;  // 開始引用符スキップ
            std::string value;
            value.reserve(64);
            while (i < len) {
                if (raw[i] == '"') {
                    if (i + 1 < len && raw[i + 1] == '"') {
                        value += '"';
                        i += 2;
                    } else {
                        ++i;
                        break;
                    }
                } else {
                    value += raw[i];
                    ++i;
                }
            }
            result.push_back(std::move(value));
        } else {
            std::string value;
            while (i < len && raw[i] != ',') {
                value += raw[i];
                ++i;
            }
            result.push_back(std::move(value));
        }

        // カンマスキップ
        if (i < len && raw[i] == ',')
            ++i;
    }

    return result;
}

std::string A5ERParser::resolveCardinality(int type1, int type2) {
    auto isMany = [](int t) { return t == 3 || t == 4; };
    auto isOne = [](int t) { return t == 1 || t == 2; };

    if (isMany(type1) && isMany(type2))
        return "N:M";
    if (isOne(type1) && isMany(type2))
        return "1:N";
    if (isMany(type1) && isOne(type2))
        return "1:N";
    if (isOne(type1) && isOne(type2))
        return "1:1";
    return "1:N";
}

A5ERModel A5ERParser::parseTextFormat(const std::string& content) {
    A5ERModel model;
    std::istringstream stream(content);
    std::string line;

    struct Section {
        std::string type;
        std::vector<std::string> lines;
    };

    std::vector<Section> sections;
    std::string currentType;
    std::vector<std::string> currentLines;

    while (std::getline(stream, line)) {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // セクションヘッダ判定: [Entity], [Relation] 等
        if (line.size() >= 3 && line.front() == '[' && line.back() == ']') {
            currentType = line.substr(1, line.size() - 2);
            currentLines.clear();
            continue;
        }

        // セクション終端（Entity/Relation のみ保持、Header/Diagram等はスキップ）
        if (line == "DEL" && !currentType.empty()) {
            if (currentType == "Entity" || currentType == "Relation") {
                sections.push_back({currentType, std::move(currentLines)});
            }
            currentType.clear();
            currentLines.clear();
            continue;
        }

        if (!currentType.empty()) {
            currentLines.push_back(line);
        }
    }

    // props から値を取得するヘルパー
    auto getProp = [](const std::unordered_map<std::string, std::string>& props, const std::string& key) -> std::string {
        auto it = props.find(key);
        return it != props.end() ? it->second : "";
    };

    auto getPropInt = [&getProp](const std::unordered_map<std::string, std::string>& props, const std::string& key) -> int {
        auto val = getProp(props, key);
        int result = 0;
        if (!val.empty()) {
            std::from_chars(val.data(), val.data() + val.size(), result);
        }
        return result;
    };

    auto getPropDouble = [&getProp](const std::unordered_map<std::string, std::string>& props, const std::string& key) -> double {
        auto val = getProp(props, key);
        if (val.empty())
            return 0.0;
        double result = 0.0;
        std::from_chars(val.data(), val.data() + val.size(), result);
        return result;
    };

    // セクション解析
    for (const auto& section : sections) {
        if (section.type == "Entity") {
            A5ERTable table{};
            std::unordered_map<std::string, std::string> props;

            for (const auto& sline : section.lines) {
                if (sline.starts_with("Field=")) {
                    auto parts = parseQuotedCSV(sline.substr(6));
                    A5ERColumn col{};
                    if (!parts.empty())
                        col.name = parts[0];
                    if (parts.size() > 1)
                        col.logicalName = parts[1];
                    if (parts.size() > 2)
                        col.type = parts[2];
                    if (parts.size() > 3)
                        col.nullable = (parts[3] != "NOT NULL");
                    if (parts.size() > 4) {
                        int pkOrder = 0;
                        std::from_chars(parts[4].data(), parts[4].data() + parts[4].size(), pkOrder);
                        col.isPrimaryKey = (pkOrder > 0);
                    }
                    if (parts.size() > 5)
                        col.defaultValue = parts[5];
                    if (parts.size() > 6)
                        col.comment = parts[6];
                    col.size = 0;
                    col.scale = 0;
                    table.columns.push_back(std::move(col));
                } else if (sline.starts_with("Index=")) {
                    auto raw = sline.substr(6);
                    auto eqPos = raw.find('=');
                    if (eqPos != std::string::npos) {
                        A5ERIndex idx{};
                        idx.name = raw.substr(0, eqPos);
                        auto rest = raw.substr(eqPos + 1);
                        std::istringstream iss(rest);
                        std::string part;
                        bool first = true;
                        while (std::getline(iss, part, ',')) {
                            if (first) {
                                idx.isUnique = (part == "1");
                                first = false;
                            } else {
                                idx.columns.push_back(part);
                            }
                        }
                        table.indexes.push_back(std::move(idx));
                    }
                } else {
                    auto eqPos = sline.find('=');
                    if (eqPos != std::string::npos) {
                        props[sline.substr(0, eqPos)] = sline.substr(eqPos + 1);
                    }
                }
            }

            table.name = getProp(props, "PName");
            table.logicalName = getProp(props, "LName");
            table.comment = getProp(props, "Comment");
            table.posX = getPropDouble(props, "Left");
            table.posY = getPropDouble(props, "Top");

            model.tables.push_back(std::move(table));
        } else if (section.type == "Relation") {
            std::unordered_map<std::string, std::string> props;
            for (const auto& sline : section.lines) {
                auto eqPos = sline.find('=');
                if (eqPos != std::string::npos) {
                    props[sline.substr(0, eqPos)] = sline.substr(eqPos + 1);
                }
            }

            A5ERRelation rel{};
            rel.parentTable = getProp(props, "Entity1");
            rel.childTable = getProp(props, "Entity2");
            rel.parentColumn = getProp(props, "Fields1");
            rel.childColumn = getProp(props, "Fields2");
            rel.name = rel.parentTable + "_" + rel.childTable;

            int type1 = getPropInt(props, "RelationType1");
            int type2 = getPropInt(props, "RelationType2");
            rel.cardinality = resolveCardinality(type1, type2);

            model.relations.push_back(std::move(rel));
        }
    }

    return model;
}

std::string A5ERParser::generateDDL(const A5ERModel& model, const std::string& targetDatabase) {
    std::ostringstream ddl;

    ddl << "-- Generated from A5:ER model: " << model.name << "\n";
    ddl << "-- Target database: " << targetDatabase << "\n\n";

    // Generate table DDLs
    for (const auto& table : model.tables) {
        ddl << generateTableDDL(table, targetDatabase) << "\n\n";
    }

    // Generate foreign keys
    for (const auto& rel : model.relations) {
        ddl << "ALTER TABLE [" << rel.childTable << "]\n";
        ddl << "ADD CONSTRAINT [FK_" << rel.childTable << "_" << rel.parentTable << "]\n";
        ddl << "FOREIGN KEY ([" << rel.childColumn << "])\n";
        ddl << "REFERENCES [" << rel.parentTable << "] ([" << rel.parentColumn << "]);\n\n";
    }

    return ddl.str();
}

std::string A5ERParser::generateTableDDL(const A5ERTable& table, const std::string& targetDatabase) {
    std::ostringstream ddl;

    if (!table.comment.empty()) {
        ddl << "-- " << table.comment << "\n";
    }

    ddl << "CREATE TABLE [" << table.name << "] (\n";

    std::vector<std::string> pkColumns;

    for (size_t i = 0; i < table.columns.size(); ++i) {
        const auto& col = table.columns[i];

        ddl << "    [" << col.name << "] ";
        ddl << mapTypeToSQLServer(col.type, col.size, col.scale);

        if (!col.nullable) {
            ddl << " NOT NULL";
        }

        if (!col.defaultValue.empty()) {
            ddl << " DEFAULT " << col.defaultValue;
        }

        if (col.isPrimaryKey) {
            pkColumns.push_back(col.name);
        }

        if (i < table.columns.size() - 1 || !pkColumns.empty()) {
            ddl << ",";
        }

        if (!col.comment.empty()) {
            ddl << " -- " << col.comment;
        }

        ddl << "\n";
    }

    // Primary key constraint
    if (!pkColumns.empty()) {
        ddl << "    CONSTRAINT [PK_" << table.name << "] PRIMARY KEY (";
        for (size_t i = 0; i < pkColumns.size(); ++i) {
            ddl << "[" << pkColumns[i] << "]";
            if (i < pkColumns.size() - 1) {
                ddl << ", ";
            }
        }
        ddl << ")\n";
    }

    ddl << ");";

    // Create indexes
    for (const auto& idx : table.indexes) {
        ddl << "\n\nCREATE ";
        if (idx.isUnique) {
            ddl << "UNIQUE ";
        }
        ddl << "INDEX [" << idx.name << "] ON [" << table.name << "] (";
        for (size_t i = 0; i < idx.columns.size(); ++i) {
            ddl << "[" << idx.columns[i] << "]";
            if (i < idx.columns.size() - 1) {
                ddl << ", ";
            }
        }
        ddl << ");";
    }

    return ddl.str();
}

std::string A5ERParser::mapTypeToSQLServer(const std::string& a5erType, int size, int scale) const {
    // Map common A5:ER types to SQL Server types
    // Supports both English and Japanese type names from A5:ER
    if (a5erType == "VARCHAR" || a5erType == "string" || a5erType == "NVARCHAR") {
        if (size <= 0 || size > 8000) {
            return "NVARCHAR(MAX)";
        }
        return "NVARCHAR(" + std::to_string(size) + ")";
    }
    if (a5erType == "INT" || a5erType == "integer" || a5erType == "INTEGER") {
        return "INT";
    }
    if (a5erType == "BIGINT" || a5erType == "bigint") {
        return "BIGINT";
    }
    if (a5erType == "DECIMAL" || a5erType == "decimal" || a5erType == "NUMERIC") {
        return "DECIMAL(" + std::to_string(size) + "," + std::to_string(scale) + ")";
    }
    if (a5erType == "DATE" || a5erType == "date") {
        return "DATE";
    }
    if (a5erType == "DATETIME" || a5erType == "datetime" || a5erType == "TIMESTAMP") {
        return "DATETIME2";
    }
    if (a5erType == "BIT" || a5erType == "boolean" || a5erType == "BOOLEAN") {
        return "BIT";
    }
    if (a5erType == "TEXT" || a5erType == "text" || a5erType == "CLOB") {
        return "NVARCHAR(MAX)";
    }
    if (a5erType == "BLOB" || a5erType == "binary" || a5erType == "BINARY") {
        return "VARBINARY(MAX)";
    }

    // Default: return as-is
    return a5erType;
}

}  // namespace velocitydb
