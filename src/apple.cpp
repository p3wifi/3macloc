#include "apple.hpp"
#include <iterator>

namespace apple_loc {

uint8_t data1[] = {
    0x00, 0x01, 0x00, 0x05, 0x65, 0x6E, 0x5F, 0x55, 0x53, 0x00, 0x13, 0x63, 0x6f, 0x6d, 0x2e, 0x61,
    0x70, 0x70, 0x6c, 0x65, 0x2e, 0x6c, 0x6f, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x64, 0x00, 0x0A,
    0x38, 0x2e, 0x31, 0x2e, 0x31, 0x32, 0x42, 0x34, 0x31, 0x31, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x19, 0x12, 0x13, 0x0a, 0x11, 0xEE, 0xEE, 0x3A, 0xEE, 0xEE, 0x3A, 0xEE, 0xEE, 0x3A, 0xEE,
    0xEE, 0x3A, 0xEE, 0xEE, 0x3A, 0xEE, 0xEE, 0x18, 0x00, 0x20, 0x01
};

#define NETWORK404VAL int64_t(-18000000000)

Location find_location_in_response(uint8_t* response, size_t resp_len, uint64_t source_bssid) {
    picoproto::Message message;
    message.ParseFromBytes(response + 10, resp_len - 10);
    std::vector<picoproto::Message*> networks = message.GetMessageArray(2);
    for(auto network : networks) {
		if(source_bssid == utils::to_mac_uint64(network->GetString(1))) {
			picoproto::Message* network_location_data = network->GetMessage(2);
			int64_t lat = network_location_data->GetUInt64(1);
			int64_t lon = network_location_data->GetUInt64(2);
			if(lat == NETWORK404VAL || lon == NETWORK404VAL) { // Network not found
				return Location();
			}
			return Location(lat / 100000000.0, lon / 100000000.0, (uint64_t)0);
		}
    }
    return Location();
}

Location get_location_internal(uint64_t bssid, bool china) {
    std::vector<const char*> headers = {
        "Content-Type: application/x-www-form-urlencoded",
        "Accept: */*",
        "Accept-Charset: utf-8",
        "Accept-Language: en-us",
        "User-Agent: locationd/1753.17 CFNetwork/711.1.12 Darwin/14.0.0"
    };
    utils::write_mac_stream(data1, 0x36, bssid);
	const char* url = china ? "https://gs-loc-cn.apple.com/clls/wloc" : "https://gs-loc.apple.com/clls/wloc";
	utils::Response resp = utils::curl_request_post(url, data1, sizeof(data1), headers);
	if(resp.code != 0 || resp.len <= 0) {
        return Location(resp.code, resp.error_str);
    }
    Location result = find_location_in_response(resp.data, resp.len, bssid);
    return result;
}

std::vector<Location> get_location_multi(std::vector<uint64_t> bssids, bool china) {
    std::vector<const char*> headers = {
        "Content-Type: application/x-www-form-urlencoded",
        "Accept: */*",
        "Accept-Charset: utf-8",
        "Accept-Language: en-us",
        "User-Agent: locationd/1753.17 CFNetwork/711.1.12 Darwin/14.0.0"
    };
	std::vector<uint8_t> data = { 
		0x00, 0x01, 0x00, 0x05, 0x65, 0x6E, 0x5F, 0x55, 0x53, 0x00, 0x13, 0x63, 0x6f, 0x6d, 0x2e, 0x61,
		0x70, 0x70, 0x6c, 0x65, 0x2e, 0x6c, 0x6f, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x64, 0x00, 0x0A,
		0x38, 0x2e, 0x31, 0x2e, 0x31, 0x32, 0x42, 0x34, 0x31, 0x31, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
		0x00, 0x00
	};
	std::vector<uint8_t> pb_networks;
	for(uint64_t bssid : bssids) {
		uint8_t network[] = {
			0x12, 0x13, 0x0a, 0x11, 0xEE, 0xEE, 0x3A, 0xEE, 0xEE, 0x3A, 0xEE,
			0xEE, 0x3A, 0xEE, 0xEE, 0x3A, 0xEE, 0xEE, 0x3A, 0xEE, 0xEE 
		};
		utils::write_mac_stream(network, 4, bssid);
		pb_networks.insert(pb_networks.end(), network, network + sizeof(network));
	}
	data.insert(data.end(), pb_networks.begin(), pb_networks.end());
	data.push_back(0x18);
	data.push_back(0x01);
	data.push_back(0x20);
	data.push_back(0x01);
	data[48] = ((data.size() - 50) >> 8) & 0xFF;
	data[49] = ((data.size() - 50)) & 0xFF;

	const char* url = china ? "https://gs-loc-cn.apple.com/clls/wloc" : "https://gs-loc.apple.com/clls/wloc";
	utils::Response resp = utils::curl_request_post(url, data.data(), data.size(), headers);
	std::vector<Location> result;
	if(resp.code != 0 || resp.len <= 0) {
        result.push_back(Location(resp.code, resp.error_str));
		return result;
    }
    picoproto::Message message;
    message.ParseFromBytes(resp.data + 10, resp.len - 10);
    std::vector<picoproto::Message*> networks = message.GetMessageArray(2);
    for(auto network : networks) {
		uint64_t resp_bssid = utils::to_mac_uint64(network->GetString(1));
		picoproto::Message* network_location_data = network->GetMessage(2);
		int64_t lat = network_location_data->GetUInt64(1);
		int64_t lon = network_location_data->GetUInt64(2);
		if(lat == NETWORK404VAL || lon == NETWORK404VAL) { // Network not found
			Location empty;
			empty.sourceBssid = resp_bssid;
			result.push_back(empty);
		} else {
			result.push_back(Location(lat / 100000000.0, lon / 100000000.0, resp_bssid));
		}
    }
    return result;
}

Location get_location(uint64_t bssid) {
	return get_location_internal(bssid, false);
}

Location get_location_china(uint64_t bssid) {
	return get_location_internal(bssid, true);
}
}
