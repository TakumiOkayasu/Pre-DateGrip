#include "a5er_utils.h"

namespace velocitydb::a5er {

std::string unescape(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'q') {
            ++i;
        } else {
            result += s[i];
        }
    }
    return result;
}

}  // namespace velocitydb::a5er
