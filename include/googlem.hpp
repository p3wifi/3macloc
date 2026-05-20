#pragma once

#include "utils.hpp"

namespace googlem_loc {
    
    Location find_location_in_response(char* response);
    std::string buildData(const std::string& bssid);
    Location get_location(uint64_t bssid);
} // namespace googlem_loc
