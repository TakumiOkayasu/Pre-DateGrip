#pragma once

#include <string>
#include <vector>

namespace velocitydb {

enum class TargetDatabase { SQLServer, PostgreSQL, MySQL };

struct ERModelColumn {
    std::string name;
    std::string logicalName;
    std::string type;
    int size = 0;
    int scale = 0;
    bool nullable = true;
    bool isPrimaryKey = false;
    std::string defaultValue;
    std::string comment;
    std::string color;  // CSS #RRGGBB or empty
};

struct ERModelIndex {
    std::string name;
    std::vector<std::string> columns;
    bool isUnique = false;
};

struct ERModelTable {
    std::string name;
    std::string logicalName;
    std::string comment;
    std::string page;
    std::vector<ERModelColumn> columns;
    std::vector<ERModelIndex> indexes;
    double posX = 0;
    double posY = 0;
    std::string color;    // Foreground CSS #RRGGBB or empty
    std::string bkColor;  // Background CSS #RRGGBB or empty
};

struct ERModelRelation {
    std::string name;
    std::string parentTable;
    std::string childTable;
    std::string parentColumn;
    std::string childColumn;
    std::string cardinality;  // "1:1", "1:N", "N:M"
};

struct ERModelShape {
    std::string shapeType;  // "rectangle", "roundrect" (normalized lowercase)
    std::string text;
    std::string fillColor;  // CSS #RRGGBB or empty
    std::string fontColor;  // CSS #RRGGBB or empty
    int fillAlpha = 255;
    int fontSize = 9;
    double left = 0;
    double top = 0;
    double width = 0;
    double height = 0;
    std::string page;
};

struct ERModel {
    std::string name;
    std::string databaseType;
    std::vector<ERModelTable> tables;
    std::vector<ERModelRelation> relations;
    std::vector<ERModelShape> shapes;
};

}  // namespace velocitydb
