#include <libjupiterli/libjupiterli.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "utils.h"

using namespace std;


map<string, string> series_d; // key -> series_id    
unsigned long run_serial_num = 0;

static pair<bool, string> get_series_id(const string& run_id, const string& key)
{
  auto it = series_d.find(key);
  bool new_key = it == series_d.end();
  string series_id;
  if (new_key) {
    series_id = run_id + "---" + std::to_string(std::hash<std::string>{}(key));
    series_d[key] = series_id;
  } else {
    series_id = (*it).second;
  }
  
  return {new_key, series_id};
}

void libjupiterli::save_run_dets()
{
  auto row_j = get_run_dets();
  httplib::Client cli("http://h1:8000");
  auto req = nlohmann::json{{"table", "runs_dets"}, {"row", row_j.get<string>()}};
  if (auto res = cli.Post("/app/add-row", req.get<string>(), "application/json")) {
    cout << "save_run_dets: " << res->status << endl;
    cout << "save_run_dets: " << res->body << endl;
  }
}

void libjupiterli::save_series_dets(const char* key)
{
  //#pragma message("NB: save_series_dets disabled")
  //#if 0
  auto row_j = get_series_dets(key);
  httplib::Client cli("http://h1:8000");
  auto req = nlohmann::json{{"table", "series_dets"}, {"row", row_j.get<string>()}};
  if (auto res = cli.Post("/app/add-row", req.get<string>(), "application/json")) {
    cout << "save_series_dets: " << res->status << endl;
    cout << "save_series_dets: " << res->body << endl;
  }
  //#endif
}

void libjupiterli::add_ts_point(const char* key, double ts, double value)
{
}

void libjupiterli::add_serial_point(const char* key, double value)
{
}

  
