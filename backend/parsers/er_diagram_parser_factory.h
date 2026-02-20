#pragma once

#include "interfaces/parsers/er_model.h"

#include <memory>
#include <string>
#include <vector>

namespace velocitydb {

class IERDiagramParser;

class ERDiagramParserFactory {
public:
    ERDiagramParserFactory();
    ~ERDiagramParserFactory();

    ERDiagramParserFactory(const ERDiagramParserFactory&) = delete;
    ERDiagramParserFactory& operator=(const ERDiagramParserFactory&) = delete;
    ERDiagramParserFactory(ERDiagramParserFactory&&) noexcept;
    ERDiagramParserFactory& operator=(ERDiagramParserFactory&&) noexcept;

    [[nodiscard]] ERModel parse(const std::string& content, const std::string& filename = "") const;

    [[nodiscard]] std::string generateDDL(const std::string& content, const std::string& filename = "", TargetDatabase target = TargetDatabase::SQLServer) const;

    /// Parse and generate DDL in one findParser call (avoids double lookup)
    struct ParseResult {
        ERModel model;
        std::string ddl;
    };
    [[nodiscard]] ParseResult parseWithDDL(const std::string& content, const std::string& filename = "", TargetDatabase target = TargetDatabase::SQLServer) const;

private:
    std::vector<std::unique_ptr<IERDiagramParser>> m_parsers;

    /// @throws std::runtime_error if no parser matches the given content/filename
    [[nodiscard]] const IERDiagramParser& findParser(const std::string& content, const std::string& filename) const;
};

}  // namespace velocitydb
