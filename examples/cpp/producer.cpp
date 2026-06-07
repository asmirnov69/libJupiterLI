#include <iostream>
#include <jupiterli.h>
using namespace std;

int main()
{
  jupiterli::save_run_dets();
  for (int i = 0; i < 100; i++) {
    cout << i << endl;
    jupiterli::add_ts_point("test", i, i);
  }
}
