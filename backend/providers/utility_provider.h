#pragma once

#include "../interfaces/providers/utility_provider.h"

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class SQLFormatter;
class ERDiagramParserFactory;

/// Provider for utility operations (formatting, parsing)
class UtilityProvider : public IUtilityProvider {
public:
    UtilityProvider();
    ~UtilityProvider() override;

    UtilityProvider(const UtilityProvider&) = delete;
    UtilityProvider& operator=(const UtilityProvider&) = delete;
    UtilityProvider(UtilityProvider&&) noexcept;
    UtilityProvider& operator=(UtilityProvider&&) noexcept;

    [[nodiscard]] std::string uppercaseKeywords(std::string_view params) override;
    [[nodiscard]] std::string parseERDiagram(std::string_view params) override;

    [[nodiscard]] SQLFormatter& sqlFormatter() { return *m_sqlFormatter; }
    [[nodiscard]] const SQLFormatter& sqlFormatter() const { return *m_sqlFormatter; }

private:
    std::unique_ptr<SQLFormatter> m_sqlFormatter;
    std::unique_ptr<ERDiagramParserFactory> m_parserFactory;
};

}  // namespace velocitydb
