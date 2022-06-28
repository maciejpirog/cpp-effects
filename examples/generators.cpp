// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Generators

#include <iostream>
#include <optional>
#include <string>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

using namespace CppEffects;

// --------------
// Internal stuff
// --------------

template <typename T>
class GeneratorHandler;

template <typename T>
struct Yield : Command<> {
  T value;
};

template <typename T>
void yield(int64_t label, T x)
{
  OneShot::StaticInvokeCmd<GeneratorHandler<T>>(label, Yield<T>{{}, x});
}

template <typename T>
struct GenState;

template <typename T>
using Result = std::optional<GenState<T>>;

template <typename T>
struct GenState {
  T value;
  Resumption<void, Result<T>> resumption;
};

template <typename T>
class GeneratorHandler : public Handler<Result<T>, void, NoManage<Yield<T>>> {
  Result<T> CommandClause(Yield<T> y, Resumption<void, Result<T>> r) override
  {
    return GenState<T>{y.value, std::move(r)};
  }
  Result<T> ReturnClause() override
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
    auto label = OneShot::FreshLabel();
    result = OneShot::Handle<GeneratorHandler<T>>(label, [f, label](){
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
    result = std::move(result->resumption).Resume();
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
