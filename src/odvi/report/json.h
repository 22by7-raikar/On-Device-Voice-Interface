/// @file
/// @brief Minimal JSON string escaping used by the event writers.
/// @ingroup report

#pragma once

#include <string>
#include <string_view>

namespace odvi {

/// @defgroup report Reporting
/// @brief JSON encoding for the event stream.
/// @{

/// @brief Escape @p value for use inside a JSON string literal (RFC 8259):
/// quote, backslash and all control characters below U+0020.
std::string json_escape(std::string_view value);

/// @brief `"` + json_escape(@p value) + `"`.
std::string json_quote(std::string_view value);

/// @}

}  // namespace odvi
