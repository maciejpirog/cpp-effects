// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: We print out the lifecycle of a handler.

// There are two places where handler can die:
// 
// - After the return clause, where the metaframe no longer kept on
//   the metastack (Metastack().pop_back() in
//   OneShot::HandleWith(int64_t, ...)
//
// - After a resumption that holds a pointer (in storedMetastack) dies.
//
// But there's no telling which happens first, so handlers are managed
// via shared_ptrs (unless the user does something evil with the
// pointers outside the effect handler library, no cycles appear).

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

// -------------------------------------------------------------------
// In the first example, the handler object needs to survive after the
// return clause
// -------------------------------------------------------------------

struct Cmd : public Command<void> { };

class MyHandler : public Handler<void, void, Cmd> {
public:
  ~MyHandler() { msg = "dead"; std::cout << "The handler is dead!" << std::endl; }
private:
  std::string msg = "alive";
  void PrintStatus() { std::cout << "I'm " << this->msg << "!" << std::endl; }
  void ReturnClause() override { }
  void CommandClause(Cmd, Resumption<void, void> r) override
  {
    this->PrintStatus();
    OneShot::Resume(std::move(r));
    this->PrintStatus(); // Do I still exist?
  }
};

void afterReturn()
{
  std::cout << "Good morning!" << std::endl;
  OneShot::InvokeCmd(Cmd{});
  std::cout << "How are you?" << std::endl;
  OneShot::InvokeCmd(Cmd{});
  std::cout << "Cheers!" << std::endl;
}

// -----------------------------------------------------------
// In the second example, the HandleWith context dies, but the
// resumption lives on
// -----------------------------------------------------------

Resumption<void, void> res;

class EscapeHandler :  public Handler<void, void, Cmd> {
public:
  ~EscapeHandler() { std::cout << "The handler is dead!" << std::endl; }
private:
  void CommandClause(Cmd, Resumption<void, void> r)
  {
    res = std::move(r);
    std::cout << "Must give us pause!" << std::endl;
  }
  void ReturnClause() { std::cout << "Thanks!" << std::endl; }
};

void resumptionEscape()
{
  OneShot::Handle<EscapeHandler>([](){
    std::cout << "To be" << std::endl;
    OneShot::InvokeCmd(Cmd{});
    std::cout << "Or not to be" << std::endl;
  });

  std::cout << "[A short break]" << std::endl;

  OneShot::Resume(std::move(res));
}

// ----
// Main
// ----

int main()
{
  std::cout << "--- handler-lifetime ---" << std::endl;
  std::cout << "** handler exists after return clause **" << std::endl;
  OneShot::Handle<MyHandler>(afterReturn);
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
