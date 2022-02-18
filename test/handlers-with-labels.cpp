// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Swapping handlers

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

template <typename T>
struct Read : Command<T> { };

template <typename Answer, typename R>
using ReaderType = Handler<Answer, Answer, Read<R>>;

template <typename R>
class Reader : public Handler<void, void, Read<R>> {
public:
  Reader(R val) : val(val) { }
private:
  const R val;
  void CommandClause(Read<R>, std::unique_ptr<Resumption<R, void>> r) override
  {
    OneShot::TailResume(std::move(r), val);
  }
  void ReturnClause() override { }
};

void rec(int64_t n)
{
  if (n > 0) {
    OneShot::HandleWith(n, std::bind(rec, n - 1), std::make_unique<Reader<int64_t>>(n));
  } else {
    for (int64_t i = 0; i < 300000; i += 767) {
      std::cout << OneShot::InvokeCmd(i % 999 + 1, Read<int64_t>{}) << " ";
    }
  }
}

int main()
{
  rec(1000);
  std::cout << std::endl;
}
