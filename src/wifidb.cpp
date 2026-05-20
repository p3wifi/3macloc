#include "utils.hpp"

namespace wifidb_loc {
using json = nlohmann::json;
using namespace utils;

Location find_location_in_response(char* response) {
    Location ret;
    try {
        json j = json::parse(response);
        if (j.contains("features") && !j["features"].empty()) {
            const auto& entry = j["features"][0]["geometry"];
            double lat  = entry["coordinates"][1];
            double lon  = entry["coordinates"][0];
            ret = Location(lat, lon, (uint64_t)0);
        }
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

Location get_location(uint64_t bssid) {
    std::string url = "https://wifidb.net/api/geojson.php?func=exp_search"
                   "&json=1&from=1&inc=1&mac=" + url_encode(to_mac_string(bssid));
	Response resp = curl_request_get(url.c_str());
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}

}
