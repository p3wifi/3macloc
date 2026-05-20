#include "utils.hpp"

namespace mylnikov_loc {
using json = nlohmann::json;
using namespace utils;

Location find_location_in_response(char* response) {
    Location ret;
    try {
        json j = json::parse(response);
        if (j.contains("data") && !j["data"].empty()) {
            const auto& entry = j["data"];
            double lat  = entry.value("lat", 0.0);
            double lon  = entry.value("lon", 0.0);
            // range    = entry.value("range", 0.0);
            ret = Location(lat, lon, (uint64_t)0);
        }
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

Location get_location(uint64_t bssid) {
    std::string url = "https://api.mylnikov.org/geolocation/wifi?v=1.1&bssid=" + to_mac_string(bssid, false);
	utils::Response resp = curl_request_get(url.c_str());
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}

}
