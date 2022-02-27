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

volatile int MAX = 7000000;

volatile int64_t SUM = 0;

// ----
// Loop
// ----

void testLoop(int max)
{
  for (int i = 0; i < max; i++) {
    SUM += (a * i + b) % 101;
  }
}

// ------
// Native
// ------

__attribute__((noinline))
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

// -------------
// Native inline
// -------------

int fooInline(int x)
{
  return (a * x + b) % 101;
}

void testInline(int max)
{
  for (int i = 0; i < max; i++) {
    SUM += fooInline(i);
  }
}

// ------
// Lambda
// ------

std::function<int(int)> lamFoo = [](int x) -> int {
  return (a * x + b) % 101;
};

void testLambda(int max)
{
  for (int i = 0; i < max; i++) {
    SUM += lamFoo(i);
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

int main(int argc, char**)
{
// hopefully prevents inlining
std::function<int(int)> lamX = [](int x){ return x; };
if (argc == 7) { lamFoo = lamX; }

std::cout << "--- plain handler ---" << std::endl;

std::cout << "loop:           " << std::flush;

auto beginloop = std::chrono::high_resolution_clock::now();
testLoop(MAX);
auto endloop = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(endloop-beginloop).count() << "ns" << std::endl;
std::cout << "native:         " << std::flush;

auto begin = std::chrono::high_resolution_clock::now();
testNative(MAX);
auto end = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << std::endl;

std::cout << "native-inline:  " << std::flush;

auto begini = std::chrono::high_resolution_clock::now();
testInline(MAX);
auto endi = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(endi-begini).count() << "ns" << std::endl;

std::cout << "lambda:         " << std::flush;

auto beginl = std::chrono::high_resolution_clock::now();
testLambda(MAX);
auto endl = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(endl-beginl).count() << "ns" << std::endl;

std::cout << "handlers:       " << std::flush;

auto begin2 = std::chrono::high_resolution_clock::now();
testHandlers(MAX);
auto end2 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end2-begin2).count() << "ns" << std::endl;

std::cout << "plain-handlers: " << std::flush;

auto begin3 = std::chrono::high_resolution_clock::now();
testPlainHandlers(MAX);
auto end3 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end3-begin3).count() << "ns" << std::endl;
}
