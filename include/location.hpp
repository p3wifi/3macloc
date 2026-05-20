#pragma once

#include <cstdint>
#include <string>
#include <cstring>
#include "json.hpp"
using json = nlohmann::json;

struct Location {
    double lat = 0;
    double lon = 0;
	double accuracy = 0;
    uint64_t sourceBssid = 0;
    bool valid = false;
    uint8_t* raw_response = nullptr;
    size_t raw_response_len = 0;
	int code = 0;
	std::string error_str;

    Location() {}
	Location(int _code)
		: code(_code) {}
	Location(int _code, std::string _error_str)
		: code(_code),error_str(_error_str) {}
    Location(double _lat, double _lon) {
        lat = _lat;
        lon = _lon;
        valid = true;
    }
    Location(double _lat, double _lon, uint64_t _sourceBssid) {
        lat = _lat;
        lon = _lon;
        sourceBssid = _sourceBssid;
        valid = true;
    }
    json to_json() const {
        json j;
        j["found"] = valid;
        if (valid) {
            j["lat"] = lat;
            j["lon"] = lon;
			if(accuracy != 0) j["accuracy"] = accuracy;
        }
        return j;
    }
	std::string get_error_str() {
		std::string err2str[] = {
			"OK",
			"Error initializing libcurl",
			"Error parsing geolocation provider response",
			"Invalid token",
			"The geolocation provider returned an unknown error",
			"No token",
			"Too many requests",
			"The provider requires several Wi-Fi points to search, you passed one"
		};
		return (code > 0 ? error_str : err2str[-code]);
	}
};
