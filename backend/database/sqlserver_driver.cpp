#include "sqlserver_driver.h"

#include "odbc_unicode.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace velocitydb {

SQLServerDriver::SQLServerDriver() {
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_env);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) [[unlikely]] {
        throw std::runtime_error("Failed to allocate ODBC environment handle");
    }

    ret = SQLSetEnvAttr(m_env, SQL_ATTR_ODBC_VERSION, toSqlPointer(SQL_OV_ODBC3), 0);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) [[unlikely]] {
        SQLFreeHandle(SQL_HANDLE_ENV, m_env);
        throw std::runtime_error("Failed to set ODBC version");
    }

    ret = SQLAllocHandle(SQL_HANDLE_DBC, m_env, &m_dbc);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) [[unlikely]] {
        SQLFreeHandle(SQL_HANDLE_ENV, m_env);
        throw std::runtime_error("Failed to allocate ODBC connection handle");
    }
}

SQLServerDriver::~SQLServerDriver() {
    disconnect();
    if (m_dbc != SQL_NULL_HDBC) {
        SQLFreeHandle(SQL_HANDLE_DBC, m_dbc);
    }
    if (m_env != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, m_env);
    }
}

bool SQLServerDriver::connect(std::string_view connectionString) {
    if (m_connected) {
        disconnect();
    }

    std::array<SQLWCHAR, 1024> outConnectionString{};
    SQLSMALLINT outConnectionStringLen = 0;

    // Set connection timeout to prevent indefinite hangs (e.g. dead SSH tunnels)
    constexpr SQLUINTEGER loginTimeout = 30;
    SQLSetConnectAttr(m_dbc, SQL_ATTR_LOGIN_TIMEOUT, toSqlPointer(loginTimeout), 0);
    SQLSetConnectAttr(m_dbc, SQL_ATTR_CONNECTION_TIMEOUT, toSqlPointer(loginTimeout), 0);

    auto wideConnStr = utf8ToWide(connectionString);
    SQLRETURN ret = SQLDriverConnectW(m_dbc, nullptr, toSqlWchar(wideConnStr.data()), SQL_NTS, outConnectionString.data(), static_cast<SQLSMALLINT>(outConnectionString.size()),
                                      &outConnectionStringLen, SQL_DRIVER_NOPROMPT);

    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) [[unlikely]] {
        storeODBCDiagnosticMessage(ret, SQL_HANDLE_DBC, m_dbc);
        return false;
    }

    m_connected.store(true, std::memory_order_release);
    return true;
}

void SQLServerDriver::disconnect() {
    std::lock_guard lock(m_executeMutex);
    auto stmt = m_stmt.exchange(SQL_NULL_HSTMT, std::memory_order_acq_rel);
    if (stmt != SQL_NULL_HSTMT) {
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }
    if (m_connected.exchange(false, std::memory_order_acq_rel)) {
        SQLDisconnect(m_dbc);
    }
}

std::string SQLServerDriver::convertSQLTypeToDisplayName(SQLSMALLINT dataType) {
    switch (dataType) {
        case SQL_CHAR:
        case SQL_VARCHAR:
        case SQL_LONGVARCHAR:
            return "VARCHAR";
        case SQL_WCHAR:
        case SQL_WVARCHAR:
        case SQL_WLONGVARCHAR:
            return "NVARCHAR";
        case SQL_INTEGER:
            return "INT";
        case SQL_BIGINT:
            return "BIGINT";
        case SQL_SMALLINT:
            return "SMALLINT";
        case SQL_FLOAT:
        case SQL_DOUBLE:
            return "FLOAT";
        case SQL_DECIMAL:
        case SQL_NUMERIC:
            return "DECIMAL";
        case SQL_TYPE_DATE:
            return "DATE";
        case SQL_TYPE_TIME:
            return "TIME";
        case SQL_TYPE_TIMESTAMP:
            return "DATETIME";
        case SQL_BIT:
            return "BIT";
        default:
            return "UNKNOWN";
    }
}

