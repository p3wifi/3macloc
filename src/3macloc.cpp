#define TMACLOC_VERSION "1.5"

#define TMACLOC "3macloc v" TMACLOC_VERSION

//#define CURL_STATICLIB
//#define _CRT_SECURE_NO_WARNINGS
//#pragma warning(disable : 4996)

#include <iostream>
#include <cstdint>
#include <iomanip>
#include <vector>
#include <future>
#include <string>
#include <cstring>
#include <csignal>
#include <unordered_map>
#include <algorithm>

#define TMACLOC_WEBSERVER

#ifdef TMACLOC_WEBSERVER
#define CROW_JSON_USE_MAP
#include "crow.h"
#include "index_html_gz.h"
#endif

#include <json.hpp>

#include "google.hpp"
#include "googlem.hpp"
#include "yandex.hpp"
#include "apple.hpp"
#include "microsoft.hpp"
#include "mylnikov.hpp"
#include "vendor.hpp"
#include "wigle.hpp"
#include "beacondb.hpp"
#include "precisely.hpp"
#include "skyhook.hpp"
#include "combain.hpp"
#include "wifidb.hpp"
#include "unwiredlabs.hpp"
#include "here.hpp"

//#include "results_share.cpp"
#include "utils.hpp"

using json = nlohmann::json;
using namespace utils;

#define ISHEXCHR(s) ((s >= '0' && s <= '9') || (s >= 'a' && s <= 'f') || (s >= 'A' && s <= 'F'))
#define ISDOTCHR(s) (s == ':' || s == '-' || s == '_')

bool get_bssid_from_user(uint64_t &bssid) {
	char sbssid[100];
	std::cin >> std::setw(50) >> sbssid;
	return (bssid = to_mac_uint64(sbssid)) > 0;
}

inline void print_help() {
	std::cout << TMACLOC << '\n' << 
R"(Open source WiFi BSSID locator.

Usage:
  3macloc [options] [bssid ...]

Argument:
  [bssid ...]       router BSSID(s) (or MAC).
                    Example: 11:22:33:44:55:66 | 11-22-33-44-55-66 | 112233445566

Options:
  -                 Read router BSSID(s) from pipe.
  -h, --help        Print help message.
  -P, --print-all   Output providers that fail to determine location.
  -b <string>       BSSID to search.
  --precision <int> Output float precision (default 7).
  --json            Print result in JSON format.
  -D                Disable all source, to enable some use -D?.
  -D?               Enable/Disable source, for source list use -l.
  -l                List of geolocation providers.
  -t <int>          Timeout for operation (1-60s), default 5s.
  -s                Start web server.
  --port            Web server port.
  -r                Max retries (default 3).
  --singlethread    Disable concurrent requests.

OS environment key used:
combain_key, here_key, wigle_key, precisely_key,
skyhook_key, unwiredlabs_key

)";
}

struct Settings {
	bool print_help = false;
	bool print_all = false;
	bool print_to_json = false;
	bool disable_multithread = false;
	bool bssid_in_args = false;
	bool run_server = false;
	std::vector<uint64_t> bssids;
	int precision = 7;
	int port = 58778;
};

struct Provider {
	bool disabled;
	bool publ;
	char hotkey;
	const char *name;
	Location (*get)(uint64_t);
};

std::vector<Provider> Providers = {
	{0, 1, 'G', "Google",      google_loc::get_location},
	{0, 1, 'Y', "Yandex",      yandex_loc::get_location},
	{0, 1, 'A', "Apple",       apple_loc::get_location},
	{0, 1, 'N', "AppleChina",  apple_loc::get_location_china},
	{0, 1, 'M', "Microsoft",   microsoft_loc::get_location},
	{0, 0, 'P', "Precisely",   precisely_loc::get_location},
	{0, 0, 'S', "SkyHook",     skyhook_loc::get_location},
	{0, 0, 'W', "WiGLE",       wigle_loc::get_location},
	{0, 0, 'C', "Combain",     combain_loc::get_location},
	{0, 0, 'H', "Here",        here_loc::get_location},
	{0, 1, 'I', "Mylnikov",    mylnikov_loc::get_location},
	{1, 1, 'B', "BeaconDB",    beacondb_loc::get_location},
	{1, 1, 'D', "WiFiDB",      wifidb_loc::get_location},
	{1, 0, 'g', "GoogleMaps",  googlem_loc::get_location},
	{1, 0, 'U', "UnwiredLabs", unwiredlabs_loc::get_location}
};
std::unordered_map<std::string, size_t> provIdx;

