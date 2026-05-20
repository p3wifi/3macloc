#include "utils.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace precisely_loc {
using json = nlohmann::json;
using namespace utils;

Location find_location_in_response(char* data) {
    Location ret;
    try {
        json j = json::parse(data);
        if (j.contains("geometry")) {
            double lat = j["geometry"]["coordinates"][1];
            double lon = j["geometry"]["coordinates"][0];
			double acc = 0;
			if(j.contains("accuracy")) 
				acc = stod(j["accuracy"].value("value", "0"));
			ret = Location(lat, lon, (uint64_t)0);
			ret.accuracy = acc;
        } else if(j.contains("errors")) {
			for(auto err : j["errors"]) {
				if(err["errorCode"] == "PB-APIM-ERR-0404")
					return Location(utils::error_codes::token);
				else
					return Location(utils::error_codes::prov_unkn, err["errorDescription"]);
			}
		}
    } catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

std::unordered_map<std::string, std::pair<std::string, uint32_t>> tokens;

std::string get_token(int& error_code) {
    std::vector<std::string> apiKeys = {};
    const char* key = std::getenv("precisely_key");
    if(key) {
         apiKeys.clear();
         apiKeys.push_back(std::string(key));
    }
	if(apiKeys.size() == 0) { 
		error_code = utils::error_codes::no_token;
		return std::string();
	}
	std::string keystr = random_element(apiKeys);
	uint32_t unixtime = std::chrono::duration_cast<std::chrono::seconds>
						(std::chrono::system_clock::now().time_since_epoch()).count();
	if(tokens.contains(keystr)) {
		if(tokens[keystr].second - 10 > unixtime) {
			return tokens[keystr].first;
		}
	}
	std::string url = "https://api.precisely.com/oauth/token";
	std::string data = "grant_type=client_credentials";
    std::string authHeader = "Authorization: Basic " + keystr;
	utils::Response resp = curl_request_post(url.c_str(), (uint8_t*)data.c_str(), data.size(), {authHeader.c_str()});
	if(resp.code != 0) {
        error_code = resp.code;
		return resp.error_str;
	}
    resp.data[resp.len] = '\0';
    std::string access_token;
	uint32_t expire_time = 0;
    try {
        json j = json::parse((char*)resp.data);
        if(j.contains("access_token")) {
            access_token = j["access_token"];
			auto issuedAt = std::stoll(j["issuedAt"].get<std::string>()); 
			auto expiresIn = std::stoll(j["expiresIn"].get<std::string>()); 
			expire_time = issuedAt / 1000 + expiresIn;
        } else if(j.contains("errors")) {
			for(auto err : j["errors"]) {
				if(err["errorCode"] == "PB-APIM-ERR-0401") {
					error_code = utils::error_codes::token;
					return std::string();
				} else {
					error_code = utils::error_codes::prov_unkn;
					return err["errorDescription"];
				}

			}
		}
    } catch(const std::exception& e) { 
		error_code = utils::error_codes::prov_parse;
        return e.what();
    }
	tokens[keystr] = std::pair(access_token, expire_time);
	return access_token;
}

//API DEMO https://developer.precisely.com/demos/geolocation
Location get_location(uint64_t bssid) {
	std::string url = "https://api.precisely.com/geolocation/v1/location/byaccesspoint?mac=" + url_encode(to_mac_string(bssid));
	int token_err = 0;
	std::string token = get_token(token_err);
	if(token_err != 0) return Location(token_err, token);
	std::string authHeader = "Authorization: Bearer " + token;
    Response resp2 = curl_request_get(url.c_str(), {authHeader.c_str()});
	if(resp2.code != 0) {
        return Location(resp2.code, resp2.error_str);
    }
    resp2.data[resp2.len] = 0;
    Location res = find_location_in_response(reinterpret_cast<char*>(resp2.data));
    return res;
}

}
