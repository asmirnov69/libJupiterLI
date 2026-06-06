#include <iostream>
#include <jupiterli.h>

using namespace std;

int main()
{
  for (int i = 0; i < 10; i++) {
    auto ts = i;
    jupiterli::add_ts_point("testloop", ts, i);
  }
  cout << "all done" << endl;    
}