#define CMPARG(x) strcmp(argv[i], x) == 0

inline Settings get_settings(int argc, char** argv) {
	Settings settings;
	uint64_t bssid;
	for(int i = 1; i < argc; i++) {
		if(CMPARG("-b"))  {
			if(i + 1 >= argc) { // if no int in args. Example: 3macloc -b
				std::cerr << "Invalid argument: -b expects bssid\n";
			} else {
				settings.bssid_in_args = (bssid = to_mac_uint64(argv[i + 1])) > 0;
				if(!settings.bssid_in_args) {
					std::cerr << "Invalid BSSID" << std::endl;
				} else 	settings.bssids.push_back(bssid);
				i++;
			}
		}
		else if(CMPARG("-")) {
			std::string sbssid;
			while (std::cin >> sbssid) {
			    bssid = to_mac_uint64(sbssid);
			    if(bssid > 0) {
			        settings.bssids.push_back(bssid);
			        settings.bssid_in_args = true;
			    }
			}
		}
		else if(CMPARG("-t")) {
			if(i + 1 >= argc) {
				std::cerr << "Invalid argument: -t expects int" << std::endl;
			} else { 
				int val = atoi(argv[i + 1]);
				if(val) {utils::CURL_TIMEOUT = val & 0x3F; i++;}
			}
		}
		else if(CMPARG("-r")) {
			if(i + 1 >= argc) {
				std::cerr << "Invalid argument: -r expects int" << std::endl;
			} else { 
				int val = atoi(argv[i + 1]);
				if(val) {utils::MAX_RETRIES = val; i++;}
			}
		}
		else if(CMPARG("--port")) {
			if(i + 1 >= argc) {
				std::cerr << "Invalid argument: --port expects int" << std::endl;
			} else { 
				int val = atoi(argv[i + 1]);
				if(val) { settings.port = val; i++;}
				settings.run_server = true;
			}
		}
		else if(CMPARG("--json")) settings.print_to_json = true;
		else if(CMPARG("--precision")) {
			if(i + 1 >= argc) { // if no int in args. Example: 3macloc --precision
				std::cerr << "Invalid argument: --precision expects int" << std::endl;
			} else { // if int in args. Example: 3macloc --precision 20
				int val = atoi(argv[i + 1]);
				if(val) { settings.precision = val; i++;}
			}
		}
		else if(CMPARG("-D")) {
			for (size_t i = 0; i < Providers.size(); i++) {
				Providers[i].disabled = 1;
			}
		}
		else if(strncmp(argv[i], "-D", 2) == 0) {
			unsigned char hk = argv[i][2];
			for (size_t i = 0; i < Providers.size(); i++) {
				if(hk == Providers[i].hotkey) {Providers[i].disabled ^= 1;}
			}
		}
		else if(CMPARG("-P") or CMPARG("--print-all")) settings.print_all = true;
		else if(CMPARG("--help") or CMPARG("-h")) {
			print_help();
			settings.print_help = true;
			break;
		}
		else if(CMPARG("--singlethread")) settings.disable_multithread = true;
		else if(CMPARG("-l")) {
			std::cout << "List of geolocation providers:" << std::endl;
			for (size_t i = 0; i < Providers.size(); i++) {
			  char dis = ' ';
			  if(Providers[i].disabled) dis = '-';
			  std::cout << " " << dis << "  " << Providers[i].name << "	for toggle enable/disable: -D" << Providers[i].hotkey << std::endl;
			}
			settings.print_help = true;
			break;
		} else if(CMPARG("-s"))  {
			settings.run_server = true;
		}
		else {
			settings.bssid_in_args = (bssid = to_mac_uint64(argv[i])) > 0;
			if(!settings.bssid_in_args) {
				std::cerr << "Invalid argument: " << argv[i] << '\n';
			} else 	settings.bssids.push_back(bssid);
		}
	}
	if(argc == 1) {
		print_help();
		settings.run_server = true;
	}
	return settings;
}

