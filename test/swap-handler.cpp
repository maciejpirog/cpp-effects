// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Test: Swapping handlers

#include <functional>
#include <iostream>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

class Bottom { Bottom() = delete; };

template <typename H>
struct CmdAid : eff::command<Bottom> {
  std::shared_ptr<H> han;
  eff::resumption_data<void, typename H::body_type>* res;
};

template <typename H>
struct CmdAbet : eff::command<> {
  std::shared_ptr<H> han;
};

template <typename H>
class Aid : public eff::handler<typename H::answer_type, typename H::answer_type, CmdAid<H>> {
  typename H::answer_type handle_command(CmdAid<H> c, eff::resumption<typename H::answer_type(Bottom)>) override {
    return eff::handle<Aid<H>>([=](){
      return eff::handle_with([=](){
          return eff::resumption<typename H::body_type()>(c.res).resume(); },
        c.han);
    });
  }
  typename H::answer_type handle_return(typename H::answer_type a) override
  {
    return a;
  }
}; 

template <typename H>
class Abet : public eff::handler<typename H::body_type, typename H::body_type, CmdAbet<H>> {
  [[noreturn]] typename H::body_type handle_command(CmdAbet<H> c,
    eff::resumption<typename H::body_type()> r) override
  {
    eff::invoke_command(CmdAid<H>{{}, c.han, r.release()});
    exit(-1); // This will never be reached
  }
  typename H::body_type handle_return(typename H::body_type b) override
  {
    return b;
  }
};

template <typename H>
typename H::answer_type SwappableHandleWith(std::function<typename H::body_type()> body, std::shared_ptr<H> handler)
{
  return eff::handle<Aid<H>>([=](){
    return eff::handle_with([=](){
        return eff::handle<Abet<H>>(body);
      },
      std::move(handler));
  });
}

template <typename T>
struct Read : eff::command<T> { };

template <typename Answer, typename R>
using ReaderType = eff::handler<Answer, Answer, Read<R>>;

template <typename Answer, typename R>
class Reader : public ReaderType<Answer, R> {
public:
  Reader(R val) : val(val) { }
private:
  const R val;
  Answer handle_command(Read<R>, eff::resumption<Answer(R)> r) override
  {
    return std::move(r).resume(val);
  }
  Answer handle_return(Answer b) override
  {
    return b;
  }
};

void put(int a)
{
  eff::invoke_command(CmdAbet<ReaderType<int, int>>{{}, std::make_shared<Reader<int, int>>(a)});
}
int get()
{
  return eff::invoke_command(Read<int>{});
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
