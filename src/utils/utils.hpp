#pragma once
#include "wrappers/message_box.hpp"

namespace utils {
	[[nodiscard]]
	int show_last_error(LPCWSTR context);
}