#pragma once

#include "utils.hpp"

namespace microsoft_loc {
    
    std::string get_current_time();
    Location find_location_in_response(char* response);
    std::string buildXml(const std::string& timestamp, const std::string& bssid);
    Location get_location(uint64_t bssid);
} // namespace microsoft_loc
