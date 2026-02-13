#pragma once

#include <string>

namespace velocitydb::a5er {

/**
 * A5:ER テキスト形式のエスケープシーケンスを解除する。
 *
 *   \q → 除去（引用符マーカー。\q0\q → "0"）
 *
 * 将来 \n, \\ 等のエスケープが判明した場合はここに追加する。
 */
[[nodiscard]] std::string unescape(const std::string& s);

}  // namespace velocitydb::a5er
