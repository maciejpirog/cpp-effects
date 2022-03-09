// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

/* Test: Traits of type arguments one can use in the library:

- AnswerType - at least move-costructible
- BodyType - at least move-constructible
- classes derived from Command - at least copy-constructible
- OutType - at least move-constructible and move-assignable
*/

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

class XTCC { // exclude trivial constructor and copy
public:
  XTCC(int v) : val(v) { }
  XTCC(XTCC&&) = default;
  XTCC& operator=(XTCC&&) = default;

  XTCC() = delete;
  XTCC(const XTCC&) = delete;
  XTCC& operator=(const XTCC&) = delete;

  int val;
};

struct Cmd : Command<int> { };

// ------------------------------------------------------------------

class TestAnswerType : public Handler<XTCC, int, Cmd> {
  XTCC CommandClause(Cmd, Resumption<int, XTCC> r) override {
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
      std::make_shared<TestAnswerType>()).val
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class TestFlatAnswerType : public FlatHandler<XTCC, Cmd> {
  XTCC CommandClause(Cmd, Resumption<int, XTCC> r) override {
    return XTCC(OneShot::Resume(std::move(r), 100).val + 1);
  }
};

void testFlatAnswerType()
{
  std::cout << OneShot::HandleWith(
      [](){ return OneShot::InvokeCmd(Cmd{}) + 10; },
      std::make_shared<TestFlatAnswerType>()).val
    << "\t(expected: 111)" << std::endl;
}


// ------------------------------------------------------------------

class TestAnswerTypeVoid : public Handler<XTCC, void, Cmd> {
  XTCC CommandClause(Cmd, Resumption<int, XTCC> r) override {
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
      std::make_shared<TestAnswerTypeVoid>()).val
    << "\t(expected: 101)" << std::endl;
}

// ------------------------------------------------------------------

class TestBodyType : public Handler<int, XTCC, Cmd> {
  int CommandClause(Cmd, Resumption<int, int> r) override {
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
      std::make_shared<TestBodyType>())
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class TestBodyTypeVoid : public Handler<void, XTCC, Cmd> {
  void CommandClause(Cmd, Resumption<int, void> r) override {
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
    std::make_shared<TestBodyTypeVoid>());
  std::cout << "\t(expected: *110)" << std::endl;
}

// ------------------------------------------------------------------

class CmdX : public Command<int> {
public:
  CmdX(int v) : val(v) { }
  CmdX(const CmdX&) = default;

  CmdX() = delete;
  CmdX& operator=(CmdX&&) = delete;
  CmdX& operator=(const CmdX&) = delete;

  int val;
};

// ------------------------------------------------------------------

class XTestAnswerType : public Handler<XTCC, int, CmdX> {
  XTCC CommandClause(CmdX, Resumption<int, XTCC> r) override {
    return XTCC(OneShot::Resume(std::move(r), 100).val + 1);
  }
  XTCC ReturnClause(int v) override {
    return XTCC(v);
  }
};

void xTestAnswerType()
{
  std::cout << OneShot::HandleWith(
      [](){ return OneShot::InvokeCmd(CmdX(0)) + 10; },
      std::make_shared<XTestAnswerType>()).val
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class XTestAnswerTypeVoid : public Handler<XTCC, void, CmdX> {
  XTCC CommandClause(CmdX, Resumption<int, XTCC> r) override {
    return XTCC(OneShot::Resume(std::move(r), 100).val + 1);
  }
  XTCC ReturnClause() override {
    return XTCC(100);
  }
};

void xTestAnswerTypeVoid()
{
  std::cout << OneShot::HandleWith(
      [](){ OneShot::InvokeCmd(CmdX(0)); },
      std::make_shared<XTestAnswerTypeVoid>()).val
    << "\t(expected: 101)" << std::endl;
}

// ------------------------------------------------------------------

class XTestBodyType : public Handler<int, XTCC, CmdX> {
  int CommandClause(CmdX, Resumption<int, int> r) override {
    return OneShot::Resume(std::move(r), 100) + 1;
  }
  int ReturnClause(XTCC v) override {
    return v.val;
  }
};

void xTestBodyType()
{
  std::cout << OneShot::HandleWith(
      [](){ return XTCC(OneShot::InvokeCmd(CmdX(0)) + 10); },
      std::make_shared<XTestBodyType>())
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class XTestBodyTypeVoid : public Handler<void, XTCC, CmdX> {
  void CommandClause(CmdX c, Resumption<int, void> r) override {
    if (c.val != -1) { std::cout << "*"; }
    OneShot::Resume(std::move(r), 100);
  }
  void ReturnClause(XTCC v) override {
    std::cout << v.val;
  }
};

void xTestBodyTypeVoid()
{
  OneShot::HandleWith(
    [](){ return XTCC(OneShot::InvokeCmd(CmdX(0)) + 10); },
    std::make_shared<XTestBodyTypeVoid>());
  std::cout << "\t(expected: *110)" << std::endl;
}

// ------------------------------------------------------------------

class YTCC {
public:
  YTCC(int v) : val(v) { }
  YTCC(YTCC&&) = default;

  YTCC() = delete;
  YTCC(const YTCC&) = delete;
  YTCC& operator=(YTCC&&) = default;
  YTCC& operator=(const YTCC&) = delete;

  int val;
};

struct CmdY : public Command<YTCC> { int val; };

// ------------------------------------------------------------------

class YTestOutType : public Handler<int, int, CmdY> {
  int CommandClause(CmdY c, Resumption<YTCC, int> r) override {
    return OneShot::Resume(std::move(r), YTCC(c.val)) + 1;
  }
  int ReturnClause(int v) override {
    return v;
  }
};

void yTestOutType()
{
  std::cout << OneShot::HandleWith(
      [](){ return OneShot::InvokeCmd(CmdY{{}, 100}).val + 10; },
      std::make_shared<YTestOutType>())
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

int main()
{
  std::cout << "--- traits ---" << std::endl;
  testAnswerType();
  testFlatAnswerType();
  testAnswerTypeVoid();
  testBodyType();
  testBodyTypeVoid();
  xTestAnswerType();
  xTestAnswerTypeVoid();
  xTestBodyType();
  xTestBodyTypeVoid();
  yTestOutType();
}
