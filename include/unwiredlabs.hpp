#pragma once

#include "utils.hpp"

namespace unwiredlabs_loc {
    
    Location find_location_in_response(char* response);
    std::string buildData(const std::string& bssid, const std::string& token);
    Location get_location(uint64_t bssid);
} // namespace unwiredlabs_loc
