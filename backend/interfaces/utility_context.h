#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

class IDatabaseContext;

/// Interface for utility operations (formatting, search, parsing, filtering)
class IUtilityContext {
public:
    virtual ~IUtilityContext() = default;

    [[nodiscard]] virtual std::string handleUppercaseKeywords(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleParseA5ER(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleParseA5ERContent(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleSearchObjects(IDatabaseContext& db, std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleQuickSearch(IDatabaseContext& db, std::string_view params) = 0;
};

}  // namespace velocitydb
