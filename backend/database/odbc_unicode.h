#pragma once

#include "../utils/encoding.h"

#include <sql.h>

#include <concepts>
#include <string>
#include <string_view>

namespace velocitydb {

static_assert(sizeof(SQLWCHAR) == sizeof(wchar_t), "SQLWCHAR and wchar_t must have the same size");

inline SQLWCHAR* toSqlWchar(wchar_t* p) {
    return reinterpret_cast<SQLWCHAR*>(p);
}
inline const wchar_t* toWchar(const SQLWCHAR* p) {
    return reinterpret_cast<const wchar_t*>(p);
}

// SQLWCHAR buffer → UTF-8 string (convenience wrapper)
inline std::string sqlWcharToUtf8(const SQLWCHAR* buf, size_t len) {
    return wideToUtf8(toWchar(buf), len);
}

// UTF-8 string → std::wstring, ready for ODBC W APIs via toSqlWchar(result.data())
using ::velocitydb::utf8ToWide;

// ODBC APIs accept integer attribute values as SQLPOINTER (void*).
// This is by ODBC specification design; wrap the cast for clarity.
template <typename T>
    requires std::integral<T>
inline SQLPOINTER toSqlPointer(T value) {
    return reinterpret_cast<SQLPOINTER>(static_cast<uintptr_t>(value));
}

}  // namespace velocitydb
