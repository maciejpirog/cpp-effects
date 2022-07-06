// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Generators

#include <iostream>
#include <optional>
#include <string>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

namespace eff = cpp_effects;

// --------------
// Internal stuff
// --------------

template <typename T>
class GeneratorHandler;

template <typename T>
struct Yield : eff::command<> {
  T value;
};

template <typename T>
void yield(int64_t label, T x)
{
  eff::static_invoke_command<GeneratorHandler<T>>(label, Yield<T>{{}, x});
}

template <typename T>
struct GenState;

template <typename T>
using Result = std::optional<GenState<T>>;

template <typename T>
struct GenState {
  T value;
  eff::resumption<Result<T>()> resumption;
};

template <typename T>
class GeneratorHandler : public eff::handler<Result<T>, void, eff::no_manage<Yield<T>>> {
  Result<T> handle_command(Yield<T> y, eff::resumption<Result<T>()> r) override
  {
    return GenState<T>{y.value, std::move(r)};
  }
  Result<T> handle_return() override
  {
    return {};
  }
};

// ----------------------
// Programmer's interface
// ----------------------

// When a generator is created, its body is executed until the first
// Yield. It is only because in this example we want the programmer's
// interface to be as simple as possible, so that the "there is a
// value" and "you can resume me" states of a generator always hve the
// same value, available via operator bool.

template <typename T>
class Generator {
public:
  Generator(std::function<void(std::function<void(T)>)> f)
  {
    auto label = eff::fresh_label();
    result = eff::handle<GeneratorHandler<T>>(label, [f, label](){
      f([label](T x) { yield<T>(label, x); });
    });
  }
  Generator() // Create a dummy generator that generates nothing
  {
  }
  T Value() const
  {
    if (!result) { throw std::out_of_range("Generator::Value"); }
    return result.value().value;
  }
  bool Next()
  {
    if (!result) { throw std::out_of_range("Generator::Next"); }
    result = std::move(result->resumption).resume();
    return result.has_value();
  }
  explicit operator bool() const
  {
    return result.has_value();
  }
private:
  Result<T> result = {};
};

// ------------------
// Particular example
// ------------------

int main()
{
  Generator<int> naturals([](auto yield) {
    int i = 1;
    while (true) { yield(i++); }
  });

  Generator<std::string> peaks([](auto yield) {
    yield("Everest");
    yield("K2");
    yield("Kangchenjunga");
    yield("Lhotse");
    yield("Makalu");
  });

  while ((bool)peaks) {
    std::cout << naturals.Value() << " " << peaks.Value() << std::endl;
    naturals.Next();
    peaks.Next();
  }

  // Output:
  // 1 Everest
  // 2 K2
  // 3 Kangchenjunga
  // 4 Lhotse
  // 5 Makalu
}
