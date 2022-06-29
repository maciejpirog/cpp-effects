// C++ Effects library
// Maciej Pirog
// License: MIT

// Test: Commands that can return multiple arguments (in a tuple)

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

struct Cmd1 : public Command<int, int, std::string> { };
struct Cmd2 : public Command<int, int, std::string> { };

class H : public FlatHandler<void, Cmd1, Cmd2> {
  int a = 100;
  int b = 100;
  std::string s = "hello!";
  void CommandClause(Cmd1, Resumption<std::tuple<int, int, std::string>, void> r) override
  {
    std::move(r).Resume(a++, b--, s);
  }
  void CommandClause(Cmd2, Resumption<std::tuple<int, int, std::string>, void> r) override
  {
    std::move(r).TailResume(a++, b--, s);
  }
};

void foo()
{
  for (int i = 0; i < 10; i++) {
    {
      auto [a,b,s] = OneShot::InvokeCmd(Cmd1{});
      std::cout << a << " " << b << " " << s << std::endl;
    }
    {
      auto [a,b,s] = OneShot::InvokeCmd(Cmd2{});
      std::cout << a << " " << b << " " << s << std::endl;
    }
  }
}

int main()
{
  OneShot::Handle<H>(foo);
}
