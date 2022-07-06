// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

/* Test: Traits of type arguments one can use in the library:

- AnswerType - at least move-costructible
- BodyType - at least move-constructible
- classes derived from eff::command - at least copy-constructible
- OutType - at least move-constructible and move-assignable
*/

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

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

struct Cmd : eff::command<int> { };

// ------------------------------------------------------------------

class TestAnswerType : public eff::handler<XTCC, int, Cmd> {
  XTCC handle_command(Cmd, eff::resumption<XTCC(int)> r) override {
    return XTCC(std::move(r).resume(100).val + 1);
  }
  XTCC handle_return(int v) override {
    return XTCC(v);
  }
};

void testAnswerType()
{
  std::cout << eff::handle_with(
      [](){ return eff::invoke_command(Cmd{}) + 10; },
      std::make_shared<TestAnswerType>()).val
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class TestFlatAnswerType : public eff::flat_handler<XTCC, Cmd> {
  XTCC handle_command(Cmd, eff::resumption<XTCC(int)> r) override {
    return XTCC(std::move(r).resume(100).val + 1);
  }
};

void testFlatAnswerType()
{
  std::cout << eff::handle_with(
      [](){ return eff::invoke_command(Cmd{}) + 10; },
      std::make_shared<TestFlatAnswerType>()).val
    << "\t(expected: 111)" << std::endl;
}


// ------------------------------------------------------------------

class TestAnswerTypeVoid : public eff::handler<XTCC, void, Cmd> {
  XTCC handle_command(Cmd, eff::resumption<XTCC(int)> r) override {
    return XTCC(std::move(r).resume(100).val + 1);
  }
  XTCC handle_return() override {
    return XTCC(100);
  }
};

void testAnswerTypeVoid()
{
  std::cout << eff::handle_with(
      [](){ eff::invoke_command(Cmd{}); },
      std::make_shared<TestAnswerTypeVoid>()).val
    << "\t(expected: 101)" << std::endl;
}

// ------------------------------------------------------------------

class TestBodyType : public eff::handler<int, XTCC, Cmd> {
  int handle_command(Cmd, eff::resumption<int(int)> r) override {
    return std::move(r).resume(100) + 1;
  }
  int handle_return(XTCC v) override {
    return v.val;
  }
};

void testBodyType()
{
  std::cout << eff::handle_with(
      [](){ return XTCC(eff::invoke_command(Cmd{}) + 10); },
      std::make_shared<TestBodyType>())
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class TestBodyTypeVoid : public eff::handler<void, XTCC, Cmd> {
  void handle_command(Cmd, eff::resumption<void(int)> r) override {
    std::cout << "*";
    std::move(r).resume(100);
  }
  void handle_return(XTCC v) override {
    std::cout << v.val;
  }
};

void testBodyTypeVoid()
{
  eff::handle_with(
    [](){ return XTCC(eff::invoke_command(Cmd{}) + 10); },
    std::make_shared<TestBodyTypeVoid>());
  std::cout << "\t(expected: *110)" << std::endl;
}

// ------------------------------------------------------------------

class CmdX : public eff::command<int> {
public:
  CmdX(int v) : val(v) { }
  CmdX(const CmdX&) = default;

  CmdX() = delete;
  CmdX& operator=(CmdX&&) = delete;
  CmdX& operator=(const CmdX&) = delete;

  int val;
};

// ------------------------------------------------------------------

class XTestAnswerType : public eff::handler<XTCC, int, CmdX> {
  XTCC handle_command(CmdX, eff::resumption<XTCC(int)> r) override {
    return XTCC(std::move(r).resume(100).val + 1);
  }
  XTCC handle_return(int v) override {
    return XTCC(v);
  }
};

void xTestAnswerType()
{
  std::cout << eff::handle_with(
      [](){ return eff::invoke_command(CmdX(0)) + 10; },
      std::make_shared<XTestAnswerType>()).val
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class XTestAnswerTypeVoid : public eff::handler<XTCC, void, CmdX> {
  XTCC handle_command(CmdX, eff::resumption<XTCC(int)> r) override {
    return XTCC(std::move(r).resume(100).val + 1);
  }
  XTCC handle_return() override {
    return XTCC(100);
  }
};

void xTestAnswerTypeVoid()
{
  std::cout << eff::handle_with(
      [](){ eff::invoke_command(CmdX(0)); },
      std::make_shared<XTestAnswerTypeVoid>()).val
    << "\t(expected: 101)" << std::endl;
}

// ------------------------------------------------------------------

class XTestBodyType : public eff::handler<int, XTCC, CmdX> {
  int handle_command(CmdX, eff::resumption<int(int)> r) override {
    return std::move(r).resume(100) + 1;
  }
  int handle_return(XTCC v) override {
    return v.val;
  }
};

void xTestBodyType()
{
  std::cout << eff::handle_with(
      [](){ return XTCC(eff::invoke_command(CmdX(0)) + 10); },
      std::make_shared<XTestBodyType>())
    << "\t(expected: 111)" << std::endl;
}

// ------------------------------------------------------------------

class XTestBodyTypeVoid : public eff::handler<void, XTCC, CmdX> {
  void handle_command(CmdX c, eff::resumption<void(int)> r) override {
    if (c.val != -1) { std::cout << "*"; }
    std::move(r).resume(100);
  }
  void handle_return(XTCC v) override {
    std::cout << v.val;
  }
};

void xTestBodyTypeVoid()
{
  eff::handle_with(
    [](){ return XTCC(eff::invoke_command(CmdX(0)) + 10); },
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

struct CmdY : public eff::command<YTCC> { int val; };

// ------------------------------------------------------------------

class YTestOutType : public eff::handler<int, int, CmdY> {
  int handle_command(CmdY c, eff::resumption<int(YTCC)> r) override {
    return std::move(r).resume(YTCC(c.val)) + 1;
  }
  int handle_return(int v) override {
    return v;
  }
};

void yTestOutType()
{
  std::cout << eff::handle_with(
      [](){ return eff::invoke_command(CmdY{{}, 100}).val + 10; },
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
