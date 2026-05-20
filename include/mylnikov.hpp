#pragma once

#include "utils.hpp"

namespace mylnikov_loc {
    
    Location find_location_in_response(char* response);
    Location get_location(uint64_t bssid);
} // namespace mylnikov_loc
