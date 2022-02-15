// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Swapping handlers

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

struct Read : Command<int> { };

using HType = Handler<void, void, Read>;

struct CmdAid : Command<void> {
  HType* han;
  Resumption<void, void>* res;
};

struct CmdAbet : Command<void> {
  HType* han;
};

class Aid : public Handler<void, void, CmdAid> {
  void CommandClause(CmdAid c, std::unique_ptr<Resumption<void, void>>) override {
    OneShot::Handle<Aid>([=](){
      OneShot::HandleWith([=](){
          OneShot::Resume(std::unique_ptr<Resumption<void, void>>(c.res)); },
        std::unique_ptr<HType>(c.han));
    });
  }
  void ReturnClause() override { }
}; 

class Abet : public Handler<void, void, CmdAbet> {
  void CommandClause(CmdAbet c, std::unique_ptr<Resumption<void, void>> r) override {
    OneShot::InvokeCmd(CmdAid{{}, c.han, r.release()});
  }
  void ReturnClause() override { }
}; 

class Reader : public HType {
public:
  Reader(int val) : val(val) { }
private:
  const int val;
  void CommandClause(Read, std::unique_ptr<Resumption<int,void>> r) override
  {
    OneShot::Resume(std::move(r), val);
  }
  void ReturnClause() override { }
};

void put(int a)
{
  OneShot::InvokeCmd(CmdAbet{{}, new Reader(a)});
}
int get()
{
  return OneShot::InvokeCmd(Read{});
}

void comp()
{
  std::cout << get() << std::endl;
  put(get() + 10);
  std::cout << get() << std::endl;
  put(200);
  put(300);
  put(get() + 10);
  std::cout << get() << std::endl;
  std::cout << get() << std::endl;
  put(get() + 10);
  std::cout << get() << std::endl;
}

int main()
{
  OneShot::Handle<Aid>([](){
    OneShot::HandleWith([](){
      OneShot::Handle<Abet>(comp);},
      std::make_unique<Reader>(100));
  });
}

// Output:
// 100
// 110
// 310
// 310
// 320
