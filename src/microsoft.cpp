#include <sys/time.h>
#include "utils.hpp"
#include <tinyxml2.h>

namespace microsoft_loc {
using namespace tinyxml2;

std::string get_current_time() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_ms = time_point_cast<milliseconds>(now);
    auto ms = duration_cast<milliseconds>(now_ms.time_since_epoch()) % 1000;
    std::time_t now_c = system_clock::to_time_t(now_ms);
	std::tm parts;
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&parts, &now_c);
#else
    gmtime_r(&now_c, &parts);
#endif
	char* buffer = new char[64];
	std::strftime(buffer, 20, "%Y-%m-%dT%H:%M:%S", &parts);
	std::snprintf(buffer + 19, 11, ".%03dZ", static_cast<int>(ms.count()));
	std::string out((const char*)buffer);
	delete[] buffer;
	return out;
}

Location find_location_in_response(char* response) {
    Location loc = Location();
    tinyxml2::XMLDocument doc;
	if(doc.Parse(response) != XML_SUCCESS) {
		return Location(utils::error_codes::prov_parse, doc.ErrorStr());
	}
    XMLElement* locationResult = doc.FirstChildElement("PositionByNetworkResponse")
                                   ->FirstChildElement("PositionByNetworkResult")
                                   ->FirstChildElement("LocationResult");
    if(!(locationResult->FirstChildElement("ResolverStatus")->Attribute("Source", "Internal")))
        return loc;
    XMLElement* resolved = locationResult->FirstChildElement("ResolvedPosition");
    if (!resolved) return loc;
    double lat = resolved->DoubleAttribute("Latitude");
    double lon = resolved->DoubleAttribute("Longitude");
//    double alt = resolved->DoubleAttribute("Altitude");
    loc = Location(lat, lon, (uint64_t)0);
    return loc;
}

std::string buildXml(const std::string& timestamp, const std::string& bssid) {
    return
		"<?xml version=\"1.0\"?>"
		"<PositionByNetwork xmlns=\"http://inference.location.live.com\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
		" <RequestHeader>"
		"  <Timestamp>" + timestamp + "</Timestamp>"
		"  <ApplicationId>d414dd4f9db345fa8003e32adc81b362</ApplicationId>"
		" </RequestHeader>"
		" <BeaconFingerprint>"
		"  <Detections>"
		"   <Wifi7 BssId=\"" + bssid + "\" rssi=\"-1\" cf=\"-2147483648\"/>"
		"  </Detections>"
		" </BeaconFingerprint>"
		"</PositionByNetwork>";
}

Location get_location(uint64_t bssid) {
    const char* url = "https://inference.location.live.net/inferenceservice/v21/Pox/PositionByNetwork";
    std::string data = buildXml(get_current_time(), utils::to_mac_string(bssid));
	std::vector<const char*> headers = {
		"pragma: no-cache",
		"cache-control: no-cache",
		"content-type: application/xml",
		"sec-fetch-site: none",
		"sec-fetch-mode: no-cors",
		"sec-fetch-dest: empty",
		"user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/144.0.0.0 Safari/537.36 Edg/143.0.3650.139",
		"priority: u=4, i"
	};
	utils::Response resp = utils::curl_request_post(url, (uint8_t*)data.c_str(), data.size(), headers);
	if(resp.code != 0) {
        return Location(resp.code, resp.error_str);
    }
	if(resp.http_code == 403)
		return Location(utils::error_codes::toomanyreq);
    resp.data[resp.len] = '\0';
    Location res = find_location_in_response(reinterpret_cast<char*>(resp.data));
    return res;
}
}
