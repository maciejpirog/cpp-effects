// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: We print out the lifecycle of a command to see that we have
// to copy it inside a command clause.

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

class Cmd : public Command<void> {
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

class MyHandler : public Handler<void, void, Cmd> {
  void ReturnClause() override { }
  void CommandClause(Cmd c, std::unique_ptr<Resumption<void, void>> r) override
  {
    std::cout << "In handler: received command id = " << c.id << std::endl;
    OneShot::Resume(std::move(r));
    std::cout << "In handler: still using command id = " << c.id << std::endl;
  }
};

void myComputation()
{
  std::cout << "In computation: Creating a command..." << std::endl;
  {
    Cmd cmd;
    OneShot::InvokeCmd(cmd);
  }
  std::cout << "In computation: The command is gone..." << std::endl;
}

int main()
{
  OneShot::Handle<MyHandler>(myComputation);
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

// Note how catastrofic pass-by-reference would be in this example!
