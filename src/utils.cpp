#include "utils.hpp"
#include "location.hpp"
#include "base64.h"
#include "tinyxml2.h"
#include "json.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <curl/curl.h>
#include <zlib.h>
#include <vector>
#include <string>
#include <cctype>
#include <random>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>


std::mutex cerr_mutex;

namespace utils {

int CURL_TIMEOUT = 5L; // timeout for curl operations in sec
int MAX_RETRIES = 1;
thread_local std::string proxy;

//utils::dumpToFile("req.zin",data1,sizeof(data1));
void dumpToFile(const char* filename, const uint8_t* data, size_t length) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::lock_guard<std::mutex> lock(cerr_mutex);
        std::cerr << "Can't open file: " << filename << std::endl;
        return;
    }
    file.write(reinterpret_cast<const char*>(data), length);
    file.close();
}
//-------------------

std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex << std::uppercase;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

std::string toLower(const std::string& input) {
    std::string result = input;
    for (char& c : result) {
        c = std::tolower(static_cast<unsigned char>(c));
    }
    return result;
}

uint8_t write_mac_stream(uint8_t* data, size_t offset, uint64_t mac, bool colon) {
    for (int i = 5; i >= 0; --i) {
        uint8_t byte = (mac >> (i * 8)) & 0xFF;
        char high = "0123456789ABCDEF"[byte >> 4];
        char low  = "0123456789ABCDEF"[byte & 0x0F];
        data[offset++] = high;
        data[offset++] = low;
        if ((i != 0) && colon) data[offset++] = ':';
    }
    return offset;
}

std::string to_mac_string(uint64_t mac, bool colon) {
    char buf[18];
    buf[write_mac_stream((uint8_t*)buf, 0, mac, colon)] = 0;
    return buf;
}

uint64_t to_mac_uint64(const std::string& macStr) {
	if(std::ranges::count(macStr, ':') == 5) {
		std::stringstream ss(macStr);
		std::string part;
		uint64_t result = 0;
		int count = 0;
		try {
		while (std::getline(ss, part, ':')) {
			uint64_t byte = 0;
			if (!part.empty()) {
				byte = std::stoul(part, nullptr, 16);
				if (byte > 0xFF) return 0L;
			}
			result = (result << 8) | byte;
			count++;
		}
		} catch(std::exception &e) {
			return 0L;
		}
		if (count != 6) return 0L;
		return result;
	} else {
		std::string clean;
		for (unsigned char c : macStr)
			if (std::isxdigit(c))
				clean += c;
		if (clean.length() != 12) return 0L;

		uint64_t result = 0;
		for (size_t i = 0; i < 12; i += 2) {
			std::string byteStr = clean.substr(i, 2);
			uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
			result = (result << 8) | byte;
		}

		return result;
	}
}

#define COMPRESSION_BUFFER_SIZE 12288

uint8_t* zlib_compress(uint8_t* data, size_t dsize, size_t& osize) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw(std::runtime_error("deflateInit2 failed while compressing."));
    }
    zs.next_in = reinterpret_cast<Bytef*>(data);
    zs.avail_in = static_cast<uInt>(dsize); // set the input data
    int ret;
    uint8_t* out = new uint8_t[COMPRESSION_BUFFER_SIZE];
    uint8_t buffer[4096];
    osize = 0;
    // retrieve the compressed bytes blockwise
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buffer);
        zs.avail_out = 4096;
        ret = deflate(&zs, Z_FINISH);
		if(osize + zs.total_out > 10240) {
			std::lock_guard<std::mutex> lock(cerr_mutex);
			std::cerr << "Compression: Data is too long" << std::endl; // TODO: Log
			osize = 0;
			return nullptr;
		}
        memcpy(out + osize, buffer, zs.total_out);
        osize += zs.total_out;
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END) { // an error occurred that was not EOF
		std::lock_guard<std::mutex> lock(cerr_mutex);
		std::cerr << "Error during compression: " << zs.msg << std::endl; // TODO: Log
		osize = 0;
		return nullptr;
    }
    return out;
}

uint8_t* zlib_decompress(uint8_t* compressedData, size_t len, size_t& out_len) {
    out_len = 0;
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = static_cast<uInt>(len);
    stream.next_in = reinterpret_cast<Bytef*>(compressedData);

    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK)
        return nullptr;

    int ret;
	uint8_t* decompressedData = new uint8_t[len * 5];
	uint8_t* buffer[COMPRESSION_BUFFER_SIZE];
    do {
        stream.avail_out = COMPRESSION_BUFFER_SIZE;
        stream.next_out = reinterpret_cast<Bytef*>(buffer);
        ret = inflate(&stream, Z_NO_FLUSH);
        if (ret == Z_NEED_DICT or ret < 0) {
            out_len = 0;
            delete[] decompressedData;
            return nullptr;
        }
        int have = COMPRESSION_BUFFER_SIZE - stream.avail_out;
        memcpy(decompressedData + out_len, buffer, have);
        out_len += have;
    } while (ret != Z_STREAM_END);

    if (inflateEnd(&stream) != Z_OK) {
        out_len = 0;
        delete[] decompressedData;
        return nullptr;
    }
    return decompressedData;
}