int main(int argc, char** argv) {
	for (size_t i = 0; i < Providers.size(); i++) {provIdx[Providers[i].name] = i;}
	Settings settings = get_settings(argc, argv);
	if(settings.print_help) return 0;
#ifdef TMACLOC_WEBSERVER
	if(settings.run_server) {
		crow::SimpleApp app;
		// app.loglevel(crow::LogLevel::WARNING);
		CROW_ROUTE(app, "/")([](){
			crow::response res;
			res.code = 200;
			res.set_header("Content-Type", "text/html; charset=utf-8");
			res.set_header("Content-Encoding", "gzip");
			res.body.assign(
				reinterpret_cast<const char*>(index_html_gz),
				index_html_gz_len
			);

			return res;
		});
		CROW_ROUTE(app, "/status")([](){
			return crow::json::wvalue({{"ok", true}});
		});
		CROW_ROUTE(app, "/providers")([](){
			auto provs = crow::json::wvalue::list();
			for (size_t i = 0; i < Providers.size(); i++) {
				provs.push_back({{"name", Providers[i].name},{"public", Providers[i].publ}, {"id", i}, {"disabled", Providers[i].disabled}});
			}
			crow::json::wvalue x({{"providers", provs}});
			return x;
		});
		CROW_ROUTE(app, "/geolocate").methods("POST"_method)([settings](const crow::request& req){
			auto x = crow::json::load(req.body);
			if (!x) return crow::response(400);
			if(!x.has("bssid") || !x.has("providers")) return crow::response(400);
			if(x["bssid"].t() != crow::json::type::String) return crow::response(400);
			if(x["providers"].t() != crow::json::type::List) return crow::response(400);
			uint64_t bssid;
			if(!(bssid = utils::to_mac_uint64(x["bssid"].s()))) return crow::response(400);

			bool local_singlethread = x.has("singlethread") && x["singlethread"].t() == crow::json::type::True;
			if(x.has("timeout") && x["timeout"].t() == crow::json::type::Number) utils::CURL_TIMEOUT = std::max(int64_t(1), std::min(int64_t(60), x["timeout"].i()));

			std::string req_proxy;
			if(x.has("proxy") && x["proxy"].t() == crow::json::type::String) {
				req_proxy = x["proxy"].s();
			}

			for(auto& iv : x["providers"].lo()) if(iv.t() != crow::json::type::Number) return crow::response(400);

			auto entry = crow::json::wvalue::object();
			crow::json::wvalue resp;

			if (settings.disable_multithread || local_singlethread || x["providers"].size() <= 1) {
				resp["singlethread"] = true;
				for(auto& iv : x["providers"]) {
					int i = iv.i();
					utils::proxy = req_proxy;
					Location loc = Providers[i].get(bssid);
					auto p = &entry[std::to_string(i)];
					(*p)["found"] = loc.valid;
					if(loc.valid) {
						(*p)["location"] = {{"lat", loc.lat},{"lon", loc.lon}};
						if(loc.accuracy != 0)
							(*p)["location"]["accuracy"] = loc.accuracy; 
					}
					if(loc.code != 0) {
						(*p)["error_code"] = loc.code;
						(*p)["error"] = loc.get_error_str();
					}
				}
			} else {
				std::vector<std::pair<Provider, int>> local_providers;
				auto selected_provs = x["providers"].lo();
				local_providers.reserve(selected_provs.size());

				std::transform(selected_provs.begin(), selected_provs.end(), std::back_inserter(local_providers),
						[](auto i) { return std::pair(Providers.at(i.i()), i.i()); });

				std::vector<std::thread> threads;
				std::vector<std::future<Location>> futures;
				for(size_t i = 0; i < local_providers.size(); i++) {
					auto fn = local_providers[i].first.get;
					std::packaged_task<Location(uint64_t)> task([req_proxy, fn](uint64_t bssid) {
						utils::proxy = req_proxy;
						return fn(bssid);
					});
					futures.push_back(task.get_future());
					threads.emplace_back(std::move(task), bssid);
				}
				for(size_t i = 0; i < local_providers.size(); i++) {
					Location loc = futures[i].get();
					auto p = &entry[std::to_string(local_providers[i].second)];
					(*p)["found"] = loc.valid;
					if(loc.valid) {
						(*p)["location"] = {{"lat", loc.lat},{"lon", loc.lon}};
						if(loc.accuracy != 0)
							(*p)["location"]["accuracy"] = loc.accuracy; 
					}
					if(loc.code != 0) {
						(*p)["error_code"] = loc.code;
						(*p)["error"] = loc.get_error_str();
					}
				}
				for (auto& t : threads) t.join();
			}
			utils::proxy.clear();
			resp["results"] = entry;
			resp["success"] = true;
			return crow::response(resp);
		});
		CROW_ROUTE(app, "/multigeo").methods("POST"_method)([settings](const crow::request& req){
			auto x = crow::json::load(req.body);
			if (!x) return crow::response(400);
			if(!x.has("bssids")) return crow::response(400);
			if(x["bssids"].t() != crow::json::type::List) return crow::response(400);
			int provider = 2; // apple
			if(x.has("provider")) {
				if(x["provider"].t() != crow::json::type::Number) return crow::response(400);
				provider = x["provider"].i();
			}
			std::vector<uint64_t> bssids(x["bssids"].lo().size());
			int bi = 0;
			for(auto bssid : x["bssids"]) {
				if(!(bssids[bi++] = utils::to_mac_uint64(bssid.s()))) return crow::response(400);
			}
			if(x.has("timeout") && x["timeout"].t() == crow::json::type::Number)
				utils::CURL_TIMEOUT = std::max(int64_t(1), std::min(int64_t(60), x["timeout"].i()));

			if(x.has("proxy") && x["proxy"].t() == crow::json::type::String) {
				utils::proxy = x["proxy"].s();
			}
			
			std::vector<Location> locations;
			if(provider == 0) locations = google_loc::get_location_multi(bssids);
			else if(provider == 2) locations = apple_loc::get_location_multi(bssids, false);
			else if(provider == 3) locations = apple_loc::get_location_multi(bssids, true);

			utils::proxy.clear();
			auto entry = crow::json::wvalue::object();
			for(Location loc : locations) {
				auto p = &entry[utils::to_mac_string(loc.sourceBssid)];
				(*p)["found"] = loc.valid;
				if(loc.valid) {
					(*p)["location"] = {{"lat", loc.lat},{"lon", loc.lon}};
					if(loc.accuracy != 0)
						(*p)["location"]["accuracy"] = loc.accuracy; 
				}
				if(loc.code != 0) {
					(*p)["error_code"] = loc.code;
					(*p)["error"] = loc.get_error_str();
				}
			}
			crow::json::wvalue resp;
			resp["results"] = entry;
			resp["success"] = true;
			return crow::response(resp);
		});
		app.signal_clear();
		app.port(settings.port).run();
		return 0;
	}
#endif // TMACLOC_WEBSERVER
	Providers.erase(std::remove_if(Providers.begin(), Providers.end(),
		[](const Provider& p) {return p.disabled;}), Providers.end());

	json j;
	j["success"] = true;
	j["results"] = json::array();
	for (auto bssid : settings.bssids) { // all bssids
	   json entry;
	   entry["bssid"] = to_mac_string(bssid);
	   std::ostringstream out;
	   bool no_results = true;
	   if (settings.disable_multithread) {
	        if(!settings.print_to_json) {
	            std::string buf = vendor::get(bssid);
	            std::cout << "Results for " << to_mac_string(bssid) << " (" << buf << ")" << std::endl;
		}
		for(size_t i = 0; i < Providers.size(); i++) {
		    Location loc = Providers[i].get(bssid);
		    entry[toLower(Providers[i].name)] = loc.to_json();
		    if(settings.print_all or loc.valid) {
		         out << std::fixed << std::setprecision(settings.precision)
		             << Providers[i].name << ": " << loc.lat << " " << loc.lon << std::endl;
		         no_results = false;
		    }
		}
	   } else {
		std::packaged_task<std::string(uint64_t)> task(vendor::get);
		std::future<std::string> svend = task.get_future();
		std::thread vendwkr(std::move(task), bssid);
		std::vector<std::thread> threads;
		std::vector<std::future<Location>> futures;
		for(size_t i = 0; i < Providers.size(); i++) {
		    std::packaged_task<Location(uint64_t)> task(Providers[i].get);
		    futures.push_back(task.get_future());
		    threads.emplace_back(std::move(task), bssid);
		}
		std::string buf = svend.get();
		out << "Results for " << to_mac_string(bssid) << " (" << buf << ")" << std::endl;
		for(size_t i = 0; i < Providers.size(); i++) {
		    Location loc = futures[i].get();
		    entry[toLower(Providers[i].name)] = loc.to_json();
		    if(settings.print_all or loc.valid) {
		         out << std::fixed << std::setprecision(settings.precision)
		             << Providers[i].name << ": " << loc.lat << " " << loc.lon << std::endl;
		         no_results = false;
		    }
			if(loc.code != 0 && loc.code != utils::error_codes::no_token) {
				out << "Error requesting " << Providers[i].name << " provider. "
					<< loc.code << " " << loc.get_error_str() << std::endl;
			}
		}
	        vendwkr.join();
		for (auto& t : threads) t.join();
	   }
	   if(no_results) out << "no results" << std::endl;
	   if(!settings.print_to_json)
	        std::cout << out.str() << std::endl;
  	   j["results"].push_back(entry);

	   //load_results_to_server(results, bssid);
	} // all bssids
	if(settings.print_to_json)
	   std::cout << j.dump() << std::endl;
	return 0;
}
