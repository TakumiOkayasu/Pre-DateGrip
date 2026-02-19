#pragma once

#include "er_model.h"

#include <string>
#include <vector>

namespace velocitydb {

class IERDiagramParser {
public:
    virtual ~IERDiagramParser() = default;

    [[nodiscard]] virtual std::vector<std::string> extensions() const = 0;
    [[nodiscard]] virtual bool canParse(const std::string& content) const = 0;
    [[nodiscard]] virtual ERModel parse(const std::string& content) const = 0;

    [[nodiscard]] virtual std::string generateDDL(const ERModel& model, TargetDatabase target = TargetDatabase::SQLServer) const = 0;
};

}  // namespace velocitydb
