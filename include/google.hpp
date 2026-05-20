#pragma once

#include "utils.hpp"

namespace google_loc {
    void insert_mac(uint8_t* buf, int pos, uint64_t input);
    uint8_t* constructData2(const uint8_t* data, size_t len, size_t& out_len);
    Location find_location_in_response(uint8_t* response, size_t resp_len);
    Location get_location(uint64_t bssid);
}