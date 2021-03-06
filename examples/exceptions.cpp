// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Exception handlers via effect handlers

#include <functional>
#include <iostream>
#include <vector>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

namespace eff = cpp_effects;

// ------------------------
// Exceptions and a handler
// ------------------------

// There is actually one exception, called Error. We define one
// handler, which handles an exception by returning a value specified
// in the constructor.

struct Error : eff::command<> { };

template <typename T>
[[noreturn]] T error()
{
  eff::invoke_command(Error{});
  exit(-1); // This will never happen!
}

template <typename T>
class WithDefault : public eff::flat_handler<T, Error> {
public:
  WithDefault(const T& t) : defaultVal(t) { }
private:
  const T defaultVal;
  T handle_command(Error, eff::resumption<T()>) override
  {
    return defaultVal;
  }
};

// ------------------
// Particular example
// ------------------

// We multiply numbers in a vector, but if any of the terms is 0, we
// can break the loop using an exception, as we know that the product
// is 0 as well.

int product(const std::vector<int>& v)
{
  return eff::handle<WithDefault<int>>(
    [&v]() {
      int r = 1;
      for (auto i : v) {
        if (i == 0) { error<void>(); }
        r *= i;
      }
      return r;
    },
    0); // the default value is 0
}

int main()
{
  std::cout << product({1, 2, 3, 4, 5}) << std::flush << std::endl;
  std::cout << product({1, 2, 0, 4, 5}) << std::flush << std::endl;

  // Output:
  // 120
  // 0
}
