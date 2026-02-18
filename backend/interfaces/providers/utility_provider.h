#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for utility operations (formatting, parsing)
class IUtilityProvider {
public:
    virtual ~IUtilityProvider() = default;

    [[nodiscard]] virtual std::string handleUppercaseKeywords(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleParseA5ER(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleParseA5ERContent(std::string_view params) = 0;
};

}  // namespace velocitydb
