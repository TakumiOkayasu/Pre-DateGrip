#pragma once

#include "../interfaces/utility_context.h"

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace velocitydb {

class SQLFormatter;
class GlobalSearch;
class A5ERParser;
class SIMDFilter;
struct ResultSet;

/// Context for utility operations (formatting, search, parsing, filtering)
class UtilityContext : public IUtilityContext {
public:
    UtilityContext();
    ~UtilityContext() override;

    UtilityContext(const UtilityContext&) = delete;
    UtilityContext& operator=(const UtilityContext&) = delete;
    UtilityContext(UtilityContext&&) noexcept;
    UtilityContext& operator=(UtilityContext&&) noexcept;

    // IUtilityContext implementation (IPC handle methods)
    [[nodiscard]] std::string handleUppercaseKeywords(std::string_view params) override;
    [[nodiscard]] std::string handleParseA5ER(std::string_view params) override;
    [[nodiscard]] std::string handleParseA5ERContent(std::string_view params) override;
    [[nodiscard]] std::string handleSearchObjects(IDatabaseContext& db, std::string_view params) override;
    [[nodiscard]] std::string handleQuickSearch(IDatabaseContext& db, std::string_view params) override;

    // Direct access to services (for legacy/migration use)
    [[nodiscard]] SQLFormatter& sqlFormatter() { return *m_sqlFormatter; }
    [[nodiscard]] const SQLFormatter& sqlFormatter() const { return *m_sqlFormatter; }
    [[nodiscard]] A5ERParser& a5erParser() { return *m_a5erParser; }
    [[nodiscard]] const A5ERParser& a5erParser() const { return *m_a5erParser; }
    [[nodiscard]] SIMDFilter& simdFilter() { return *m_simdFilter; }
    [[nodiscard]] const SIMDFilter& simdFilter() const { return *m_simdFilter; }
    [[nodiscard]] GlobalSearch& globalSearch() { return *m_globalSearch; }
    [[nodiscard]] const GlobalSearch& globalSearch() const { return *m_globalSearch; }

private:
    std::unique_ptr<SQLFormatter> m_sqlFormatter;
    std::unique_ptr<GlobalSearch> m_globalSearch;
    std::unique_ptr<A5ERParser> m_a5erParser;
    std::unique_ptr<SIMDFilter> m_simdFilter;
};

}  // namespace velocitydb
