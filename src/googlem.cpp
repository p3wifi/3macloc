#include "location.hpp"
#include "utils.hpp"

namespace googlem_loc {
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
        }
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

//API DOC https://developers.google.cn/maps/documentation/geolocation/requests-geolocation?hl=en#wifi_access_point_object
std::string buildData(const std::string& bssid) {
    return "{\"considerIp\":false,"
            "\"wifiAccessPoints\":[{\"macAddress\":\"" + bssid + "\",\"signalStrength\":-20,\"channel\":11,\"signalToNoiseRatio\":0}],"
            "\"fallbacks\":{\"lacf\":false,\"ipf\":false}"
           "}";
}

Location get_location(uint64_t bssid) {
	return Location(utils::error_codes::multi);
    const char* url = "https://www.googleapis.com/geolocation/v1/geolocate?key=AIz************************************";
    std::string data = buildData(utils::to_mac_string(bssid));
    std::vector<const char*> headers = {
         "Content-Type: application/json",
         "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) WindowsPowerShell/537.36"
    };
	utils::Response resp = utils::curl_request_post(url, (uint8_t*)data.c_str(), data.size(), headers);
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}
}
