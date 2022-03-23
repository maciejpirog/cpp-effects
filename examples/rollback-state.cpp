// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Track assignments, which can be later rolled back

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

// --------------
// Internal stuff
// --------------

struct AssignBase {
  virtual void undo() = 0;
  virtual ~AssignBase() { }
};

template <typename T>
struct Assign : AssignBase {
  T& var;
  T val;
  Assign(T& x, T v) : var(x), val(x) { x = v; }
  virtual void undo() override { var = val; };
  virtual ~Assign() { }
};

struct AssignCmd : Command<void> {
  AssignBase* asg;
};

struct Rollback : Command<void> { };

// ----------------------
// Programmer's interface
// ----------------------

void rollback()
{
  OneShot::InvokeCmd(Rollback{});
}

class RollbackState : public Handler<bool, void, Rollback, AssignCmd> {
  bool ReturnClause() override
  {
    return true;
  }
  bool CommandClause(Rollback, Resumption<void, bool>) override
  {
    return false;
  }
  bool CommandClause(AssignCmd a, Resumption<void, bool> r) override
  {
    bool result = std::move(r).Resume();
    if (!result) { a.asg->undo(); }
    delete a.asg;
    return result;
  }
};

// track is a syntactic trick that allows the programmer to "annotate"
// an assignment that might be rolled back.

template <typename T>
struct track {
  T& var;
  track(T& var) : var(var) { }
  void operator=(const T& val) { OneShot::InvokeCmd(AssignCmd{{}, new Assign<T>(var, val)}); }
};

// ------------------
// Particular example
// ------------------

int main()
{
  int x = 1;
  char c = 'a';
  OneShot::Handle<RollbackState>([&]() {
    std::cout << "x = " << x << ", " << "c = " << c << std::endl;
    (track<int>) x = 2;
    (track<char>) c = 'b';
    std::cout << "x = " << x << ", " << "c = " << c << std::endl;
    (track<int>) x = 3;
    (track<char>) c = 'c';
    std::cout << "x = " << x << ", " << "c = " << c << std::endl;
    std::cout << "rolling back!" << std::endl;
    rollback();
    (track<int>) x = 4;
    (track<char>) c = 'd';
  });
  std::cout << "x = " << x << ", " << "c = " << c << std::endl;

  // Output:
  // x = 1, c = a
  // x = 2, c = b
  // x = 3, c = c
  // rolling back!
  // x = 1, c = a
}
