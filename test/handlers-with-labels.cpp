// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Swapping handlers

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

template <typename T>
struct Read : eff::command<T> { };

template <typename Answer, typename R>
using ReaderType = eff::handler<Answer, Answer, Read<R>>;

template <typename R>
class Reader : public eff::handler<void, void, Read<R>> {
public:
  Reader(R val) : val(val) { }
private:
  const R val;
  void handle_command(Read<R>, eff::resumption<void(R)> r) override
  {
    std::move(r).tail_resume(val);
  }
  void handle_return() override { }
};

void rec(int64_t n)
{
  if (n > 0) {
    eff::handle<Reader<int64_t>>(n, std::bind(rec, n - 1), n);
  } else {
    for (int64_t i = 0; i < 300000; i += 767) {
      std::cout << eff::invoke_command(i % 999 + 1, Read<int64_t>{}) << " ";
    }
  }
}

int main()
{
  std::cout << "--- handlers-with-labels ---" << std::endl;
  rec(1000);
  std::cout << std::endl;
}
