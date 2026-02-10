#pragma once

#include "../interfaces/io_context.h"

#include <atomic>
#include <mutex>
#include <string>
#include <string_view>

namespace velocitydb {

/// Concrete implementation of IIOContext for file/log/bookmark operations
class IOContext : public IIOContext {
public:
    IOContext() = default;
    ~IOContext() override = default;

    IOContext(const IOContext&) = delete;
    IOContext& operator=(const IOContext&) = delete;
    IOContext(IOContext&&) = delete;
    IOContext& operator=(IOContext&&) = delete;

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
