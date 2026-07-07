#include <jupiterli/redis-based.h>
#include <hiredis/hiredis.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include "utils.h"

using namespace std;
using json = nlohmann::json;

namespace jupiterli {
  namespace redis_based {
    redisContext* sub_ctx = nullptr;
    redisContext *stream_ctx = nullptr;
    string run_id = make_uuid();
    map<string, string> series_d; // key -> series_id    
    unsigned long run_serial_num = 0;
  
    bool wait_for_reply(redisContext *sub_ctx, std::string &out);    
    std::string two_way_call(redisContext *stream_ctx, redisContext *sub_ctx, const std::string &stream, const std::string &table, const json &row_json);
    json build_run_dets_row(const string& run_id);
    pair<bool, string> get_series_id(const string& key);
    void save_series_dets(const string& series_id, const string& key);
  }
}

// ---------- helper: read Pub/Sub message ----------
bool jupiterli::redis_based::wait_for_reply(redisContext *sub_ctx, std::string &out)
{
    redisReply *reply = nullptr;

    while (redisGetReply(sub_ctx, (void **)&reply) == REDIS_OK)
    {
        if (!reply) continue;

        // Pub/Sub message format:
        // [ "message", channel, data ]
        if (reply->type == REDIS_REPLY_ARRAY &&
            reply->elements == 3)
        {
            std::string type = reply->element[0]->str;
            if (type == "message")
            {
                out = reply->element[2]->str;
                freeReplyObject(reply);
                return true;
            }
        }

        freeReplyObject(reply);
    }

    return false;
}

// ---------- main two-way call ----------
std::string jupiterli::redis_based::two_way_call(redisContext *stream_ctx,
						 redisContext *sub_ctx,
						 const std::string &stream,
						 const std::string &table,
						 const json &row_json)
{
  std::string reply_channel = "reply:" + make_uuid();

  // 1. subscribe first
  redisCommand(sub_ctx, "SUBSCRIBE %s", reply_channel.c_str());
  
  auto row_json_s = row_json.dump();

  // 2. send stream message
  redisCommand(stream_ctx,
	       "XADD %s MAXLEN ~ 10000 * reply-to %s table %s row %b",
	       stream.c_str(),
	       reply_channel.c_str(),
	       table.c_str(),
	       row_json_s.data(),
	       row_json_s.size());
  
  // 3. wait for response
  std::string response;
  if (wait_for_reply(sub_ctx, response)) {
    return response;
  }
  
  return "";
}

json jupiterli::redis_based::build_run_dets_row(const string& run_id)
{
  char host[256];
  gethostname(host, sizeof(host));
  
  pid_t pid = getpid();
  
  // time.time()
  double created_ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();

  auto argv = get_command_line();
  std::size_t argc = argv.size();
  
  // sys.argv  
  std::string args;
  for (std::size_t i = 0; i < argc; i++) {
    if (i > 0) {
      args += " ";
    }
    args += argv[i];
  }

  // env fallback logic
  std::string run_label = get_env("RUN_LABEL");
  if (run_label.empty()) {
    run_label = get_env("RL");
  }

  json row = {
    {"run_id", run_id},
    {"created_ts", created_ts},
    {"host", std::string(host)},
    {"pid", pid},
    {"argv0", get_executable()},
    {"args", args},
    {"run_label", run_label}
  };
  
  return row;
}

pair<bool, string> jupiterli::redis_based::get_series_id(const string& key)
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

void jupiterli::redis_based::save_series_dets(const string& series_id, const string& key)
{  
  json row_json = {
    {"series_id", series_id},
    {"run_id", run_id},
    {"key", key}
  };
  std::string reply = two_way_call(stream_ctx, sub_ctx, "telemetry-admin", "series_dets", row_json);
  std::cout << "Reply: " << reply << std::endl;
}

void jupiterli::redis_based::save_run_dets()
{
  sub_ctx = redisConnect("127.0.0.1", 6379);
  stream_ctx = redisConnect("127.0.0.1", 6379);

  if (!stream_ctx || stream_ctx->err) {
    throw std::runtime_error("Stream connection failed");
  }

  if (!sub_ctx || sub_ctx->err) {
    throw std::runtime_error("PubSub connection failed");
  }  
  
  json row_json = build_run_dets_row(run_id);
  std::cout << "row_json: " << row_json << std::endl;
  //row_json["id"] = 123; row_json["name"] = "Alice";
  std::string reply = two_way_call(stream_ctx, sub_ctx, "telemetry-admin", "runs_dets", row_json);
  std::cout << "Reply: " << reply << std::endl;
}

void jupiterli::redis_based::add_ts_point(const char* key, double ts, double value)
{
  auto [is_new_key, series_id] = get_series_id(key);
  if (is_new_key) {
    save_series_dets(series_id, key);
  }
  
  json msg_json = {
    {"series_id", series_id},
    {"run_serial_num", ++run_serial_num}, // starting from 1
    {"timestamp", ts},
    {"value", value}
  };
  auto msg_json_s = msg_json.dump();
  
  redisCommand(stream_ctx,
	       "XADD %s MAXLEN ~ 10000 * data %b",
	       "telemetry",
	       msg_json_s.data(), msg_json_s.size());
}
