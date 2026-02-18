#pragma once

#include "../interfaces/providers/io_provider.h"

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>

namespace velocitydb {

/// Provider for I/O operations (logging, file, bookmarks)
class IOProvider : public IIOProvider {
public:
    IOProvider() = default;
    ~IOProvider() override = default;

    IOProvider(const IOProvider&) = delete;
    IOProvider& operator=(const IOProvider&) = delete;
    IOProvider(IOProvider&&) = delete;
    IOProvider& operator=(IOProvider&&) = delete;

    [[nodiscard]] std::string handleWriteFrontendLog(std::string_view params) override;
    [[nodiscard]] std::string handleSaveQueryToFile(std::string_view params) override;
    [[nodiscard]] std::string handleLoadQueryFromFile(std::string_view params) override;
    [[nodiscard]] std::string handleBrowseFile(std::string_view params) override;
    [[nodiscard]] std::string handleGetBookmarks(std::string_view params) override;
    [[nodiscard]] std::string handleSaveBookmark(std::string_view params) override;
    [[nodiscard]] std::string handleDeleteBookmark(std::string_view params) override;

private:
    std::atomic<bool> m_firstLogWrite{true};
    std::mutex m_logMutex;
};

}  // namespace velocitydb
