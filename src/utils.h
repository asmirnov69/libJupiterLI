#pragma once
#include <string>
#include <vector>

namespace jupiterli {
  std::string make_uuid();
  std::vector<std::string> get_command_line();
  std::string get_executable();
  std::string get_env(const char* key);
}
