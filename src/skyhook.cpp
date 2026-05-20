#include "utils.hpp"

namespace skyhook_loc {
using namespace utils;
using json = nlohmann::json;

Location find_location_in_response(char* response) {
    Location ret = Location();
    try {
        json j = json::parse(response);
        if (j.contains("location") && !j["location"].empty() && !j.contains("fallback")) {
            const auto& entry = j["location"];
            double lat  = entry.value("lat", 0.0);
            double lon  = entry.value("lng", 0.0);
            ret = Location(lat, lon, (uint64_t)0);
			ret.accuracy = j.value("accuracy", 0.0);
        }
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

//API DOC https://ichnaea.readthedocs.io/en/latest/api/geolocate.html
std::string buildData(const std::string& bssid) {
    return "{\"considerIp\":false,"
            "\"allowSingleAPLocation\":true,"
            "\"wifiAccessPoints\":[{\"macAddress\":\"" + bssid + "\",\"signalStrength\":-20,\"connected\":true}]"
           "}";
}

Location get_location(uint64_t bssid) {
    const char* url = "https://global.skyhookwireless.com/wps2/json/location";
    std::vector<std::string> apiKeys = {};
    const char* key = std::getenv("skyhook_key");
    if(key) {
         apiKeys.clear();
         apiKeys.push_back(std::string(key));
    }
	if(apiKeys.size() == 0) {
        return Location(utils::error_codes::no_token);
	}
	std::string apiKey = "Skyhook-Auth-Key: " + random_element(apiKeys);
    std::string data = buildData(utils::to_mac_string(bssid));
    std::vector<const char*> headers = {
         "Skyhook-Proto-Ver: 2.41",
         apiKey.c_str(),
         "Content-Type: application/json",
         "X-Forwarded-For: 127.0.0.1"
    };
	utils::Response resp = utils::curl_request_post(url, (uint8_t*)data.c_str(), data.size(), headers);
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
	}
	if(resp.http_code == 401) return Location(utils::error_codes::token);
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}
}
