#include "er_diagram_parser_factory.h"

#include "../interfaces/parsers/er_diagram_parser.h"
#include "a5er_parser.h"

#include <algorithm>
#include <stdexcept>

namespace velocitydb {

namespace {

std::string extractExtension(const std::string& filename) {
    auto dotPos = filename.rfind('.');
    if (dotPos == std::string::npos || dotPos == 0)
        return "";
    auto ext = filename.substr(dotPos);
    std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext;
}

}  // namespace

ERDiagramParserFactory::ERDiagramParserFactory() {
    m_parsers.push_back(std::make_unique<A5ERParser>());
}

ERDiagramParserFactory::~ERDiagramParserFactory() = default;
ERDiagramParserFactory::ERDiagramParserFactory(ERDiagramParserFactory&&) noexcept = default;
ERDiagramParserFactory& ERDiagramParserFactory::operator=(ERDiagramParserFactory&&) noexcept = default;

const IERDiagramParser& ERDiagramParserFactory::findParser(const std::string& content, const std::string& filename) const {
    auto ext = extractExtension(filename);

    // 1. Extension match + canParse
    for (const auto& p : m_parsers) {
        for (const auto& e : p->extensions()) {
            if (e == ext && p->canParse(content))
                return *p;
        }
    }

    // 2. canParse fallback
    for (const auto& p : m_parsers) {
        if (p->canParse(content))
            return *p;
    }

    throw std::runtime_error("No parser found for the given ER diagram format");
}

ERModel ERDiagramParserFactory::parse(const std::string& content, const std::string& filename) const {
    return findParser(content, filename).parse(content);
}

std::string ERDiagramParserFactory::generateDDL(const std::string& content, const std::string& filename, TargetDatabase target) const {
    const auto& parser = findParser(content, filename);
    auto model = parser.parse(content);
    return parser.generateDDL(model, target);
}

ERDiagramParserFactory::ParseResult ERDiagramParserFactory::parseWithDDL(const std::string& content, const std::string& filename, TargetDatabase target) const {
    const auto& parser = findParser(content, filename);
    auto model = parser.parse(content);
    auto ddl = parser.generateDDL(model, target);
    return {std::move(model), std::move(ddl)};
}

}  // namespace velocitydb
