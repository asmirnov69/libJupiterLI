#include <iostream>
#include <shared-dict.h>

int main()
{
  SharedDict runs;

  auto ks = runs.keys();
  for(const auto& k : ks) {
    std::cout << k << "\n";
    
    auto h = runs.find(k);
    auto it = runs.values(h);
    
    while(it.valid()) {
      std::cout << "  " << it.value() << "\n";
      it.next();
    }
  }
}
