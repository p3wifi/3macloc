#pragma once

#include <location.hpp>
extern "C" {
#include "base64.h"
}
#include <cstring>
#include <curl/curl.h>
#include <zlib.h>
#include <vector>
#include <string>
#include <random>
#include <cstdlib>
#include <iostream>
#include <mutex>

extern std::mutex cerr_mutex;

namespace utils {

enum error_codes {
	ok = 0,
	curl_init = -1,
	prov_parse = -2,
	token = -3,
	prov_unkn = -4,
	no_token = -5,
	toomanyreq = -6,
	multi = -7
};

#define CURLBUFFERSIZE 4096 

struct CurlBuffer {
	size_t len = 0;
	size_t capacity = 0;
	uint8_t* data = nullptr;
	CurlBuffer() {
		len = 0;
		capacity = CURLBUFFERSIZE;
		data = reinterpret_cast<uint8_t*>(std::malloc(CURLBUFFERSIZE));
	}
	//~CurlBuffer() {
	//	if(data) std::free(data);
	//	data = nullptr;
	//}
};


struct Response {
	uint8_t* data = nullptr;
	size_t len = 0;
	int code = 0;
	int http_code = 0;
	char* error_str = nullptr;
	double time;
	
	Response() { code = -1; }
    ~Response() { std::free(data); std::free(error_str); }
	Response(int _code) 
		: code(_code) {}
	Response(int _code, char* _error_str)
		: code(_code),error_str(_error_str) {}
	Response(CurlBuffer _buf, int _code, double _time) {
		data = _buf.data;
		len = _buf.len;
		time = _time;
		code = 0;
		http_code = _code;
	}

    Response(Response&& other) noexcept
        : data(other.data), len(other.len), code(other.code), http_code(other.http_code), error_str(other.error_str), time(other.time) {
        other.data = nullptr;
        other.error_str = nullptr;
        other.len = 0;
    }
    Response& operator=(Response&& other) noexcept {
        if (this != &other) {
            std::free(data);
            std::free(error_str);
            data = other.data;
            len = other.len;
			code = other.code;
			http_code = other.http_code;
			error_str = other.error_str;
			time = other.time;
            other.data = nullptr;
            other.len = 0;
        }
        return *this;
    }
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;
};

extern int CURL_TIMEOUT; // timeout for curl operations in sec
extern int MAX_RETRIES;
extern thread_local std::string proxy;

void dumpToFile(const char* filename, const uint8_t* data, size_t length);

template<typename T>
const T& random_element(const std::vector<T>& vec) {
    if (vec.empty()) return NULL; //vec[0];
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<std::size_t> dist(0, vec.size() - 1);
    return vec[dist(gen)];
}

std::string url_encode(const std::string& value);

std::string toLower(const std::string& input);

uint8_t write_mac_stream(uint8_t* data, size_t offset, uint64_t mac, bool colon = true);

std::string to_mac_string(uint64_t mac, bool colon = true);

uint64_t to_mac_uint64(const std::string& macStr);

uint8_t* zlib_compress(uint8_t* data, size_t dsize, size_t& osize);

uint8_t* zlib_decompress(uint8_t* compressedData, size_t len, size_t& out_len);

size_t curlWrite(const char* ptr, const size_t size, const size_t nmemb, void* buffer_ptr);

Response curl_request_get(const char* request_url, std::vector<const char*> headers = {});

Response curl_request_get(const char* request_url, std::vector<const char*> headers, int retries);

Response curl_request_post(const char* request_url, uint8_t* request_data, size_t request_data_length, std::vector<const char*> headers = {});

Response curl_request_post(const char* request_url, uint8_t* request_data, size_t request_data_length, std::vector<const char*> headers, int retries);

void pb_write_varint(std::vector<uint8_t>& out, uint64_t v);

void pb_write_key(std::vector<uint8_t>& out, uint32_t field, uint32_t wireType);

void pb_write_varint(std::vector<uint8_t>& out, uint32_t field, uint64_t value);

void pb_write_bytes(std::vector<uint8_t>& out, uint32_t field, const std::vector<uint8_t>& data);

} // namespace utils
