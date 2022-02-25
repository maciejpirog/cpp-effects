// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Benchmark: Compare function call with invoking a command that behaves like a function

#include <chrono>
#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

using namespace CppEffects;

volatile int a = 19;
volatile int b = 585;

volatile int MAX = 1000000;

volatile int64_t SUM = 0;

// ------
// Native
// ------

int foo(int x)
{
  return (a * x + b) % 101;
}

void testNative(int max)
{
  for (int i = 0; i < max; i++) {
    SUM += foo(i);
  }
}

// --------
// Handlers
// --------

struct Foo : Command<int> { int x; };

class Han : public Handler<void, void, Foo> {
  void ReturnClause () override { }
  void CommandClause(Foo c, std::unique_ptr<Resumption<int, void>> r) override
  {
    OneShot::TailResume(std::move(r), (a * c.x + b) % 101);
  }
};

void testHandlers(int max)
{
  OneShot::Handle<Han>([=](){
    for (int i = 0; i < max; i++) {
      SUM += OneShot::InvokeCmd(Foo{{}, i});
    }
  });
}

// --------------
// Plain handlers
// --------------

class PHan : public Handler<void, void, Plain<Foo>> {
  void ReturnClause () override { }
  int CommandClause(Foo c) override
  {
    return (a * c.x + b) % 101;
  }
};

void testPlainHandlers(int max)
{
  OneShot::Handle<PHan>([=](){
    for (int i = 0; i < max; i++) {
      SUM += OneShot::InvokeCmd(Foo{{}, i});
    }
  });
}

// ----
// Main
// ----

int main()
{
std::cout << "--- plain handler ---" << std::endl;

std::cout << "native:         " << std::flush;

auto begin = std::chrono::high_resolution_clock::now();
testNative(MAX);
auto end = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << std::endl;

std::cout << "handlers:       " << std::flush;

auto begin2 = std::chrono::high_resolution_clock::now();
testHandlers(MAX);
auto end2 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end2-begin2).count() << "ns" << std::endl;

std::cout << "plain handlers: " << std::flush;

auto begin3 = std::chrono::high_resolution_clock::now();
testPlainHandlers(MAX);
auto end3 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end3-begin3).count() << "ns" << std::endl;
}
