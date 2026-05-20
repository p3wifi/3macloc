#include "utils.hpp"

namespace vendor {

std::string get(uint64_t bssid) {
    std::vector<std::string> apiKeys = {
         "01k3x9qawxa6dw4y371t8xsejp01k3x9ry88sczjy9aptx4m50m9ljxbvflgzn8m",
         "01k3x9qawxa6dw4y371t8xsejp01k3y3cxvyb7cmy1thw8wbkantpqakswsc2jzm",
         "01k3x9qawxa6dw4y371t8xsejp01k3y3d6g1mb8akfrgdsx6cajyly6ypzzakbjt",
         "01k3x9qawxa6dw4y371t8xsejp01k3y3dea7vymc65gfhcnc5e1qdwo3ybbuyjhl"
    };
    if((bssid >> 41) & 1) return "Locally administered or P2P";
    std::string url = "https://api.maclookup.app/v2/macs/" + utils::to_mac_string(bssid, false) +
             "/company/name?apiKey=" + utils::random_element(apiKeys);
    utils::Response resp = utils::curl_request_get(url.c_str());
	if(resp.code != 0) return "Vendor failed";
    resp.data[resp.len] = '\0';
    return reinterpret_cast<const char*>(resp.data);
}

}
