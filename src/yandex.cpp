#include "utils.hpp"
#include <tinyxml2.h>

namespace yandex_loc {
using namespace tinyxml2;

Location find_location_in_response(char* response) {
    Location ret = Location();
	try {
		tinyxml2::XMLDocument doc;
		if(doc.Parse(response) != XML_SUCCESS) {
			return Location(utils::error_codes::prov_parse, doc.ErrorStr());
		}
		XMLElement* location = doc.FirstChildElement("location");
		XMLElement* coords = location ? location->FirstChildElement("coordinates") : nullptr;
		if (coords) {
			double lat = coords->DoubleAttribute("latitude");
			double lon = coords->DoubleAttribute("longitude");
			// double nlat = coords->DoubleAttribute("nlatitude");
			// double nlon = coords->DoubleAttribute("nlongitude");
			ret = Location(lat, lon, (uint64_t)0);
		}
	} catch(const std::exception& e) {
		ret = Location(utils::error_codes::prov_parse, e.what());
	}
    return ret;
}

Location get_location(uint64_t bssid) {
    std::string url = "https://api.lbs.yandex.net/cellid_location?wifinetworks="
		+ utils::to_mac_string(bssid, false) + "%3A-65";
	utils::Response resp = utils::curl_request_get(url.c_str());
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}

}
