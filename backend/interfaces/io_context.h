#pragma once

#include <string>
#include <string_view>

namespace velocitydb {

/// Interface for I/O operations (logging, file, bookmarks)
class IIOContext {
public:
    virtual ~IIOContext() = default;

    [[nodiscard]] virtual std::string handleWriteFrontendLog(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleSaveQueryToFile(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleLoadQueryFromFile(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleBrowseFile(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleGetBookmarks(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleSaveBookmark(std::string_view params) = 0;
    [[nodiscard]] virtual std::string handleDeleteBookmark(std::string_view params) = 0;
};

}  // namespace velocitydb
