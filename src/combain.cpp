#include "location.hpp"
#include "utils.hpp"
#include <iostream>

namespace combain_loc {
using json = nlohmann::json;

Location find_location_in_response(char* response) {
    Location ret;
    try {
        json j = json::parse(response);
        if (j.contains("location") && !j["location"].empty() && !j.contains("fallback")) {
            const auto& entry = j["location"];
            double lat  = entry.value("lat", 0.0);
            double lon  = entry.value("lng", 0.0);
            ret = Location(lat, lon, (uint64_t)0);
        } else if(j.contains("error")) {
			if(j["error"].value("message", "") == "Key Missing or Invalid")
				return Location(utils::error_codes::token);
		}
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

//API DOC https://ichnaea.readthedocs.io/en/latest/api/geolocate.html
std::string buildData(const std::string& bssid) {
    return "{\"wifiAccessPoints\":[{\"macAddress\":\"" + bssid + "\",\"signalStrength\":-20}]}";
}

Location get_location(uint64_t bssid) {
    std::vector<std::string> apiKeys = {};
    const char* key = std::getenv("combain_key");
    if(key) {
         apiKeys.clear();
         apiKeys.push_back(std::string(key));
    }
	if(apiKeys.size() == 0) {
        return Location(utils::error_codes::no_token);
	}
    std::string url = "https://apiv2.combain.com?key=" + utils::random_element(apiKeys);
    std::string data = buildData(utils::to_mac_string(bssid));
    std::vector<const char*> headers = {
         "Content-Type: application/json"
    };
	utils::Response resp = utils::curl_request_post(url.c_str(), (uint8_t*)data.c_str(), data.size(), headers);
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
	resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}
}
