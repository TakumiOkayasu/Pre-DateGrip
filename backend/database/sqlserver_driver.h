#pragma once

#include "driver_interface.h"

#include <Windows.h>
#include <sql.h>
#include <sqlext.h>

#include <atomic>
#include <expected>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace velocitydb {

struct ColumnInfo {
    std::string name;
    std::string type;
    int size = 0;
    bool nullable = true;
    bool isPrimaryKey = false;
    std::string comment;
};

struct ResultRow {
    std::vector<std::string> values;
};

struct ResultSet {
    std::vector<ColumnInfo> columns;
    std::vector<ResultRow> rows;
    int64_t affectedRows = 0;
    double executionTimeMs = 0.0;
};

class SQLServerDriver : public IDatabaseDriver {
public:
    SQLServerDriver();
    ~SQLServerDriver() override;

    SQLServerDriver(const SQLServerDriver&) = delete;
    SQLServerDriver& operator=(const SQLServerDriver&) = delete;
    SQLServerDriver(SQLServerDriver&&) = delete;
    SQLServerDriver& operator=(SQLServerDriver&&) = delete;

    // IDatabaseDriver interface
    [[nodiscard]] bool connect(std::string_view connectionString) override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const noexcept override { return m_connected.load(std::memory_order_acquire); }

    [[nodiscard]] ResultSet execute(std::string_view sql) override;
    void cancel() override;

    [[nodiscard]] std::string getLastError() const override;
    [[nodiscard]] DriverType getType() const noexcept override { return DriverType::SQLServer; }

private:
    void storeODBCDiagnosticMessage(SQLRETURN returnCode, SQLSMALLINT odbcHandleType, SQLHANDLE odbcHandle);
    [[nodiscard]] static std::string convertSQLTypeToDisplayName(SQLSMALLINT dataType);

    SQLHENV m_env = SQL_NULL_HENV;
    SQLHDBC m_dbc = SQL_NULL_HDBC;
    std::atomic<SQLHSTMT> m_stmt{SQL_NULL_HSTMT};
    std::atomic<bool> m_connected{false};
    std::string m_lastError;
    mutable std::mutex m_executeMutex;  // Serializes concurrent execute()/disconnect()/getLastError() calls
};

}  // namespace velocitydb
