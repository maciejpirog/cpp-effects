// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Handlers can be used to define global values

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

struct Break : eff::command<> { };

class HH : public eff::handler<int, void, Break> {
  int handle_command(Break, eff::resumption<int()>) override {
    return 100;
  }
  int handle_return() override {
    return 10;
  }
};

int g = eff::handle<HH>([](){ eff::invoke_command(Break{}); });

int main()
{
  std::cout << "--- global-from-handle ---" << std::endl;
  std::cout << g << std::endl;
}
