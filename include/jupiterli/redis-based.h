#pragma once

namespace jupiterli {
  namespace redis_based
  {
    void save_run_dets();
    void add_ts_point(const char* key, double ts, double value);
  }
}
