#include "utils.hpp"

namespace unwiredlabs_loc {
using json = nlohmann::json;
using namespace utils;

Location find_location_in_response(char* response) {
    Location ret = Location();
    try {
        json j = json::parse(response);
        if (j.contains("status") && (j["status"] == "ok")) {
            double lat = j["lat"];
            double lon = j["lon"];
            // accuracy = j["accuracy"];
            ret = Location(lat, lon, (uint64_t)0);
        }
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

std::string buildData(const std::string& bssid, const std::string& token) {
    return "{\"token\":\"" + token + "\","
           "\"wifi\": [{\"bssid\":\"" + bssid + "\",\"signal\": -20}]}";
}

Location get_location(uint64_t bssid) {
    std::vector<std::string> apiKeys = {};
    const char* key = std::getenv("unwiredlabs_key");
    if(key) {
         apiKeys.clear();
         apiKeys.push_back(std::string(key));
    }
	if(apiKeys.size() == 0) {
        return Location(utils::error_codes::no_token);
	}
    const char* url = "https://us1.unwiredlabs.com/v2/process";
    std::string data = buildData(utils::to_mac_string(bssid), random_element(apiKeys));
    std::vector<const char*> headers = {
         "Content-Type: application/json",
         "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) WindowsPowerShell/537.36"
    };
    Response resp = utils::curl_request_post(url, (uint8_t*)data.c_str(), data.size(), headers);
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}
}