static std::string make_host_key(const char* url) {
    const char* p = strstr(url, "https://");
    if(!p) return std::string();
    p += 8;
    const char* end = p;
    while(*end && *end != '/' && *end != ':' && *end != '?') ++end;
    std::string key("https://", 8);
    key.append(p, end - p);
    if(!proxy.empty()) {
        key += "|proxy=";
        key += proxy;
    }
    return key;
}

std::mutex pool_mutex;
std::unordered_map<std::string, std::vector<CURL*>> curl_pool;

static void apply_curl_opts(CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, CURL_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, CURL_TIMEOUT);
    if(!proxy.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
    }
}

static CURL* pop_curl(const std::string& host_key) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    auto& vec = curl_pool[host_key];
    if(!vec.empty()) {
        CURL* curl = vec.back();
        vec.pop_back();
        curl_easy_reset(curl);
        apply_curl_opts(curl);
        return curl;
    }
    CURL* curl = curl_easy_init();
    apply_curl_opts(curl);
    return curl;
}

static void push_curl(const std::string& host_key, CURL* curl) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    curl_pool[host_key].push_back(curl);
}


size_t curlWrite(const char* ptr, const size_t size, const size_t nmemb, void* buffer_ptr) {
    size_t realsize = size * nmemb;
	CurlBuffer* buff = reinterpret_cast<CurlBuffer*>(buffer_ptr);
	bool rloc = false;
    while(buff->len + realsize >= buff->capacity) {
		buff->capacity *= 2;
		rloc = true;
    }
	if(rloc) buff->data = reinterpret_cast<uint8_t*>(std::realloc(buff->data, buff->capacity));
	std::memcpy(buff->data + buff->len, ptr, realsize);
    buff->len += realsize;
    return realsize;
}

Response curl_request_get(const char* request_url,
                          std::vector<const char*> headers) {
	return curl_request_get(request_url, headers, -1);
}

Response curl_request_get(const char* request_url,
                          std::vector<const char*> headers,
						  int retries) {
    std::string host_key = make_host_key(request_url);
    CURL* curl = pop_curl(host_key);
    if(!curl) return Response(-1);

    char errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = '\0';
    CurlBuffer buff;

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    curl_slist* curl_headers = NULL;
    for(const char* header : headers)
        curl_headers = curl_slist_append(curl_headers, header);
    if(curl_headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

    curl_easy_setopt(curl, CURLOPT_URL, request_url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buff);

    CURLcode code = curl_easy_perform(curl);

    int http_code = 0;
    double time = 0;
    if(code == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &time);
    } else {
		{
			std::lock_guard<std::mutex> lock(cerr_mutex);
			std::cerr << "Error occurred while making request. Code: " << code
				<< " Error: " << errorBuffer << ". Retrying..." << std::endl;
		}
		int ret;
		if(retries == -1) ret = MAX_RETRIES - 1;
		else ret = retries - 1;
		if(ret < 0) {
			std::lock_guard<std::mutex> lock(cerr_mutex);
			std::cerr << "Error retrying. Retry attempts exceeded" << std::endl;
		} else {
			if(curl_headers)
				curl_slist_free_all(curl_headers);
			push_curl(host_key, curl);
			return curl_request_get(request_url, headers, ret);
		}
    }

    if(curl_headers)
        curl_slist_free_all(curl_headers);

    Response resp(code == CURLE_OK ? Response(buff, http_code, time)
                                  : Response(code, strdup(errorBuffer)));

    push_curl(host_key, curl);
    return resp;
}

Response curl_request_post(const char* request_url,
                           uint8_t* request_data,
                           size_t request_data_length,
                           std::vector<const char*> headers) {
	return curl_request_post(request_url, request_data, request_data_length, headers, -1);
}

Response curl_request_post(const char* request_url,
                           uint8_t* request_data,
                           size_t request_data_length,
                           std::vector<const char*> headers,
						   int retries) {
    std::string host_key = make_host_key(request_url);
    CURL* curl = pop_curl(host_key);
    if(!curl) return Response(-1);

    char errorBuffer[CURL_ERROR_SIZE];
    errorBuffer[0] = 0;
    CurlBuffer buff;

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

    curl_slist* curl_headers = NULL;
    for(const char* header : headers)
        curl_headers = curl_slist_append(curl_headers, header);
    if(curl_headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);

    curl_easy_setopt(curl, CURLOPT_URL, request_url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_data_length);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buff);

    CURLcode code = curl_easy_perform(curl);

    int http_code = 0;
    double time = 0;
    if(code == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &time);
    } else {
		{
			std::lock_guard<std::mutex> lock(cerr_mutex);
			std::cerr << "Error occurred while making request. Code: " << code
				<< " Error: " << errorBuffer << ". Retrying..." << std::endl;
		}
		int ret;
		if(retries == -1) ret = MAX_RETRIES - 1;
		else ret = retries - 1;
		if(ret < 0) {
			std::lock_guard<std::mutex> lock(cerr_mutex);
			std::cerr << "Error retrying. Retry attempts exceeded" << std::endl;
		} else {
			if(curl_headers)
				curl_slist_free_all(curl_headers);
			push_curl(host_key, curl);
			return curl_request_post(request_url,request_data, request_data_length, headers, ret);
		}
	}
    if(curl_headers)
        curl_slist_free_all(curl_headers);

    Response resp(code == CURLE_OK ? Response(buff, http_code, time)
                                  : Response(code, strdup(errorBuffer)));

    push_curl(host_key, curl);
    return resp;
}

} // namespace utils
