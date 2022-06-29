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

__attribute__((noinline))
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

__attribute__((noinline))
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

__attribute__((noinline))
void testLambda(int max)
{
  for (int i = 0; i < max; i++) {
    SUM += lamFoo(i);
  }
}

// ----------------------
// dynamic_cast + virtual
// ----------------------

class Base {
public:
  virtual int foo(int x) { return x; }
};

class Base2 {
public:
  virtual int goo(int x) { return x; }
};

Base2* dCastPtr = nullptr;

class Derived : public Base, public Base2 {
public:
  int foo(int x) override
  {
    return (a * x + b) % 101;
  }
};

__attribute__((noinline))
void testDCast(int max)
{
  for (int i = 0; i < max; i++) {
    SUM += dynamic_cast<Base*>(dCastPtr)->foo(i);
  }
}

// --------
// Handlers
// --------

struct Foo : Command<int> { int x; };

class Han : public Handler<void, void, Foo> {
  void ReturnClause () override { }
  void CommandClause(Foo c, Resumption<void(int)> r) override
  {
    std::move(r).TailResume((a * c.x + b) % 101);
  }
};

__attribute__((noinline))
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

__attribute__((noinline))
void testPlainHandlers(int max)
{
  OneShot::Handle<PHan>([=](){
    for (int i = 0; i < max; i++) {
      SUM += OneShot::InvokeCmd(Foo{{}, i});
    }
  });
}


// ---------------
// Static handlers
// ---------------

__attribute__((noinline))
void testStaticHandlers(int max)
{
  OneShot::Handle<Han>([=](){
    for (int i = 0; i < max; i++) {
      SUM += OneShot::StaticInvokeCmd<Han>(Foo{{}, i});
    }
  });
}

// ---------------------
// Static plain handlers
// ---------------------

__attribute__((noinline))
void testStaticPlainHandlers(int max)
{
  OneShot::Handle<PHan>([=](){
    for (int i = 0; i < max; i++) {
      SUM += OneShot::StaticInvokeCmd<PHan>(Foo{{}, i});
    }
  });
}

// --------------------
// Known plain handlers
// --------------------

__attribute__((noinline))
void testKnownPlainHandlers(int max)
{
  OneShot::Handle<PHan>(10, [=](){
    for (int i = 0; i < max; i++) {
      auto it = OneShot::FindHandler(10);
      SUM += OneShot::StaticInvokeCmd<PHan>(it, Foo{{}, i});
    }
  });
}

// ----
// Main
// ----

int main(int argc, char**)
{

std::cout << "--- plain handler ---" << std::endl;

std::cout << "loop:             " << std::flush;

auto beginloop = std::chrono::high_resolution_clock::now();
testLoop(MAX);
auto endloop = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(endloop-beginloop).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(endloop-beginloop).count() / MAX) << "ns per iteration)" <<std::endl;

std::cout << "native:           " << std::flush;

auto begin = std::chrono::high_resolution_clock::now();
testNative(MAX);
auto end = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / MAX) << "ns per iteration)" <<std::endl;

std::cout << "native-inline:    " << std::flush;

auto begini = std::chrono::high_resolution_clock::now();
testInline(MAX);
auto endi = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(endi-begini).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(endi-begini).count() / MAX) << "ns per iteration)" <<std::endl;

std::cout << "lambda:           " << std::flush;

// hopefully prevents inlining
std::function<int(int)> lamX = [](int x){ return x; };
if (argc == 7) { lamFoo = lamX; }

auto beginl = std::chrono::high_resolution_clock::now();
testLambda(MAX);
auto endl = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(endl-beginl).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(endl-beginl).count() / MAX) << "ns per iteration)" << std::endl;

std::cout << "dynamic_cast:     " << std::flush;

if (argc % 123 == 7) {
  dCastPtr = new Base2();
} else {
  dCastPtr = new Derived();
}

auto begindc = std::chrono::high_resolution_clock::now();
testDCast(MAX);
auto enddc = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(enddc-begindc).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(enddc-begindc).count() / MAX) << "ns per iteration)" << std::endl;

std::cout << "handlers:         " << std::flush;

auto begin2 = std::chrono::high_resolution_clock::now();
testHandlers(MAX);
auto end2 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end2-begin2).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end2-begin2).count() / MAX) << "ns per iteration)" <<std::endl;

std::cout << "plain-handlers:   " << std::flush;

auto begin3 = std::chrono::high_resolution_clock::now();
testPlainHandlers(MAX);
auto end3 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end3-begin3).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end3-begin3).count() / MAX) << "ns per iteration)" <<std::endl;

std::cout << "s-handlers:       " << std::flush;

auto begins2 = std::chrono::high_resolution_clock::now();
testStaticHandlers(MAX);
auto ends2 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(ends2-begins2).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(ends2-begins2).count() / MAX) << "ns per iteration)" << std::endl;

std::cout << "s-plain-handlers: " << std::flush;

auto begins3 = std::chrono::high_resolution_clock::now();
testStaticPlainHandlers(MAX);
auto ends3 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(ends3-begins3).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(ends3-begins3).count() / MAX) << "ns per iteration)" << std::endl;

std::cout << "known-handlers:   " << std::flush;

auto beginks3 = std::chrono::high_resolution_clock::now();
testKnownPlainHandlers(MAX);
auto endks3 = std::chrono::high_resolution_clock::now();
std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(endks3-beginks3).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(endks3-beginks3).count() / MAX) << "ns per iteration)" << std::endl;
}
