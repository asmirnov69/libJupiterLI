#pragma once

namespace libjupiterli {
  void save_run_dets();
  void add_ts_point(const char* key, double ts, double value);
  void add_serial_point(const char* key, double value);
}
