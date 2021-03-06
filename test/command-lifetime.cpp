// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: We print out the lifecycle of a command to see that we have
// to copy it inside a command clause.

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

class Cmd : public eff::command<> {
public:
  int id;
  Cmd() : id(rand() % 1000) { std::cout << id << " born" << std::endl; }
  ~Cmd() { std::cout << id << " dead"  << std::endl; }
  Cmd(const Cmd& cmd)
  {
    id = rand() % 1000;
    std::cout << id << " born (copied from " << cmd.id << ")" << std::endl;
  }
  Cmd(Cmd&& cmd)
  {
    id = rand() % 1000;
    std::cout << id << "born (moved from " << cmd.id << ")" << std::endl;
  }
  Cmd& operator=(const Cmd&) = default;
  Cmd& operator=(Cmd&&) = default;
};

class MyHandler : public eff::handler<void, void, Cmd> {
  void handle_return() override { }
  void handle_command(Cmd c, eff::resumption<void()> r) override
  {
    std::cout << "In handler: received command id = " << c.id << std::endl;
    std::move(r).resume();
    std::cout << "In handler: still using command id = " << c.id << std::endl;
  }
};

void myComputation()
{
  std::cout << "In computation: Creating a command..." << std::endl;
  {
    Cmd cmd;
    eff::invoke_command(cmd);
  }
  std::cout << "In computation: The command is gone..." << std::endl;
}

int main()
{
  std::cout << "--- command-lifetime ---" << std::endl;
  eff::handle<MyHandler>(myComputation);
}

// Output:
// In computation: Creating a command...
// 807 born
// 249 born (copied from 807)
// In handler: received command id = 249
// 807 dead
// In computation: The command is gone...
// In handler: still using command id = 249
// 249 dead

// Note how catastrophic pass-by-reference would be in this example!
