#pragma once

#include "utils.hpp"
#include <vector>
#include "picoproto.h"

namespace apple_loc {
    Location find_location_in_response(uint8_t* response, size_t resp_len, uint64_t source_bssid);
	std::vector<Location> get_location_multi(std::vector<uint64_t> bssids, bool china);
    Location get_location(uint64_t bssid);
    Location get_location_china(uint64_t bssid);
} // namespace apple_loc
