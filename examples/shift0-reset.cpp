// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: The shift0/reset operators via effect handlers

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

// -----------------------------
// Shift0 and reset via handlers
// -----------------------------

template <typename Answer, typename Hole>
struct Shift0 : eff::command<Hole> {
  std::function<Answer(eff::resumption<Answer(Hole)>)> e;
};

template <typename Answer, typename Hole>
class Reset : public eff::flat_handler<Answer, Shift0<Answer, Hole>> {
  Answer handle_command(
    Shift0<Answer, Hole> s, eff::resumption<Answer(Hole)> r) final override
  {
    return s.e(std::move(r));
  }
};

// --------------------
// Functional interface
// --------------------

template <typename Answer, typename Hole>
Answer reset(std::function<Answer()> f)
{
  return eff::handle<Reset<Answer, Hole>>(f);
}

template <typename Answer, typename Hole>
Hole shift0(std::function<Answer(std::function<Answer(Hole)>)> e)
{
  return eff::invoke_command(Shift0<Answer, Hole>{{},
    [=](eff::resumption<Answer(Hole)> k) -> Answer {
      return e([k = k.release()](Hole out) -> Answer {
        return eff::resumption<Answer(Hole)>(k).resume(out);
      });
    }
  });
}

// ------------------
// Particular example
// ------------------

int main()
{
  std::cout <<
    reset<std::string, int>([]() {
      return "2 + 2 = " + std::to_string(
        2 + shift0<std::string, int>([](auto k) { return "It is not true that " + k(3); }));
    }) << std::endl;;
}
