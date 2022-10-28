// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: We control the output stream by using handlers

#include <functional>
#include <iostream>
#include <optional>
#include <tuple>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

// -----------------
// Log
// -----------------

// By invoking this operation, the handler logs data in different streams

struct Log : eff::command<> {};

// Use stdout for logging

template <typename T>
class StdoutLog : public eff::handler<std::optional<T>, T, Log> {
private:
  std::ostream &output = std::cout;
  std::optional<T>
  handle_command(Log, eff::resumption<std::optional<T>()> r) override {
    return std::move(r).resume();
  }
  std::optional<T> handle_return(T val) override {
    output << val << std::endl;
    return std::optional(val);
  }
};

// Use stderr for logging

template <typename T>
class StderrLog : public eff::handler<std::optional<T>, T, Log> {
private:
  std::ostream &output = std::cerr;
  std::optional<T>
  handle_command(Log, eff::resumption<std::optional<T>()> r) override {
    return std::move(r).resume();
  }
  std::optional<T> handle_return(T val) override {
    output << val << std::endl;
    return std::optional(val);
  }
};

// ------------------
// Particular example
// ------------------

int64_t fib(int64_t n) {
  eff::invoke_command(Log{});
  switch (n) {
  case 0:
    return 0;
  case 1:
    return 1;
  default:
    return fib(n - 1) + fib(n - 2);
  }
}

void stdoutFib(int64_t n) {
  // Log the result of fib(n) using stdout
  eff::handle<StdoutLog<int64_t>>(std::bind(fib, n));
}

void stderrFib(int64_t n) {
  // Log the result of fib(n) using stderr
  eff::handle<StderrLog<int64_t>>(std::bind(fib, n));
}

int main() {
  stdoutFib(5);
  stderrFib(10);
  stdoutFib(15);
  stderrFib(20);

  // Output:
  // stdout -> fib(5) = 5
  // stderr -> fib(10) = 55
  // stdout -> fib(15) = 610
  // stderr -> fib(20) = 6765
}
