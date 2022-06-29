// C++ Effects library
// Maciej Pirog
// License: MIT

// Test: Commands that can return multiple arguments (in a tuple)

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

struct Cmd : public Command<int, int, std::string> { };

class H : public FlatHandler<void, Cmd> {
  int a = 100;
  int b = 100;
  std::string s = "hello!";
  void CommandClause(Cmd, Resumption<std::tuple<int, int, std::string>, void> r) override
  {
    std::move(r).Resume(a++, b--, s);
  }
};

void foo()
{
  for (int i = 0; i < 10; i++) {
    auto [a,b,s] = OneShot::InvokeCmd(Cmd{});
    std::cout << a << " " << b << " " << s << std::endl;
  }
}

int main()
{
  OneShot::Handle<H>(foo);
}
