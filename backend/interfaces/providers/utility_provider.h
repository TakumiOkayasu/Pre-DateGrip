#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for utility operations (formatting, parsing)
class IUtilityProvider {
public:
    virtual ~IUtilityProvider() = default;

    [[nodiscard]] virtual std::string uppercaseKeywords(std::string_view params) = 0;
    [[nodiscard]] virtual std::string parseERDiagram(std::string_view params) = 0;
};

}  // namespace velocitydb
