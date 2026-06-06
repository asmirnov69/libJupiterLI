#include "jupiterli.h"
#include <iostream>

using namespace std;

vector<CircularBuffer<T>> ringbufs;
unordered_map<string, int> keys_d; // key -> ringbuf idx in ringbufs

static size_t get_ringbuf_idx(const char* key, size_t vtype_size)
{
  auto it = keys_d.find(key);
  if (it == keys_d.end()) {
    string uuid = ...;
    ringbuf_t* ringbuf_o = new ringbuf_t();
    ringbuf_setup(ringbuf_o, 1, 10000 * vtype_size);
    auto ringbuf_worker = ringbuf_register(ringbuf_o, 0);
    ringbufs.emplace_back(std::move({ringbuf_o, ringbuf_worker}));
    auto new_idx = ringbufs.size();
    //int some_v = expensive_computation();
    it = keys_d.insert({key, new_idx}).first;
  }

  int w_ringbuf_idx = it->second;
  return w_ringbuf_idx;
}

void jupiterli::add_ts_point(const char* key, double ts, double v)
{
  cout << "jupiterli::add_ts_point: " << key << " " << ts << " " << v << endl;

  // find ringbuf corresponding to key
  auto w_ringbuf_idx = get_ringbuf_idx(key);
  auto w_ringbuf_o, w_ringbuf_worker = ringbufs.at(w_ringbuf_idx);
  auto w_offset = ringbuf_acquire(w_ringbuf_o, w_ringbuf_worker, sizeof(tuple<double, double>));
  if (w_offset == -1) { // need to release one spot
    ringbuf_consume(w_ringbuf_o, &w_offset); // offset will be set by consume ???
    ringbuf_release(w_ringbuf_o, sizeof(...) * 1);
  }
  memcpy(&(w_ringbuf->workers[0].data[w_offset]), sizeof(...), {ts, v});
  ringbuf_produce(w_ringbuf_o, w_ringbuf_worker);
  
}

