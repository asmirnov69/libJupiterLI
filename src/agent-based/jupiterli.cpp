#include <random>
#include <sstream>
#include <iomanip>
#include <string>

#include <vector>
#include <unordered_map>
#include <iostream>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "jupiterli.h"
#include "circular-buffer.h"
#include "shared-dict.h"

using namespace std;

std::string generate_uuid_v4()
{
  // Seed with a real random number if available
  std::random_device rd;
  // Use the 64-bit Mersenne Twister engine for speed and quality
  std::mt19937_64 engine(rd());

  std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFF);

  uint64_t parts[2];
  parts[0] = dist(engine);
  parts[1] = dist(engine);

  // Set variant to RFC 4122/9562 (bits 64-65 to 0b10)
  parts[1] = (parts[1] & 0x3FFFFFFFFFFFFFFF) | 0x8000000000000000;
  // Set version to 4 (bits 12-15 of the first 64 bits to 0x4)
  parts[0] = (parts[0] & 0xFFFFFFFFFFFF0FFF) | 0x0000000000004000;

  std::stringstream ss;
  ss << std::hex << std::setfill('0')
     << std::setw(8) << (parts[0] >> 32) << "-"
     << std::setw(4) << ((parts[0] >> 16) & 0xFFFF) << "-"
     << std::setw(4) << (parts[0] & 0xFFFF) << "-"
     << std::setw(4) << (parts[1] >> 48) << "-"
     << std::setw(12) << (parts[1] & 0xFFFFFFFFFFFF);

  return ss.str();
}


struct TelemetryPoint {
  unsigned long serial_no;
  double ts, v;
};

SharedDict runs;
vector<CircularBuffer<TelemetryPoint>*> ringbufs;
unordered_map<string, int> keys_d; // key -> idx in ringbufs
string run_id = generate_uuid_v4();

static size_t get_ringbuf_idx(const char* key, const string& run_id)
{
  auto it = keys_d.find(key);
  if (it == keys_d.end()) {
    string series_id = generate_uuid_v4();
    ostringstream series_o;
    series_o << "{ " << "series_id: " << series_id << " }";
    runs.append(run_id, series_o.str());
    string fn = "series--" + series_id;
    auto capacity = 10000;
    size_t bytes = CircularBuffer<TelemetryPoint>::bytes_required(capacity);
    int fd = shm_open(fn.c_str(), O_CREAT | O_RDWR, 0666);
    ftruncate(fd, bytes);
    void* mem = mmap(nullptr, bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);    
    CircularBuffer<TelemetryPoint>* b = reinterpret_cast<CircularBuffer<TelemetryPoint>*>(mem);
    b->initialize(capacity);
    auto new_idx = ringbufs.size();
    ringbufs.push_back(b);
    //it = keys_d.insert({key, new_idx}).first;
    keys_d[key] = new_idx; it = keys_d.find(key);
  }

  int w_ringbuf_idx = it->second;
  return w_ringbuf_idx;
}

void jupiterli::save_run_dets()
{
  cout << "jupiterli::save_run_dets()" << endl;
  ostringstream m;
  m << "{"
    << "run_id: " << run_id << ", "
    << "pid: " << getpid()
    << "}";
  runs.append(run_id, m.str());
}

void jupiterli::add_ts_point(const char* key, double ts, double v)
{
  cout << "jupiterli::add_ts_point: " << key << " " << (void*)key << " " << ts << " " << v << endl;

  auto w_ringbuf_idx = get_ringbuf_idx(key, run_id);
  auto ringbuf = ringbufs.at(w_ringbuf_idx);
  while (!ringbuf->push_back({0, ts, v})) {}
  //ringbuf->push_back({0, ts, v});
}

