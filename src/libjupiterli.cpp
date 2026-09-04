#include <libjupiterli/libjupiterli.h>
#include <libjupiterli/mqtt-client.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "utils.h"

#include <MQTTPacket.h>
using namespace std;

map<string, string> series_ids; // key -> series_id    
string run_id = libjupiterli::make_uuid();
unsigned long run_serial_num = 0;
MqttSyncClient mqttc;

void libjupiterli::save_run_dets()
{
  if (mqttc.socket_fd == -1) {
    if (!mqttc.connect_to_broker("127.0.0.1", 1883, "libjuputerli")) {
      throw std::runtime_error("libjuputerli::save_run_dets: can't connect to mqtt broker");
    }
  }
  
  httplib::Client cli("http://h1:8000");
  auto req_j = get_run_dets(run_id);
  auto req = req_j.dump();
  if (auto res = cli.Post("/api/add-row", req, "application/json")) {
    cout << "save_run_dets: " << res->status << endl;
    cout << "save_run_dets: " << res->body << endl;
  }
}

static void save_series_dets(const string& series_id, const string& run_id, const char* key)
{
  httplib::Client cli("http://h1:8000");
  auto req_j = nlohmann::json{{"table__", "series"}, {"series_id", series_id}, {"run_id", run_id}, {"key", key}};
  auto req = req_j.dump();
  if (auto res = cli.Post("/api/add-row", req, "application/json")) {
    cout << "save_series_dets: " << res->status << endl;
    cout << "save_series_dets: " << res->body << endl;
  }
}

static pair<bool, string> get_series_id(const char* key)
{
  auto it = series_ids.find(key);
  if (it != series_ids.end()) {
    return {false, (*it).second};
  }
  auto new_series_id = run_id + "---" + std::to_string(std::hash<std::string>{}(key));
  series_ids[key] = new_series_id;
  return {true, new_series_id};
}

void libjupiterli::add_ts_point(const char* key, double ts, double value)
{
  auto [is_new_key, series_id] = get_series_id(key);
  if (is_new_key) {
    save_series_dets(series_id, run_id, key);
  }
  run_serial_num++;
  auto rec_j = nlohmann::json::array({{{"table__", "series_points"}, {"series_id", series_id}, {"timestamp", ts}, {"value", value}, {"run_serial_num", run_serial_num}}});
  auto rec = rec_j.dump();
  mqttc.publish_string(std::format("telemetry/series/{}", series_id), rec);
}

void libjupiterli::add_serial_point(const char* key, double value)
{
  add_ts_point(key, -1.0, value);
}

  
