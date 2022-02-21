// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Noresume command clauses

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/adaptors.h"

using namespace CppEffects;

struct Print : Command<void> { };

class Printer : public Handler<int, int, Print> {
public:
  Printer(const std::string& msg) : msg(msg) { }
private:
  std::string msg;
  int CommandClause(Print, std::unique_ptr<Resumption<void, int>> r) override
  {
    std::cout << msg << std::flush;
    return OneShot::Resume(std::move(r));
  }
  int ReturnClause(int a) override { return a + 1; }
};

struct Error : Command<void> { };

class Catch : public Handler<int, int, NoResume<Error>> {
  int CommandClause(Error) override
  {
    std::cout << "[caught]" << std::flush;
    return 42;
  }
  int ReturnClause(int a) override { return a + 100; }
};

void test()
{
  std::cout <<
  OneShot::HandleWith([](){
  int i =
  OneShot::HandleWith([](){
  OneShot::HandleWith([](){
    OneShot::InvokeCmd(Print{});
    OneShot::InvokeCmd(Print{});
    OneShot::InvokeCmd(Error{});
    OneShot::InvokeCmd(Print{});
    OneShot::InvokeCmd(Print{});
    return 100;
  }, std::make_unique<Printer>("[in]"));  OneShot::InvokeCmd(Print{}); return 2;
  }, std::make_unique<Catch>());          OneShot::InvokeCmd(Print{}); return i;
  }, std::make_unique<Printer>("[out]"));
  std::cout << "\t(expected: [in][in][caught][out]43)" << std::endl;
}

int main()
{
  std::cout << "--- noresume-handler ---" << std::endl;
  test();
}
