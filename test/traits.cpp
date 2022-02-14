// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: copy/assignment capabilities of types that can be used with the library

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

class XTCC { // exclude trivial constructor and copy
public:
  XTCC(int v) : val(v) { }
  XTCC(XTCC&&) = default;

  XTCC() = delete;
  XTCC(const XTCC&) = delete;
  XTCC& operator=(XTCC&&) = delete;
  XTCC& operator=(const XTCC&) = delete;

  int val;
};

struct Cmd : Command<int> { };

// ------------------------------------------------------------------

class TestAnswerType : public Handler<XTCC, int, Cmd> {
  XTCC CommandClause(Cmd, std::unique_ptr<Resumption<int, XTCC>> r) override {
    return XTCC(OneShot::Resume(std::move(r), 100).val + 1);
  }
  XTCC ReturnClause(int v) override {
    return XTCC(v);
  }
};

void testAnswerType()
{
  std::cout << OneShot::HandleWith(
      [](){ return OneShot::InvokeCmd(Cmd{}) + 10; },
      std::make_unique<TestAnswerType>()).val
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class TestAnswerTypeVoid : public Handler<XTCC, void, Cmd> {
  XTCC CommandClause(Cmd, std::unique_ptr<Resumption<int, XTCC>> r) override {
    return XTCC(OneShot::Resume(std::move(r), 100).val + 1);
  }
  XTCC ReturnClause() override {
    return XTCC(100);
  }
};

void testAnswerTypeVoid()
{
  std::cout << OneShot::HandleWith(
      [](){ OneShot::InvokeCmd(Cmd{}); },
      std::make_unique<TestAnswerTypeVoid>()).val
    << "\t(expected: 101)" << std::endl;
}

// ------------------------------------------------------------------

class TestBodyType : public Handler<int, XTCC, Cmd> {
  int CommandClause(Cmd, std::unique_ptr<Resumption<int, int>> r) override {
    return OneShot::Resume(std::move(r), 100) + 1;
  }
  int ReturnClause(XTCC v) override {
    return v.val;
  }
};

void testBodyType()
{
  std::cout << OneShot::HandleWith(
      [](){ return XTCC(OneShot::InvokeCmd(Cmd{}) + 10); },
      std::make_unique<TestBodyType>())
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class TestBodyTypeVoid : public Handler<void, XTCC, Cmd> {
  void CommandClause(Cmd, std::unique_ptr<Resumption<int, void>> r) override {
    std::cout << "*";
    OneShot::Resume(std::move(r), 100);
  }
  void ReturnClause(XTCC v) override {
    std::cout << v.val;
  }
};

void testBodyTypeVoid()
{
  OneShot::HandleWith(
    [](){ return XTCC(OneShot::InvokeCmd(Cmd{}) + 10); },
    std::make_unique<TestBodyTypeVoid>());
  std::cout << "\t(expected: *110)" << std::endl;
}
// ------------------------------------------------------------------

int main()
{
  testAnswerType();
  testAnswerTypeVoid();
  testBodyType();
  testBodyTypeVoid();
}
