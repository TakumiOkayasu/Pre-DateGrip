#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for SQL formatting capability
class IFormattable {
public:
    virtual ~IFormattable() = default;

    IFormattable(const IFormattable&) = delete;
    IFormattable& operator=(const IFormattable&) = delete;

    /// Format SQL query with proper indentation and line breaks
    [[nodiscard]] virtual std::string format(std::string_view sql) = 0;

    /// Convert SQL keywords to uppercase
    [[nodiscard]] virtual std::string uppercaseKeywords(std::string_view sql) = 0;

protected:
    IFormattable() = default;
    IFormattable(IFormattable&&) = default;
    IFormattable& operator=(IFormattable&&) = default;
};

}  // namespace velocitydb
