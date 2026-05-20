#include "utils.hpp"

namespace here_loc {
using json = nlohmann::json;

Location find_location_in_response(char* response) {
    Location ret = Location();
    try {
        json j = json::parse(response);
        if (j.contains("location") && !j["location"].empty()) {
            const auto& entry = j["location"];
            double lat  = entry.value("lat", 0.0);
            double lon  = entry.value("lng", 0.0);
            double accuracy  = entry.value("accuracy", 0.0);
            ret = Location(lat, lon, (uint64_t)0);
			ret.accuracy = accuracy;
        }
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

//API DOC https://www.here.com/docs/bundle/network-positioning-api-v2-api-reference/page/index.html
std::string buildData(const std::string& bssid) {
    return "{\"wlan\":[{\"mac\":\"" + bssid + "\",\"rss\":-20}]}";
}

Location get_location(uint64_t bssid) {
    std::vector<std::string> apiKeys = {};
    const char* key = std::getenv("here_key");
    if(key) {
         apiKeys.clear();
         apiKeys.push_back(std::string(key));
    }
	if(apiKeys.size() == 0) {
        return Location(utils::error_codes::no_token);
	}
    std::string url = "https://positioning.hereapi.com/v2/locate?fallback=singleWifi&apiKey=" + utils::random_element(apiKeys);
    std::string data = buildData(utils::to_mac_string(bssid));
    std::vector<const char*> headers = {
         "Content-Type: application/json"
    };
    utils::Response resp = utils::curl_request_post(url.c_str(), (uint8_t*)data.c_str(), data.size(), headers);
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
	if(resp.http_code == 401) return Location(utils::error_codes::token);
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}
}
