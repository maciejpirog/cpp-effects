// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Different forms of state:

#include <iostream>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

// -----------------------------------
// Commands and programmer's interface
// -----------------------------------

template<typename S>
struct Put : Command<void> {
  S newState;
};

template<typename S>
struct Get : Command<S> { };

template<typename S>
void put(S s) {
  OneShot::InvokeCmd(Put<S>{{}, s});
}

template<typename S>
S get() {
  return OneShot::InvokeCmd(Get<S>{});
}

// ----------------------
// Particular computation
// ----------------------

void test()
{
    std::cout << get<int>() << " ";
    put(get<int>() + 1);
    std::cout << get<int>() << " ";
    put(get<int>() * get<int>());
    std::cout << get<int>() << std::endl;
}

std::string test2()
{
    std::cout << get<int>() << " ";
    put(get<int>() + 1);
    std::cout << get<int>() << " ";
    put(get<int>() * get<int>());
    std::cout << get<int>() << std::endl;
    return "ok";
}

// -------------------
// 1. Stateful handler
// -------------------

template<typename Answer, typename S>
class HStateful : public Handler<Answer, Answer, Put<S>, Get<S>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  Answer CommandClause(Put<S> p, std::unique_ptr<Resumption<void, Answer>> r) override
  {
    state = p.newState;
    return OneShot::TailResume(std::move(r));
  }
  Answer CommandClause(Get<S>, std::unique_ptr<Resumption<S, Answer>> r) override
  {
    return OneShot::TailResume(std::move(r), state);
  }
  Answer ReturnClause(Answer a) override
  {
    return a;
  }
};

// Specialisation for Answer = void

template<typename S>
class HStateful<void, S> : public Handler<void, void, Put<S>, Get<S>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void CommandClause(Put<S> p, std::unique_ptr<Resumption<void, void>> r) override
  {
    state = p.newState;
    OneShot::TailResume(std::move(r));
  }
  void CommandClause(Get<S>, std::unique_ptr<Resumption<S, void>> r) override
  {
    OneShot::TailResume(std::move(r), state);
  }
  void ReturnClause() override { }
};

void testStateful()
{
  OneShot::HandleWith(test, std::make_unique<HStateful<void, int>>(100));
  std::cout << OneShot::HandleWith(test2, std::make_unique<HStateful<std::string, int>>(100));
  std::cout << std::endl;

  // Output:
  // 100 101 10201
  // 100 101 10201
  // ok
}

// ----------------------
// 2. State using lambdas
// ----------------------

template <typename Answer, typename S>
class HLambda : public Handler<std::function<Answer(S)>, Answer, Put<S>, Get<S>> {
  std::function<Answer(S)> CommandClause(Put<S> p,
    std::unique_ptr<Resumption<void, std::function<Answer(S)>>> r) override
  {
    return [p, r = r.release()](S) -> Answer {
      return OneShot::Resume(std::unique_ptr<Resumption<void, std::function<Answer(S)>>>(r))(p.newState);
    };
  }
  std::function<Answer(S)> CommandClause(Get<S>,
    std::unique_ptr<Resumption<S, std::function<Answer(S)>>> r) override
  {
    return [r = r.release()](S s) -> Answer {
      return OneShot::Resume(std::unique_ptr<Resumption<S, std::function<Answer(S)>>>(r), s)(s);
    };
  }
  std::function<Answer(S)> ReturnClause(Answer a) override
  {
    return [a](S){ return a; };
  }
};

template <typename S>
class HLambda<void, S> : public Handler<std::function<void(S)>, void, Put<S>, Get<S>> {
  std::function<void(S)> CommandClause(Put<S> p,
    std::unique_ptr<Resumption<void, std::function<void(S)>>> r) override
  {
    return [r = r.release(), p](S) -> void {
      OneShot::Resume(std::unique_ptr<Resumption<void, std::function<void(S)>>>(r))(p.newState);
    };
  }
  std::function<void(S)> CommandClause(Get<S>,
    std::unique_ptr<Resumption<S, std::function<void(S)>>> r) override
  {
    return [r = r.release()](S s) -> void {
      OneShot::Resume(std::unique_ptr<Resumption<S, std::function<void(S)>>>(r), s)(s);
    };
  }
  std::function<void(S)> ReturnClause() override
  {
    return [](S){ };
  }
};

