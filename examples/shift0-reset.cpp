// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: The shift0/reset operators via effect handlers

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

// -----------------------------
// Shift0 and reset via handlers
// -----------------------------

template <typename Answer, typename Hole>
struct Shift0 : Command<Hole> {
  std::function<Answer(std::unique_ptr<Resumption<Hole, Answer>>)> e;
};

template <typename Answer, typename Hole>
class Reset : public FlatHandler<Answer, Shift0<Answer, Hole>> {
  Answer CommandClause(
    Shift0<Answer, Hole> s, std::unique_ptr<Resumption<Hole, Answer>> r) final override
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
  return OneShot::Handle<Reset<Answer, Hole>>(f);
}

template <typename Answer, typename Hole>
Hole shift0(std::function<Answer(std::function<Answer(Hole)>)> e)
{
  return OneShot::InvokeCmd(Shift0<Answer, Hole>{{},
    [=](std::unique_ptr<Resumption<Hole, Answer>> k) ->  Answer {
      return e([k = k.release()](Hole out) -> Answer {
        return OneShot::Resume(std::unique_ptr<Resumption<Hole, Answer>>(k), out);
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
      return "2 + 2 = " + std::to_string(2 + shift0<std::string, int>([](auto k){
        return "It is not true that " + k(3);
      }));
    }) << std::endl;;
}
