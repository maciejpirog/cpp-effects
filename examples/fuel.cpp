// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Bound execution of a computation

#include <functional>
#include <iostream>
#include <optional>
#include <tuple>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

// -----------------
// Bounded execution
// -----------------

// By invoking this operation, a computation "consumes" a given amount
// of fuel

struct Consume : Command<void> {
  int64_t amount;
};

// Allow a computation to use only a certain amount of fuel

template <typename T>
class BoundedExecution : public Handler<std::optional<T>, T, Consume> {
public:
  BoundedExecution(int64_t fuel) : fuel(fuel) { }
private:
  int64_t fuel;
  std::optional<T> CommandClause(Consume c, Resumption<void, std::optional<T>> r) override
  {
    if (fuel < c.amount) { return {}; } // Not enough fuel left to continue
    fuel -= c.amount;
    return OneShot::TailResume(std::move(r));
  }
  std::optional<T> ReturnClause(T val) override
  {
    return std::optional(val);
  }
};

// Run a computation to completion, but record the amount of fuel it used

template <typename T>
class MeasureFuel : public Handler<std::tuple<T, int64_t>, T, Consume> {
private:
  int64_t fuel = 0;
  std::tuple<T, int64_t> CommandClause(Consume c,
    Resumption<void, std::tuple<T, int64_t>> r) override
  {
    fuel += c.amount;
    return OneShot::TailResume(std::move(r));
  }
  std::tuple<T, int64_t> ReturnClause(T val) override
  {
    return {val, fuel};
  }
};

// ------------------
// Particular example
// ------------------

int64_t fib(int64_t n)
{
  OneShot::InvokeCmd(Consume{{}, 1}); // consume 1 unit of fuel
  switch (n) {
    case 0: return 0;
    case 1: return 1;
    default: return fib(n-1) + fib(n-2);
  }
}

void tryFib(int64_t n)
{
  std::cout << "fib(" << n << ") = ";
  std::optional res = OneShot::Handle<BoundedExecution<int64_t>>(
    std::bind(fib, n),
    10000); // supply 10000 units of fuel
  
  if (res) {
    std::cout << res.value() << std::endl;
  } else {
    std::cout << "...took too long to complete" << std::endl;
  }
}

void measureFib(int64_t n)
{
  std::tuple<int64_t, int64_t> res = OneShot::Handle<MeasureFuel<int64_t>>(std::bind(fib, n));

  std::cout << "fib(" << n << ") = " << std::get<0>(res)
            << "\t(took " << std::get<1>(res) << " steps to complete)" << std::endl;
}

int main()
{
  tryFib(5);
  tryFib(10);
  tryFib(15);
  tryFib(20);

  // Output:
  // fib(5) = 5
  // fib(10) = 55
  // fib(15) = 610
  // fib(20) = ...took too long to complete

  measureFib(5);
  measureFib(10);
  measureFib(15);
  measureFib(20);

  // Output:
  // fib(5) = 5       (took 15 steps to complete)
  // fib(10) = 55     (took 177 steps to complete)
  // fib(15) = 610    (took 1973 steps to complete)
  // fib(20) = 6765   (took 21891 steps to complete)
}
