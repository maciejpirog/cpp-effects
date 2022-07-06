// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Plain command clauses

#include <functional>
#include <iostream>
#include <variant>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

namespace eff = cpp_effects;

// -----------------------------------
// Commands and programmer's interface
// -----------------------------------

template<typename S>
struct Put : eff::command<> {
  S newState;
};

template<typename S>
struct Get : eff::command<S> { };

template<typename S>
void put(S s) {
  eff::invoke_command(Put<S>{{}, s});
}

template<typename S>
S get() {
  return eff::invoke_command(Get<S>{});
}

// ----------------------
// Particular computation
// ----------------------

void test()
{
    std::cout << get<int>() << " ";
    put(get<int>() + 1);
    std::cout << get<int>() << " ";
    put(get<int>() * get<int>());
    std::cout << get<int>() << std::endl;
}

std::string test2()
{
    std::cout << get<int>() << " ";
    put(get<int>() + 1);
    std::cout << get<int>() << " ";
    put(get<int>() * get<int>());
    std::cout << get<int>() << std::endl;
    return "ok";
}

// -------------------
// 1. Stateful handler
// -------------------

template<typename Answer, typename S>
class HStateful : public eff::handler<Answer, Answer, eff::plain<Put<S>>, eff::plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void handle_command(Put<S> p) override
  {
    state = p.newState;
  }
  S handle_command(Get<S>) override
  {
    return state;
  }
  Answer handle_return(Answer a) override { return a; }
};

// Specialisation for Answer = void

template<typename S>
class HStateful<void, S> : public eff::handler<void, void, eff::plain<Put<S>>, eff::plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void handle_command(Put<S> p) override
  {
    state = p.newState;
  }
  S handle_command(Get<S>) override
  {
    return state;
  }
  void handle_return() override { }
};

void testStateful()
{
  eff::handle<HStateful<void, int>>(test, 100);
  std::cout << eff::handle<HStateful<std::string, int>>(test2, 100);
  std::cout << std::endl;

  // Output:
  // 100 101 10201
  // 100 101 10201
  // ok
}

// -------------------------------------------------------

struct Do : eff::command<> { };

class Bracket : public eff::handler<void, void, Do> {
public:
  Bracket(const std::string& msg) : msg(msg) { }
private:
  std::string msg;
  void handle_command(Do, eff::resumption<void()> r) override
  {
    std::string lmsg = this->msg;
    std::cout << lmsg << "+" << std::flush;
    std::move(r).resume();
    std::cout << lmsg << "-" << std::flush;
  }
  void handle_return() override { }
};

struct Print : eff::command<> { };

class Printer : public eff::handler<void, void, eff::plain<Print>> {
public:
  Printer(const std::string& msg) : msg(msg) { }
private:
  std::string msg;
  void handle_command(Print) override
  {
    std::string lmsg = this->msg;
    std::cout << lmsg << "+" << std::flush;
    eff::invoke_command(Do{});
    std::cout << lmsg << "-" << std::flush;
  }
  void handle_return() override { }
};

void sandwichComp()
{
  eff::invoke_command(Do{});
  eff::invoke_command(Print{});
}

void testSandwich()
{
  eff::handle_with([](){
  eff::handle_with([](){
  eff::handle_with([](){
    sandwichComp();
  }, std::make_shared<Bracket>("[in]"));
  }, std::make_shared<Printer>("[print]"));
  }, std::make_shared<Bracket>("[out]"));
  std::cout << std::endl;
}

// ---------------------------------
// Example from the reference manual
// ---------------------------------

struct Add : eff::command<int> {
  int x, y;
};

template <typename T>
class Calculator : public eff::handler<T, T, eff::plain<Add>> {
  int handle_command(Add c) override {
    return c.x + c.y;
  }
  T handle_return(T s) override {
    return s;
  }
};

void testCalc()
{
  eff::handle<Calculator<std::monostate>>([]() -> std::monostate {
    std::cout << "2 + 5 = " << eff::invoke_command<Add>({{}, 2, 5}) << std::endl;
    std::cout << "11 + 3 = " << eff::invoke_command<Add>({{}, 11, 3}) << std::endl;
    return {};
  });
}

// -----------------
// The main function
// -----------------

int main()
{
  std::cout << "--- plain-handler ---" << std::endl;
  testStateful();
  testSandwich();
  testCalc();
}
