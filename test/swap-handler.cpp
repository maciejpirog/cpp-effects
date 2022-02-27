// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Swapping handlers

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

class Bottom { Bottom() = delete; };

template <typename H>
struct CmdAid : Command<Bottom> {
  std::shared_ptr<H> han;
  Resumption<void, typename H::BodyType>* res;
};

template <typename H>
struct CmdAbet : Command<void> {
  std::shared_ptr<H> han;
};

template <typename H>
class Aid : public Handler<typename H::AnswerType, typename H::AnswerType, CmdAid<H>> {
  typename H::AnswerType CommandClause(CmdAid<H> c, std::unique_ptr<Resumption<Bottom, typename H::AnswerType>>) override {
    return OneShot::Handle<Aid<H>>([=](){
      return OneShot::HandleWith([=](){
          return OneShot::Resume(std::unique_ptr<Resumption<void, typename H::BodyType>>(c.res)); },
        c.han);
    });
  }
  typename H::AnswerType ReturnClause(typename H::AnswerType a) override
  {
    return a;
  }
}; 

template <typename H>
class Abet : public Handler<typename H::BodyType, typename H::BodyType, CmdAbet<H>> {
  [[noreturn]] typename H::BodyType CommandClause(CmdAbet<H> c,
    std::unique_ptr<Resumption<void, typename H::BodyType>> r) override
  {
    OneShot::InvokeCmd(CmdAid<H>{{}, c.han, r.release()});
    exit(-1); // This will never be reached
  }
  typename H::BodyType ReturnClause(typename H::BodyType b) override
  {
    return b;
  }
};

template <typename H>
typename H::AnswerType SwappableHandleWith(std::function<typename H::BodyType()> body, std::shared_ptr<H> handler)
{
  return OneShot::Handle<Aid<H>>([=](){
    return OneShot::HandleWith([=](){
        return OneShot::Handle<Abet<H>>(body);
      },
      std::move(handler));
  });
}

template <typename T>
struct Read : Command<T> { };

template <typename Answer, typename R>
using ReaderType = Handler<Answer, Answer, Read<R>>;

template <typename Answer, typename R>
class Reader : public ReaderType<Answer, R> {
public:
  Reader(R val) : val(val) { }
private:
  const R val;
  Answer CommandClause(Read<R>, std::unique_ptr<Resumption<R, Answer>> r) override
  {
    return OneShot::Resume(std::move(r), val);
  }
  Answer ReturnClause(Answer b) override
  {
    return b;
  }
};

void put(int a)
{
  OneShot::InvokeCmd(CmdAbet<ReaderType<int, int>>{{}, std::make_shared<Reader<int, int>>(a)});
}
int get()
{
  return OneShot::InvokeCmd(Read<int>{});
}

int comp()
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
  return 18;
}

int main()
{
  std::cout << "--- swap-handler ---" << std::endl;
  std::cout <<
    SwappableHandleWith(comp, std::shared_ptr<ReaderType<int, int>>(new Reader<int, int>(100)))
    << std::endl;

}

// Output:
// 100
// 110
// 310
// 310
// 320
// 18
