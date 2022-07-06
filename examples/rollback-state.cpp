// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Track assignments, which can be later rolled back

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

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

struct AssignCmd : eff::command<> {
  AssignBase* asg;
};

struct Rollback : eff::command<> { };

// ----------------------
// Programmer's interface
// ----------------------

void rollback()
{
  eff::invoke_command(Rollback{});
}

class RollbackState : public eff::handler<bool, void, Rollback, AssignCmd> {
  bool handle_return() override
  {
    return true;
  }
  bool handle_command(Rollback, eff::resumption<bool()>) override
  {
    return false;
  }
  bool handle_command(AssignCmd a, eff::resumption<bool()> r) override
  {
    bool result = std::move(r).resume();
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
  void operator=(const T& val) { eff::invoke_command(AssignCmd{{}, new Assign<T>(var, val)}); }
};

// ------------------
// Particular example
// ------------------

int main()
{
  int x = 1;
  char c = 'a';
  eff::handle<RollbackState>([&]() {
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