ResultSet SQLServerDriver::execute(std::string_view sql) {
    std::lock_guard lock(m_executeMutex);
    ResultSet result;

    if (!m_connected.load(std::memory_order_acquire)) [[unlikely]] {
        throw std::runtime_error("Not connected to database");
    }

    const auto startTime = std::chrono::high_resolution_clock::now();

    auto oldStmt = m_stmt.exchange(SQL_NULL_HSTMT, std::memory_order_acq_rel);
    if (oldStmt != SQL_NULL_HSTMT) {
        SQLFreeHandle(SQL_HANDLE_STMT, oldStmt);
    }

    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, m_dbc, &stmt);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) [[unlikely]] {
        m_stmt.store(SQL_NULL_HSTMT, std::memory_order_release);
        storeODBCDiagnosticMessage(ret, SQL_HANDLE_DBC, m_dbc);
        throw std::runtime_error(m_lastError);
    }

    // Publish new stmt so cancel() can see it immediately
    m_stmt.store(stmt, std::memory_order_release);

    // Set query timeout to prevent indefinite hangs
    constexpr SQLULEN queryTimeout = 300;  // 5 minutes
    SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT, toSqlPointer(queryTimeout), 0);

    auto wideSql = utf8ToWide(sql);
    ret = SQLExecDirectW(stmt, toSqlWchar(wideSql.data()), SQL_NTS);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO && ret != SQL_NO_DATA) [[unlikely]] {
        storeODBCDiagnosticMessage(ret, SQL_HANDLE_STMT, stmt);
        throw std::runtime_error(m_lastError);
    }

    SQLSMALLINT numCols = 0;
    ret = SQLNumResultCols(stmt, &numCols);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) [[unlikely]] {
        storeODBCDiagnosticMessage(ret, SQL_HANDLE_STMT, stmt);
        throw std::runtime_error(std::string("Failed to get column count: ") + m_lastError);
    }

    result.columns.reserve(static_cast<size_t>(numCols));
    for (SQLSMALLINT i = 1; i <= numCols; ++i) {
        std::array<SQLWCHAR, 256> colName{};
        SQLSMALLINT colNameLen = 0;
        SQLSMALLINT dataType = 0;
        SQLULEN colSize = 0;
        SQLSMALLINT decimalDigits = 0;
        SQLSMALLINT nullable = 0;

        ret = SQLDescribeColW(stmt, i, colName.data(), static_cast<SQLSMALLINT>(colName.size()), &colNameLen, &dataType, &colSize, &decimalDigits, &nullable);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) [[unlikely]] {
            storeODBCDiagnosticMessage(ret, SQL_HANDLE_STMT, stmt);
            throw std::runtime_error(std::string("Failed to describe column: ") + m_lastError);
        }

        // Ensure colNameLen doesn't exceed buffer size (SQLDescribeColW may truncate)
        // Use (std::min) to avoid Windows min macro interference
        colNameLen = (std::min)(colNameLen, static_cast<SQLSMALLINT>(colName.size() - 1));

        auto columnName = sqlWcharToUtf8(colName.data(), static_cast<size_t>(colNameLen));

        // If column name is empty, use a default name (e.g., "Column1", "Column2")
        if (columnName.empty()) {
            columnName = std::format("Column{}", i);
        }

        result.columns.push_back({.name = columnName, .type = convertSQLTypeToDisplayName(dataType), .size = static_cast<int>(colSize), .nullable = (nullable == SQL_NULLABLE), .isPrimaryKey = false});
    }

    // Dynamic buffer for large column values (Unicode - SQLWCHAR is 2 bytes)
    constexpr size_t INITIAL_BUFFER_CHARS = 4096;
    std::vector<SQLWCHAR> buffer(INITIAL_BUFFER_CHARS);
    SQLLEN indicator = 0;

    while ((ret = SQLFetch(stmt)) == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        ResultRow row;
        row.values.reserve(static_cast<size_t>(numCols));

        for (SQLSMALLINT i = 1; i <= numCols; ++i) {
            // Use SQL_C_WCHAR to get Unicode data
            ret = SQLGetData(stmt, i, SQL_C_WCHAR, buffer.data(), buffer.size() * sizeof(SQLWCHAR), &indicator);
            if (indicator == SQL_NULL_DATA) {
                row.values.emplace_back();
            } else if (ret == SQL_SUCCESS_WITH_INFO && indicator > static_cast<SQLLEN>((buffer.size() - 1) * sizeof(SQLWCHAR))) {
                // Data was truncated, need a larger buffer
                // indicator is in bytes for SQL_C_WCHAR
                size_t requiredChars = (static_cast<size_t>(indicator) / sizeof(SQLWCHAR)) + 1;
                std::vector<SQLWCHAR> largeBuffer(requiredChars);
                // Copy already retrieved data
                size_t alreadyReadChars = buffer.size() - 1;
                std::copy(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(alreadyReadChars), largeBuffer.begin());
                // Get remaining data
                SQLLEN remainingIndicator = 0;
                ret = SQLGetData(stmt, i, SQL_C_WCHAR, largeBuffer.data() + alreadyReadChars, (requiredChars - alreadyReadChars) * sizeof(SQLWCHAR), &remainingIndicator);
                // Find actual string length
                size_t strLen = 0;
                for (size_t j = 0; j < largeBuffer.size() && largeBuffer[j] != 0; ++j) {
                    strLen = j + 1;
                }
                row.values.emplace_back(sqlWcharToUtf8(largeBuffer.data(), strLen));
            } else if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
                // Find actual string length
                size_t strLen = 0;
                for (size_t j = 0; j < buffer.size() && buffer[j] != 0; ++j) {
                    strLen = j + 1;
                }
                row.values.emplace_back(sqlWcharToUtf8(buffer.data(), strLen));
            } else {
                // Error getting data - add empty value and continue
                row.values.emplace_back();
            }
        }
        result.rows.push_back(std::move(row));
    }

    SQLLEN rowCount = 0;
    ret = SQLRowCount(stmt, &rowCount);
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        result.affectedRows = rowCount;
    } else {
        result.affectedRows = 0;
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    result.executionTimeMs = static_cast<double>(duration.count()) / 1000.0;

    return result;
}

void SQLServerDriver::cancel() {
    auto stmt = m_stmt.load(std::memory_order_acquire);
    if (stmt != SQL_NULL_HSTMT) {
        SQLCancel(stmt);
    }
}

std::string SQLServerDriver::getLastError() const {
    std::lock_guard lock(m_executeMutex);
    return m_lastError;
}

void SQLServerDriver::storeODBCDiagnosticMessage(SQLRETURN returnCode, SQLSMALLINT odbcHandleType, SQLHANDLE odbcHandle) {
    if (returnCode == SQL_SUCCESS || returnCode == SQL_SUCCESS_WITH_INFO) [[likely]] {
        return;
    }

    std::array<SQLWCHAR, 6> sqlState{};
    SQLINTEGER nativeErrorCode = 0;
    std::array<SQLWCHAR, 1024> diagnosticMessage{};
    SQLSMALLINT messageLength = 0;

    SQLGetDiagRecW(odbcHandleType, odbcHandle, 1, sqlState.data(), &nativeErrorCode, diagnosticMessage.data(), static_cast<SQLSMALLINT>(diagnosticMessage.size()), &messageLength);

    // Convert UTF-16 (SQLWCHAR) to UTF-8 (std::string)
    m_lastError = sqlWcharToUtf8(diagnosticMessage.data(), messageLength);
}

}  // namespace velocitydb
