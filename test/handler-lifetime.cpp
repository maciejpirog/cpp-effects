// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: We print out the lifecycle of a handler.

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

// -------------------------------------------------------------------
// In the first example, the handler object needs to survive after the
// return clause
// -------------------------------------------------------------------

struct Cmd : public eff::command<> { };

class MyHandler : public eff::handler<void, void, Cmd> {
public:
  ~MyHandler() { msg = "dead"; std::cout << "The handler is dead!" << std::endl; }
private:
  std::string msg = "alive";
  void PrintStatus() { std::cout << "I'm " << this->msg << "!" << std::endl; }
  void handle_return() override { }
  void handle_command(Cmd, eff::resumption<void()> r) override
  {
    this->PrintStatus();
    std::move(r).resume();
    this->PrintStatus(); // Do I still exist?
  }
};

void afterReturn()
{
  std::cout << "Good morning!" << std::endl;
  eff::invoke_command(Cmd{});
  std::cout << "How are you?" << std::endl;
  eff::invoke_command(Cmd{});
  std::cout << "Cheers!" << std::endl;
}

// -----------------------------------------------------------
// In the second example, the HandleWith context dies, but the
// resumption lives on
// -----------------------------------------------------------

struct OtherCmd : eff::command<> { };

eff::resumption<void()> res;

class EscapeHandler :  public eff::handler<void, void, Cmd, OtherCmd> {
public:
  ~EscapeHandler() { std::cout << "The handler is dead!" << std::endl; }
private:
  void handle_command(Cmd, eff::resumption<void()> r)
  {
    res = std::move(r);
    std::cout << "Must give us pause!" << std::endl;
  }
  void handle_command(OtherCmd, eff::resumption<void()> r)
  {
    std::cout << "[[This is other cmd]]" << std::endl;
    std::move(r).tail_resume();
    //std::cout << "[[This is other cmd]]" << std::endl;
  }
  void handle_return() { std::cout << "Thanks!" << std::endl; }
};

void resumptionEscape()
{
  eff::handle<EscapeHandler>([](){
    std::cout << "To be" << std::endl;
    eff::invoke_command(Cmd{});
    std::cout << "Or not to be" << std::endl;
    eff::invoke_command(OtherCmd{});
    eff::invoke_command(OtherCmd{});
    eff::invoke_command(OtherCmd{});
    std::cout << "??" << std::endl;
  });

  std::cout << "[A short break]" << std::endl;

  std::move(res).resume();
}

// ----
// Main
// ----

int main()
{
  std::cout << "--- handler-lifetime ---" << std::endl;
  std::cout << "** handler exists after return clause **" << std::endl;
  eff::handle<MyHandler>(afterReturn);
  std::cout << "  (expected:" << std::endl
    << "  Good morning!" << std::endl
    << "  I'm alive!" << std::endl
    << "  How are you?" << std::endl
    << "  I'm alive!" << std::endl
    << "  Cheers!" << std::endl
    << "  I'm alive!" << std::endl
    << "  I'm alive!" << std::endl
    << "  The handler is dead!)" << std::endl;

  std::cout << "** handler exists after HandleWith returned **" << std::endl;
  resumptionEscape();
  std::cout << "  (expected:" << std::endl
    << "  To be" << std::endl
    << "  Must give us pause!" << std::endl
    << "  [A short break]" << std::endl
    << "  Or not to be" << std::endl
    << "  Thanks!" << std::endl
    << "  The handler is dead!)" << std::endl;
}
