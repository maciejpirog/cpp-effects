// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: no_resume command clauses

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

namespace eff = cpp_effects;

struct Print : eff::command<> { };

class Printer : public eff::handler<int, int, Print> {
public:
  Printer(const std::string& msg) : msg(msg) { }
private:
  std::string msg;
  int handle_command(Print, eff::resumption<int()> r) override
  {
    std::cout << msg << std::flush;
    return std::move(r).resume();
  }
  int handle_return(int a) override { return a + 1; }
};

struct Error : eff::command<> { };

class Catch : public eff::handler<int, int, eff::no_resume<Error>> {
  int handle_command(Error) override
  {
    std::cout << "[caught]" << std::flush;
    return 42;
  }
  int handle_return(int a) override { return a + 100; }
};

void test()
{
  std::cout <<
  eff::handle_with([](){
  int i =
  eff::handle_with([](){
  eff::handle_with([](){
    eff::invoke_command(Print{});
    eff::invoke_command(Print{});
    eff::invoke_command(Error{});
    eff::invoke_command(Print{});
    eff::invoke_command(Print{});
    return 100;
  }, std::make_shared<Printer>("[in]"));  eff::invoke_command(Print{}); return 2;
  }, std::make_shared<Catch>());          eff::invoke_command(Print{}); return i;
  }, std::make_shared<Printer>("[out]"));
  std::cout << "\t(expected: [in][in][caught][out]43)" << std::endl;
}

// ---------------------------------
// Example from the reference manual
// ---------------------------------

class Cancel : public eff::handler<void, void, eff::no_resume<Error>> {
  void handle_command(Error) override
  {
    std::cout << "Error!" << std::endl;
  }
  void handle_return() override {}
};

void testError()
{
  std::cout << "Welcome!" << std::endl;
  eff::handle<Cancel>([](){
    std::cout << "So far so good..." << std::endl;
    eff::invoke_command(Error{}); 
    std::cout << "I made it!" << std::endl;
  });
  std::cout << "Bye!" << std::endl;
}


// Output:
// Welcome!
// So far so good...
// Error!
// Bye!

// ----
// Main
// ----

int main()
{
  std::cout << "--- noresume-handler ---" << std::endl;
  test();
  testError();
}
