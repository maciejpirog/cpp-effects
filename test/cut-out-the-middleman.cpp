// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Computations taken out of their original context:
// 
// 1. We use two nested handlers, and at some point remove the outer
//    one.
// 2. We handle a computation, but read the final result elsewhere.

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

// --
// 1.
// --

struct PingOuter : eff::command<> { };

struct PingInner : eff::command<> { };

struct CutMiddlemanAid : eff::command<> { };

struct CutMiddlemanAbet : eff::command<> {
  eff::resumption_data<void, void>* res;
};

class HInner : public eff::handler<void, void, PingInner, CutMiddlemanAid> {
  void handle_command(PingInner, eff::resumption<void()> r) override
  {
    std::cout << "Inner!" << std::endl;
    std::move(r).tail_resume();
  }
  void handle_command(CutMiddlemanAid, eff::resumption<void()> r) override
  {
    eff::invoke_command(CutMiddlemanAbet{{}, r.release()});
  }
  void handle_return() override { }
};

class HOuter : public eff::handler<void, void, PingOuter, CutMiddlemanAbet> {
  void handle_command(PingOuter, eff::resumption<void()> r) override
  {
    std::cout << "Outer!" << std::endl;
    std::move(r).tail_resume();
  }
  void handle_command(CutMiddlemanAbet a, eff::resumption<void()>) override
  {
    eff::resumption<void()>(a.res).tail_resume();
  }

  void handle_return() override { }
};

void test1()
{
    std::cout << "A+" << std::endl;
    eff::handle<HOuter>([](){
      std::cout << "B+" << std::endl;
      eff::handle<HInner>([&](){
        std::cout << "C+" << std::endl;
        eff::invoke_command(PingOuter{});
        eff::invoke_command(PingInner{});
        eff::invoke_command(CutMiddlemanAid{});
        // eff::invoke_command(PingOuter{}); // bad idea!
        eff::invoke_command(PingInner{});
        std::cout << "C-" << std::endl;
      });
      std::cout << "B-" << std::endl;
    });
    std::cout << "A-" << std::endl;
}

// Output:
// A+
// B+
// C+
// Outer!
// Inner!
// Inner!
// C-
// A-

// ----------------------------------------------------------------

// --
// 2.
// --

eff::resumption_data<void, int>* Res = nullptr;

struct Inc : eff::command<> { };

struct Break : eff::command<> { };

void inc()
{
  eff::invoke_command(Inc{});
}
void break_()
{
  eff::invoke_command(Break{});
}
int resume()
{
  return eff::resumption<int()>(Res).resume();
}

class HIP : public eff::handler<int, int, Inc, Break> {
  int handle_command(Break, eff::resumption<int()> r) override
  {
    Res = r.release();
    return 0;
  }
  int handle_command(Inc, eff::resumption<int()> r) override
  {
    return std::move(r).resume() + 1;
  }
  int handle_return(int v) override { return v; }
};

int comp()
{
  inc();
  inc();
  break_();
  inc();
  break_();
  inc();
  return 100;
}

void test2()
{
  std::cout << eff::handle<HIP>(comp) << std::endl;
  std::cout << resume() << std::endl;
  std::cout << resume() << std::endl;
}

// Output:
// 2
// 1
// 101

// ----------------------------------------------------------------

int main()
{
  std::cout << "--- cut-out-the-middleman ---" << std::endl;
  test1();
  std::cout << "***" << std::endl;
  test2();
}
