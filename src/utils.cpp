#include <random>
#include <fstream>
#include <unistd.h>
#include <limits.h>
#include "utils.h"

using namespace std;

std::string libjupiterli::make_uuid()
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

std::vector<std::string> libjupiterli::get_command_line()
{
  std::ifstream f("/proc/self/cmdline", std::ios::binary);

  std::vector<std::string> args;
  std::string arg;

  while (std::getline(f, arg, '\0'))
    args.push_back(arg);

  return args;
}

// helper: get executable path (Linux)
std::string libjupiterli::get_executable()
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
std::string libjupiterli::get_env(const char* key)
{
  const char* v = std::getenv(key);
  return v ? std::string(v) : "";
}

nlohmann::json libjupiterli::get_run_dets()
{
  std::string run_id = make_uuid();
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

  nlohmann::json row{
    {"table__", "runs"},
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

