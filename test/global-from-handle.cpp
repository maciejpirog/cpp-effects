// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Swapping handlers

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

struct Break : Command<void> { };

class HH : public Handler<int, void, Break> {
  int CommandClause(Break, Resumption<void, int>) override {
    return 100;
  }
  int ReturnClause() override {
    return 10;
  }
};

int g = OneShot::Handle<HH>([](){ OneShot::InvokeCmd(Break{}); });

int main()
{
  std::cout << "--- global-from-handle ---" << std::endl;
  std::cout << g << std::endl;
}
