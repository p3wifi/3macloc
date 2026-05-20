#pragma once

#include "utils.hpp"

namespace precisely_loc {
    
    Location find_location_in_response(char* response);
    Location get_location(uint64_t bssid);
} // namespace precisely_loc
