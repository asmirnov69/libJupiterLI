#include "jupiterli.h"
#include "circular-buffer.h"
#include <vector>
#include <unordered_map>
//#include <map>
#include <iostream>

using namespace std;

struct TelemetryPoint {
  unsigned long serial_no;
  double ts, v;
};

vector<CircularBuffer<TelemetryPoint>> ringbufs;
unordered_map<string, int> keys_d; // key -> idx in ringbufs

static size_t get_ringbuf_idx(const char* key)
{
  auto it = keys_d.find(key);
  if (it == keys_d.end()) {
    cout << "insert " << keys_d.size() << " " << key << endl;
    auto uuid = "uuid-goes-here";
    CircularBuffer<TelemetryPoint> b;
    b.initialize(sizeof(TelemetryPoint) * 10000);
    auto new_idx = ringbufs.size();
    ringbufs.emplace_back(std::move(b));
    //it = keys_d.insert({key, new_idx}).first;
    keys_d[key] = new_idx; it = keys_d.find(key);
  }

  cout << "v: " << ringbufs.size() << " " << keys_d.size() << endl;
  for (auto it: keys_d) {
    cout << "keys_d: " << it.first << " " << it.second << endl;
  }
  int w_ringbuf_idx = it->second;
  cout << "w_ringbuf_idx: " << w_ringbuf_idx << endl;
  return w_ringbuf_idx;
}

void jupiterli::save_run_dets()
{
  cout << "jupiterli::save_run_dets()" << endl;
}

void jupiterli::add_ts_point(const char* key, double ts, double v)
{
  cout << "jupiterli::add_ts_point: " << key << " " << (void*)key << " " << ts << " " << v << endl;

  auto w_ringbuf_idx = get_ringbuf_idx(key);
  auto& ringbuf = ringbufs.at(w_ringbuf_idx);
  //while (!ringbuf.push_back({0, ts, v})) {}
  ringbuf.push_back({0, ts, v});
}

