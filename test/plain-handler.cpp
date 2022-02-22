// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Plain command clauses

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

using namespace CppEffects;

// -----------------------------------
// Commands and programmer's interface
// -----------------------------------

template<typename S>
struct Put : Command<void> {
  S newState;
};

template<typename S>
struct Get : Command<S> { };

template<typename S>
void put(S s) {
  OneShot::InvokeCmd(Put<S>{{}, s});
}

template<typename S>
S get() {
  return OneShot::InvokeCmd(Get<S>{});
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
class HStateful : public Handler<Answer, Answer, Plain<Put<S>>, Plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void CommandClause(Put<S> p) override
  {
    state = p.newState;
  }
  S CommandClause(Get<S>) override
  {
    return state;
  }
  Answer ReturnClause(Answer a) override { return a; }
};

// Specialisation for Answer = void

template<typename S>
class HStateful<void, S> : public Handler<void, void, Plain<Put<S>>, Plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void CommandClause(Put<S> p) override
  {
    state = p.newState;
  }
  S CommandClause(Get<S>) override
  {
    return state;
  }
  void ReturnClause() override { }
};

void testStateful()
{
  OneShot::HandleWith(test, std::make_unique<HStateful<void, int>>(100));
  std::cout << OneShot::HandleWith(test2, std::make_unique<HStateful<std::string, int>>(100));
  std::cout << std::endl;

  // Output:
  // 100 101 10201
  // 100 101 10201
  // ok
}

// -------------------------------------------------------

struct Do : Command<void> { };

class Bracket : public Handler<void, void, Do> {
public:
  Bracket(const std::string& msg) : msg(msg) { }
private:
  std::string msg;
  void CommandClause(Do, std::unique_ptr<Resumption<void, void>> r) override
  {
    std::string lmsg = this->msg;
    std::cout << lmsg << "+" << std::flush;
    OneShot::Resume(std::move(r));
    std::cout << lmsg << "-" << std::flush;
  }
  void ReturnClause() override { }
};

struct Print : Command<void> { };

class Printer : public Handler<void, void, Plain<Print>> {
public:
  Printer(const std::string& msg) : msg(msg) { }
private:
  std::string msg;
  void CommandClause(Print) override
  {
    std::string lmsg = this->msg;
    std::cout << lmsg << "+" << std::flush;
    OneShot::InvokeCmd(Do{});
    std::cout << lmsg << "-" << std::flush;
  }
  void ReturnClause() override { }
};

void sandwichComp()
{
  OneShot::InvokeCmd(Do{});
  OneShot::InvokeCmd(Print{});
}

void testSandwich()
{
  OneShot::HandleWith([](){
  OneShot::HandleWith([](){
  OneShot::HandleWith([](){
    sandwichComp();
  }, std::make_unique<Bracket>("[in]"));
  }, std::make_unique<Printer>("[print]"));
  }, std::make_unique<Bracket>("[out]"));
  std::cout << std::endl;
}

int main()
{
  std::cout << "--- plain-handler ---" << std::endl;
  testStateful();
  testSandwich();
}
