#include <iostream>
#include <chrono>
#include <unistd.h>
#include <libjupiterli/libjupiterli.h>

#include <MQTTPacket.h>

using namespace std;
namespace jli = libjupiterli;

int main()
{
  jli::save_run_dets();
  for (int i = 0; i < 100; i++) {
    double ts = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    cout << ts << " " << i << endl;
    jli::add_ts_point("test", ts, i);
    sleep(1);
  }
}
