#include <random>
#include <fstream>
#include <unistd.h>
#include <limits.h>
#include "utils.h"

std::string jupiterli::make_uuid()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  
  const char *hex = "0123456789abcdef";
  
  std::string uuid = std::string(36, ' ');
  
  for (int j = 0; j < 36; j++) {
    if (j == 8 || j == 13 || j == 18 || j == 23) {
      uuid[j] = '-';
    } else {
      uuid[j] = hex[dis(gen)];
    }
  }

  return uuid;
}

std::vector<std::string> jupiterli::get_command_line()
{
  std::ifstream f("/proc/self/cmdline", std::ios::binary);

  std::vector<std::string> args;
  std::string arg;

  while (std::getline(f, arg, '\0'))
    args.push_back(arg);

  return args;
}

// helper: get executable path (Linux)
std::string jupiterli::get_executable()
{
  char buf[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len != -1) {
    buf[len] = '\0';
    return std::string(buf);
  }
  return "";
}

// helper: get env var with fallback
std::string jupiterli::get_env(const char* key)
{
  const char* v = std::getenv(key);
  return v ? std::string(v) : "";
}
