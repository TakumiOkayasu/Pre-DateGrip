#pragma once

#include "../interfaces/providers/utility_provider.h"

#include <memory>
#include <string>
#include <string_view>

namespace velocitydb {

class SQLFormatter;
class A5ERParser;

/// Provider for utility operations (formatting, parsing)
class UtilityProvider : public IUtilityProvider {
public:
    UtilityProvider();
    ~UtilityProvider() override;

    UtilityProvider(const UtilityProvider&) = delete;
    UtilityProvider& operator=(const UtilityProvider&) = delete;
    UtilityProvider(UtilityProvider&&) noexcept;
    UtilityProvider& operator=(UtilityProvider&&) noexcept;

    [[nodiscard]] std::string handleUppercaseKeywords(std::string_view params) override;
    [[nodiscard]] std::string handleParseA5ER(std::string_view params) override;
    [[nodiscard]] std::string handleParseA5ERContent(std::string_view params) override;

    [[nodiscard]] SQLFormatter& sqlFormatter() { return *m_sqlFormatter; }
    [[nodiscard]] const SQLFormatter& sqlFormatter() const { return *m_sqlFormatter; }
    [[nodiscard]] A5ERParser& a5erParser() { return *m_a5erParser; }
    [[nodiscard]] const A5ERParser& a5erParser() const { return *m_a5erParser; }

private:
    std::unique_ptr<SQLFormatter> m_sqlFormatter;
    std::unique_ptr<A5ERParser> m_a5erParser;
};

}  // namespace velocitydb