void testLambda()
{
  OneShot::HandleWith(test, std::make_unique<HLambda<void, int>>())(100);
  std::cout << OneShot::HandleWith(test2, std::make_unique<HLambda<std::string, int>>())(100);
  std::cout << std::endl;

  // Output:
  // 100 101 10201
  // 100 101 10201
  // ok
}

// --------------------------------
// 3. State using handler-switching
// --------------------------------

class Bottom { Bottom() = delete; };

template <typename H>
struct CmdAid : Command<Bottom> {
  H* han;
  Resumption<void, typename H::BodyType>* res;
};

template <typename H>
struct CmdAbet : Command<void> {
  H* han;
};

template <typename H>
class Aid : public Handler<typename H::AnswerType, typename H::AnswerType, CmdAid<H>> {
  typename H::AnswerType CommandClause(CmdAid<H> c, std::unique_ptr<Resumption<Bottom, typename H::AnswerType>>) override {
    return OneShot::Handle<Aid<H>>([=](){
      return OneShot::HandleWith([=](){
          return OneShot::Resume(std::unique_ptr<Resumption<void, typename H::BodyType>>(c.res)); },
        std::unique_ptr<H>(c.han));
    });
  }
  typename H::AnswerType ReturnClause(typename H::AnswerType a) override
  {
    return a;
  }
}; 

template <typename H>
class Abet : public Handler<typename H::BodyType, typename H::BodyType, CmdAbet<H>> {
  [[noreturn]] typename H::BodyType CommandClause(CmdAbet<H> c, std::unique_ptr<Resumption<void, typename H::BodyType>> r) override {
    OneShot::InvokeCmd(CmdAid<H>{{}, c.han, r.release()});
    exit(-1); // This will never be reached
  }
  typename H::BodyType ReturnClause(typename H::BodyType b) override
  {
    return b;
  }
};

template <typename H>
typename H::AnswerType SwappableHandleWith(std::function<typename H::BodyType()> body, std::unique_ptr<H> handler)
{
  auto h = handler.release();
  return OneShot::Handle<Aid<H>>([=](){
    return OneShot::HandleWith([=](){
        return OneShot::Handle<Abet<H>>(body);
      },
      std::unique_ptr<H>(h));
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
  const R val;  // Note the const modifier!!!
  Answer CommandClause(Read<R>, std::unique_ptr<Resumption<int,Answer>> r) override
  {
    return OneShot::TailResume(std::move(r), val);
  }
  Answer ReturnClause(Answer b) override
  {
    return b;
  }
};

template<typename Answer, typename S>
class HSwitching : public Handler<Answer, Answer, Put<S>, Get<S>> {
  Answer CommandClause(Put<S> p, std::unique_ptr<Resumption<void, Answer>> r) override
  {
    OneShot::InvokeCmd(CmdAbet<ReaderType<Answer, S>>{{}, new Reader<Answer, S>(p.newState)});
    return OneShot::Resume(std::move(r));
  }
  Answer CommandClause(Get<S>, std::unique_ptr<Resumption<S, Answer>> r) override
  {
    return OneShot::Resume(std::move(r), OneShot::InvokeCmd(Read<S>{}));
  }
  Answer ReturnClause(Answer a) override
  {
    return a;
  }
};

// TODO: Overloads for Answer = void

void testSwitching()
{
  std::cout << SwappableHandleWith(
    [](){ return OneShot::Handle<HSwitching<std::string, int>>(test2); },
    std::unique_ptr<ReaderType<std::string, int>>(new Reader<std::string, int>(100)));

  std::cout << std::endl;

  // Output:
  // 100 101 10201
  // ok
}

// ---------
// Run tests
// ---------
 
int main()
{
  testStateful();
  testLambda();
  testSwitching();
}
