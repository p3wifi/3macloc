#include "utils.hpp"

namespace wigle_loc {
using namespace utils;
using json = nlohmann::json;

Location find_location_in_response(char* response) {
    Location ret = Location();
    try {
        json j = json::parse(response);
        if (j.contains("results") && j["results"].is_array() && !j["results"].empty()) {
            const auto& entry = j["results"][0];
            double lat = entry.value("trilat", 0.0);
            double lon = entry.value("trilong", 0.0);
            // ssid   = entry.value("ssid", "");
            ret = Location(lat, lon, (uint64_t)0);
        }
    } catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

Location get_location(uint64_t bssid) {
    std::string url = "https://api.wigle.net/api/v2/network/search?netid=" + url_encode(to_mac_string(bssid));

    // get API Token at https://wigle.net/account
    std::vector<std::string> apiKeys = {};
    const char* key = std::getenv("wigle_key");
    if(key) {
         apiKeys.clear();
         apiKeys.push_back(std::string(key));
    }
	if(apiKeys.size() == 0) {
        return Location(utils::error_codes::no_token);
	}
    std::string authHeader = "Authorization: Basic " + random_element(apiKeys);
    std::vector<const char*> headers = {
         authHeader.c_str(),
         "User-Agent: 3macloc"
    };
	utils::Response resp = utils::curl_request_get(url.c_str(), headers);
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
	if(resp.http_code == 401) return Location(utils::error_codes::token);
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}

}
