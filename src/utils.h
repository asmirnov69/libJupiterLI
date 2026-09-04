#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace libjupiterli
{
  std::string make_uuid();
  std::vector<std::string> get_command_line();
  std::string get_executable();
  std::string get_env(const char* key);

  nlohmann::json get_run_dets(const std::string& run_id);
  nlohmann::json get_series_dets(const char* key);
}
