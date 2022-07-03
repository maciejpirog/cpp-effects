// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

/*
Example: Different forms of state:

1. Stateful Handler -- In this example state is kept as a member in
the handler object. Commands change this state.

2. State using lambdas -- This is the usual way that state is
presented using non-parameterised handlers in pure languages. The
computation is interpreted as a function from the initial state to a
value, while commands are interpreted as appropriate composition of
these functions.

3. State using handler-switching -- This is a tricky implementation,
in which the Get command is interpreted using a "reader" handler. The
Set command is interpreted by switching the initial reader handler for
a different reader handler that provides the new state. This switching
is done by sandwiching the reader handlers in a pair of handlers: Aid
(on the outside) and Abet (on the inside). The role of Abet is to
capture the continuation without the reader's handler, and pass it to
Aid, which throws away its continuation (together with the old
reader), and handles (using the new reader) the resumed continuation
caught by Abet. On top of this (or rather on the inside of this), we
have the actual handler for Put and Get, which dispatches the commands
to the current reader and Abet.
*/

#include <iostream>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

namespace eff = cpp_effects;

// -----------------------------------
// Commands and programmer's interface
// -----------------------------------

template <typename S>
struct Put : eff::command<> {
  S newState;
};

template <typename S>
struct Get : eff::command<S> { };

template <typename S>
void put(S s) {
  eff::invoke_command(Put<S>{{}, s});
}

template <typename S>
S get() {
  return eff::invoke_command(Get<S>{});
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

template <typename Answer, typename S>
class HStateful : public eff::flat_handler<Answer, eff::plain<Put<S>>, eff::plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void handle_command(Put<S> p) final override
  {
    state = p.newState;
  }
  S handle_command(Get<S>) final override
  {
    return state;
  }
};

// Specialisation for Answer = void

void testStateful()
{
  eff::handle<HStateful<void, int>>(test, 100);
  std::cout << eff::handle<HStateful<std::string, int>>(test2, 100);
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
class HLambda : public eff::handler<std::function<Answer(S)>, Answer, Put<S>, Get<S>> {
  std::function<Answer(S)> handle_command(Put<S> p,
    eff::resumption<std::function<Answer(S)>()> r) override
  {
    return [p, r = r.release()](S) -> Answer {
      return eff::resumption<std::function<Answer(S)>()>(r).resume()(p.newState);
    };
  }
  std::function<Answer(S)> handle_command(Get<S>,
    eff::resumption<std::function<Answer(S)>(S)> r) override
  {
    return [r = r.release()](S s) -> Answer {
      return eff::resumption<std::function<Answer(S)>(S)>(r).resume(s)(s);
    };
  }
  std::function<Answer(S)> handle_return(Answer a) override
  {
    return [a](S){ return a; };
  }
};

template <typename S>
class HLambda<void, S> : public eff::handler<std::function<void(S)>, void, Put<S>, Get<S>> {
  std::function<void(S)> handle_command(Put<S> p,
    eff::resumption<std::function<void(S)>()> r) override
  {
    return [r = r.release(), p](S) -> void {
      eff::resumption<std::function<void(S)>()>(r).resume()(p.newState);
    };
  }
  std::function<void(S)> handle_command(Get<S>,
    eff::resumption<std::function<void(S)>(S)> r) override
  {
    return [r = r.release()](S s) -> void {
      eff::resumption<std::function<void(S)>(S)>(r).resume(s)(s);
    };
  }
  std::function<void(S)> handle_return() override
  {
    return [](S){ };
  }
};

void testLambda()
{
  eff::handle<HLambda<void, int>>(test)(100);
  std::cout << eff::handle<HLambda<std::string, int>>(test2)(100);
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
        return eff::resumption<typename H::body_type()>(c.res).resume();
      }, c.han);
    });
  }
  typename H::answer_type handle_return(typename H::answer_type a) override
  {
    return a;
  }
}; 

template <typename H>
class Abet : public eff::handler<typename H::body_type, typename H::body_type, CmdAbet<H>> {
  [[noreturn]] typename H::body_type handle_command(CmdAbet<H> c, eff::resumption<typename H::body_type()> r) override {
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
  const R val;  // Note the const modifier!
  Answer handle_command(Read<R>, eff::resumption<Answer(int)> r) override
  {
    return std::move(r).tail_resume(val);
  }
  Answer handle_return(Answer b) override
  {
    return b;
  }
};

template<typename Answer, typename S>
class HSwitching : public eff::handler<Answer, Answer, Put<S>, Get<S>> {
  Answer handle_command(Put<S> p, eff::resumption<Answer()> r) override
  {
    eff::invoke_command(
      CmdAbet<ReaderType<Answer, S>>{{}, std::make_shared<Reader<Answer, S>>(p.newState)});
    return std::move(r).resume();
  }
  Answer handle_command(Get<S>, eff::resumption<Answer(S)> r) override
  {
    return std::move(r).resume(eff::invoke_command(Read<S>{}));
  }
  Answer handle_return(Answer a) override
  {
    return a;
  }
};

// TODO: Overloads for Answer = void

void testSwitching()
{
  std::cout << SwappableHandleWith(
    [](){ return eff::handle<HSwitching<std::string, int>>(test2); },
    std::shared_ptr<ReaderType<std::string, int>>(new Reader<std::string, int>(100)));

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
