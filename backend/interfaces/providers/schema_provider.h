#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for database schema inspection
class ISchemaProvider {
public:
    virtual ~ISchemaProvider() = default;

    [[nodiscard]] virtual std::string handleGetDatabases(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTables(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetColumns(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetIndexes(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetConstraints(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetForeignKeys(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetReferencingForeignKeys(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTriggers(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTableMetadata(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetTableDDL(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetExecutionPlan(std::string_view params) = 0;
};

}  // namespace velocitydb
