#include <google.hpp>

namespace google_loc {

// https://media.blackhat.com/eu-13/briefings/Jeske/bh-eu-13-floating-car-data-jeske-wp.pdf

uint8_t data1[] = {
    0x0A, 0x66, 0x0A, 0x04, 0x32, 0x30, 0x32, 0x31, 0x12, 0x57, 0x61, 0x6E, 0x64, 0x72, 0x6F, 0x69,
    0x64, 0x2F, 0x4C, 0x45, 0x41, 0x47, 0x4F, 0x4F, 0x2F, 0x66, 0x75, 0x6C, 0x6C, 0x5F, 0x77, 0x66,
    0x35, 0x36, 0x32, 0x67, 0x5F, 0x6C, 0x65, 0x61, 0x67, 0x6F, 0x6F, 0x2F, 0x77, 0x66, 0x35, 0x36,
    0x32, 0x67, 0x5F, 0x6C, 0x65, 0x61, 0x67, 0x6F, 0x6F, 0x3A, 0x36, 0x2E, 0x30, 0x2F, 0x4D, 0x52,
    0x41, 0x35, 0x38, 0x4B, 0x2F, 0x31, 0x35, 0x31, 0x31, 0x31, 0x36, 0x31, 0x37, 0x37, 0x30, 0x3A,
    0x75, 0x73, 0x65, 0x72, 0x2F, 0x72, 0x65, 0x6C, 0x65, 0x61, 0x73, 0x65, 0x2D, 0x6B, 0x65, 0x79,
    0x73, 0x2A, 0x05, 0x65, 0x6E, 0x5F, 0x55, 0x53, 0x22, 0x22, 0x12, 0x1E, 0x08, 0xA3, 0xF7, 0x09,
    0x12, 0x0A, 0x0A, 0x00, 0x40, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0x6E, 0x12, 0x0A, 0x0A, 0x00,
    0x40, 0xE6, 0xAA, 0x91, 0x9A, 0xA3, 0xA4, 0x04, 0x18, 0x02, 0x50, 0x00
};

const uint8_t data2_1[] = {
    0x00, 0x02, 0x00, 0x00, 0x1F, 0x6C, 0x6F, 0x63, 0x61, 0x74, 0x69, 0x6F, 0x6E, 0x2C, 0x32, 0x30,
    0x32, 0x31, 0x2C, 0x61, 0x6E, 0x64, 0x72, 0x6F, 0x69, 0x64, 0x2C, 0x67, 0x6D, 0x73, 0x2C, 0x65,
    0x6E, 0x5F, 0x55, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x67, 0x00,
    0x00, 0x00, 0xBB, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x08, 0x67, 0x3A, 0x6C, 0x6F, 0x63, 0x2F,
    0x71, 0x6C, 0x00, 0x00, 0x00, 0x04, 0x50, 0x4F, 0x53, 0x54, 0x6D, 0x72, 0x00, 0x00, 0x00, 0x04,
    0x52, 0x4F, 0x4F, 0x54, 0x00
};


void insert_mac(uint8_t* buf, int pos, uint64_t input) {
    int i;
    for (i = 0; i < 6; i++) {
        buf[i+pos] = (input & 0x7F) | 0x80;
        input >>= 7;
    }
    buf[i+pos] = input;
}

uint8_t* construct_masf_header(const uint8_t* data, size_t len, size_t& out_len) {
    uint8_t *buf = new uint8_t[1024];
	int pos = sizeof(data2_1);
    memcpy(buf, data2_1, sizeof(data2_1));
	buf[pos++] = ((len >> 32) & 0xFF);
	buf[pos++] = ((len >> 16) & 0xFF);
	buf[pos++] = ((len >> 8) & 0xFF);
	buf[pos++] = ((len >> 0) & 0xFF);
	buf[pos++] = 0x00; 
    buf[pos++] = 0x01;
    buf[pos++] = 0x67;
    memcpy(buf + pos, data, len);
	pos += len;
	buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    out_len = pos;
    return buf;
}

Location find_location_in_response(uint8_t* resp, size_t resp_len) {
	size_t decm_len;
	uint8_t* decompressedData = utils::zlib_decompress(resp + 19, resp_len - 19, decm_len);
	if (decm_len == 0) {
		return Location(utils::error_codes::prov_parse, "gzip decompress err");
	}
	for (size_t i = 0; i + 12 < decm_len; i++) {
		if (decompressedData[i] == 0x0A && decompressedData[i + 1] == 0x0A && decompressedData[i + 12] == 24) { // TODO: tinybuf
			int lat  = decompressedData[i + 3];
				lat |= decompressedData[i + 4] << 8;
				lat |= decompressedData[i + 5] << 16;
				lat |= decompressedData[i + 6] << 24;
			int lon  = decompressedData[i + 8];
				lon |= decompressedData[i + 9] << 8;
				lon |= decompressedData[i + 10] << 16;
				lon |= decompressedData[i + 11] << 24;
			delete[] decompressedData;
			return Location(lat / 10000000.0, lon / 10000000.0);
		}
	}
	delete[] decompressedData;
    return Location();
}

Location get_location(uint64_t bssid) {
    insert_mac(data1, 0x75, bssid);
    size_t comp_data1_len;
    uint8_t* comp_data1 = utils::zlib_compress(data1, sizeof(data1), comp_data1_len);
    size_t data2_len;
    uint8_t* data2 = construct_masf_header(comp_data1, comp_data1_len, data2_len);
    delete[] comp_data1;

    std::vector<const char*> headers = {
        "Content-Type: application/binary",
        "User-Agent: GoogleMobile/1.0 (M5 MRA58K); gzip",
        "Accept-Encoding: gzip"
    };
	utils::Response resp = utils::curl_request_post("https://www.google.com/loc/m/api", data2, data2_len, headers);
    if(resp.code != 0) {
		delete[] data2;
        return Location(resp.code, resp.error_str);
    } 
	Location res = find_location_in_response(resp.data, resp.len);
    delete[] data2;
    return res;
}


static std::vector<uint8_t> make_network(uint64_t field8) {
    std::vector<uint8_t> item;
    utils::pb_write_bytes(item, 1, {0x00, 0x00});
    utils::pb_write_varint(item, 8, field8);
    return item;
}

static std::vector<uint8_t> make_inner2(const std::vector<uint64_t>& values) {
    std::vector<uint8_t> inner;
    utils::pb_write_varint(inner, 1, 162723);
    for (uint64_t v : values) {
        std::vector<uint8_t> item = make_network(v);
        utils::pb_write_bytes(inner, 2, item);
    }
    utils::pb_write_varint(inner, 3, 2);
    return inner;
}

static std::vector<uint8_t> make_inner(const std::vector<uint64_t>& values) {
    std::vector<uint8_t> inner;
	utils::pb_write_bytes(inner, 2, make_inner2(values));
    utils::pb_write_varint(inner, 10, 0);
    return inner;
}

static std::vector<uint8_t> make_root(const std::vector<uint64_t>& values) {
    std::vector<uint8_t> root;
    utils::pb_write_bytes(root, 4, make_inner(values));
    return root;
}
#define HD "0123456789ABCDEF"

std::vector<Location> get_location_multi(std::vector<uint64_t> bssids) {
	std::vector<uint8_t> data1 = {
		0x0a, 0x65, 0x0a, 0x04, 0x32, 0x30, 0x32, 0x31, 0x12, 0x56, 0x61, 0x6e, 0x64, 0x72, 0x6f, 0x69, 0x64,
		0x2f, 0x4c, 0x45, 0x41, 0x47, 0x4f, 0x4f, 0x2f, 0x66, 0x75, 0x6c, 0x6c, 0x5f, 0x77, 0x66, 0x35, 0x36,
		0x32, 0x67, 0x5f, 0x6c, 0x65, 0x61, 0x67, 0x6f, 0x6f, 0x2f, 0x77, 0x66, 0x35, 0x36, 0x32, 0x67, 0x5f,
		0x6c, 0x65, 0x61, 0x67, 0x6f, 0x6f, 0x3a, 0x36, 0x2e, 0x30, 0x2f, 0x4d, 0x52, 0x41, 0x35, 0x38, 0x4b,
		0x2f, 0x31, 0x35, 0x31, 0x31, 0x31, 0x36, 0x31, 0x37, 0x37, 0x30, 0x75, 0x73, 0x65, 0x72, 0x2f, 0x72,
		0x65, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x2d, 0x6b, 0x65, 0x79, 0x73, 0x2a, 0x05, 0x65, 0x6e, 0x5f, 0x55,
		0x53
	};
	std::vector<uint8_t> pb_networks = make_root(bssids);
	data1.insert(data1.end(), pb_networks.begin(), pb_networks.end()); // data1 += pb_networks

	size_t comp_data1_len;
    uint8_t* comp_data1 = utils::zlib_compress(data1.data(), data1.size(), comp_data1_len);
    size_t data2_len;
    uint8_t* data2 = construct_masf_header(comp_data1, comp_data1_len, data2_len);
    delete[] comp_data1;

	utils::Response resp = utils::curl_request_post(
		"https://www.google.com/loc/m/api",
		data2,
		data2_len,
		{
			"Content-Type: application/binary",
			"User-Agent: GoogleMobile/1.0 (M5 MRA58K); gzip",
			"Accept-Encoding: gzip"
		}
	);
	std::vector<Location> result;
	if(resp.code != 0 || resp.len <= 0) {
        result.push_back(Location(resp.code, resp.error_str));
		return result;
    }

	picoproto::Message message;
	size_t dcmp_len;
	uint8_t* dcmp_data = utils::zlib_decompress(resp.data + 19, (int)resp.len - 19, dcmp_len);
	message.ParseFromBytes(dcmp_data, dcmp_len);
	delete[] dcmp_data;
	picoproto::Message* inner = message.GetMessage(2);
	std::vector<picoproto::Message*> networks = inner->GetMessageArray(3);
    for(auto network : networks) {
		uint64_t resp_bssid = network->GetMessage(3)->GetUInt64(8);
		std::vector<picoproto::Message*> network_loc = network->GetMessageArray(1);
		if(network_loc.size() == 0) {
			Location empty;
			empty.sourceBssid = resp_bssid;
			result.push_back(empty);
		} else {
			int64_t lat = network_loc[0]->GetMessage(1)->GetUInt32(1);
			int64_t lon = network_loc[0]->GetMessage(1)->GetUInt32(2);
			result.push_back(Location(lat / 10000000.0, lon / 10000000.0, resp_bssid));
		}
    }
    return result;
}

}

