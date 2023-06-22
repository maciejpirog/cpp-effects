// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Main header file for the library

#ifndef CPP_EFFECTS_CPP_EFFECTS_H
#define CPP_EFFECTS_CPP_EFFECTS_H

// Use Boost with support for Valgrind
/*
#ifndef BOOST_USE_VALGRIND
#define BOOST_USE_VALGRIND
#endif
*/

#include <boost/context/fiber.hpp>

// For different stack use policies, e.g.,
// #include <boost/context/protected_fixedsize_stack.hpp>

#include <functional>
#include <iostream>
#include <list>
#include <optional>
#include <typeinfo>
#include <tuple>

namespace cpp_effects {

namespace ctx = boost::context;

// ---------------
// API - datatypes
// ---------------

// Commands

template <typename... Outs>
struct command;

template <typename Out>
struct command<Out> {
  using out_type = Out;
  template <typename Answer> using resumption_type = Answer(Out);
};

template <>
struct command<> {
  using out_type = void;
  template <typename Answer> using resumption_type = Answer();
};

// Resumptions

class resumption_base;

template <typename Out, typename Answer>
class resumption_data;

template <typename T>
class resumption;

// Handlers

template <typename Answer, typename Body, typename... Cmds>
class handler;

template <typename Answer, typename... Cmds>
class flat_handler;

// Handler references (experimental)

namespace cpp_effects_internals {

  class metaframe;

  class init_metastack;

  using metaframe_ptr = std::shared_ptr<metaframe>;

}

using handler_ref = std::list<cpp_effects_internals::metaframe_ptr>::iterator;

// ---------------
// API - functions
// ---------------

// Misc

int64_t fresh_label();

void debug_print_metastack();

// Handling

template <typename H, typename... Args>
typename H::answer_type handle(
    int64_t label, std::function<typename H::body_type()> body, Args&&... args);

template <typename H, typename... Args>
typename H::answer_type handle(std::function<typename H::body_type()> body, Args&&... args);

template <typename H>
typename H::answer_type handle_with(
    int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

template <typename H>
typename H::answer_type handle_with(
    std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

template <typename H, typename... Args>
typename H::answer_type handle_ref(
    int64_t label, std::function<typename H::body_type(handler_ref)> body, Args&&... args);

template <typename H, typename... Args>
typename H::answer_type handle_ref(
    std::function<typename H::body_type(handler_ref)> body, Args&&... args);

template <typename H>
typename H::answer_type handle_with_ref(
    int64_t label,
    std::function<typename H::body_type(handler_ref)> body, 
    std::shared_ptr<H> handler);

template <typename H>
typename H::answer_type handle_with_ref(
    std::function<typename H::body_type(handler_ref)> body, std::shared_ptr<H> handler);

// Lifting a function to a resumption by wrapping it in a handler

template <typename H, typename... Args>
resumption<typename H::answer_type()> wrap(
    int64_t label, std::function<typename H::body_type()> body, Args&&... args);

template <typename H, typename A, typename... Args>
resumption<typename H::answer_type(A)> wrap(
    int64_t label, std::function<typename H::body_type(A)> body, Args&&... args);

template <typename H, typename... Args>
resumption<typename H::answer_type()> wrap(
    std::function<typename H::body_type()> body, Args&&... args);

template <typename H, typename A, typename... Args>
resumption<typename H::answer_type(A)> wrap(
    std::function<typename H::body_type(A)> body, Args&&... args);

template <typename H>
resumption<typename H::answer_type()> wrap_with(
    int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

template <typename H, typename A>
resumption<typename H::answer_type()> wrap_with(
    int64_t label, std::function<typename H::body_type(A)> body, std::shared_ptr<H> handler);

template <typename H>
resumption<typename H::answer_type()> wrap_with(
    std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

template <typename H, typename A>
resumption<typename H::answer_type()> wrap_with(
    std::function<typename H::body_type(A)> body, std::shared_ptr<H> handler);

// Invoking commands

template <typename Cmd>
typename Cmd::out_type invoke_command(int64_t goto_handler, const Cmd& cmd);

template <typename Cmd>
typename Cmd::out_type invoke_command(const Cmd& cmd);

template <typename Cmd>
typename Cmd::out_type invoke_command(handler_ref it, const Cmd& cmd);

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(int64_t goto_handler, const Cmd& cmd);

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(const Cmd& cmd);

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(handler_ref it, const Cmd& cmd);

// Find a reference to a handler on the metastack

template <typename Cmd>
handler_ref find_handler();

handler_ref find_handler(int64_t goto_handler);

// ---------
// Internals
// ---------

namespace cpp_effects_internals {

// -----------------------------------------------------------
// Internals - auxiliary class to deal with voids in templates
// -----------------------------------------------------------

template <typename T>
struct tangible {
  T value;
  tangible() = delete;
  tangible(T&& t) : value(std::move(t)) { }
  tangible(const std::function<T()>& f) : value(f()) { }
};

template <>
struct tangible<void> {
  tangible() = default;
  tangible(const std::function<void()>& f) { f(); }
};

// ----------------------
// Internals - metaframes
// ----------------------

// Labels:
// 0   -- reserved for the initial fiber with no handler
// >0  -- user-defined labels
// <0  -- auto-generated labels

class metaframe {
public:
  virtual ~metaframe() { }
  virtual void debug_print() const
  {
    std::cout << label << ":" << typeid(*this).name() << std::endl;
  }
  metaframe() : label(0) { }
  int64_t label;
  ctx::fiber fiber;
  void* return_buffer;
};

// When invoking a command in the client code, we know the type of the
// command, but we cannot know the type of the handler. In particular,
// we cannot know the answer type, hence we cannot simply up-cast the
// found handler to the class handler<...>. Instead, handler inherits
// (indirectly) from the class can_invoke_command, which allows us to
// call appropriate command clause of the handler without knowing the
// answer type.

template <typename Cmd>
class can_invoke_command {
public:
  virtual typename Cmd::out_type invoke_command(
    std::list<cpp_effects_internals::metaframe_ptr>::iterator it, const Cmd& cmd) = 0;
};

// The command_clause class is used to define a handler with a command clause
// for a particular operation. It inherits from can_invoke_command (see above),
// and overrides invoke_command, which means that the user (who cannot know the
// answer type of a handler!) can call the command clause of the handler anyway,
// by up-casting to can_invoke_command.

template <typename Answer, typename Cmd>
class command_clause : public can_invoke_command<Cmd> {
  template <typename, typename, typename...> friend class handler;
  template <typename, typename...> friend class flat_handler;
public:
  virtual typename Cmd::out_type invoke_command(
    std::list<cpp_effects_internals::metaframe_ptr>::iterator it, const Cmd& cmd) final override;
protected:
  virtual Answer handle_command(
      Cmd, resumption<typename Cmd::template resumption_type<Answer>>) = 0;
private:
  resumption_data<typename Cmd::out_type, Answer> resumptionBuffer;
};

// ---------------------
// Internals - metastack
// ---------------------

// Invariant: There is always at least one frame on the metastack.

inline std::list<metaframe_ptr> metastack;

class init_metastack
{
public:
  init_metastack()
  {
    if (metastack.empty()) {
      auto initmetaframe = std::make_shared<metaframe>();
      metastack.push_front(initmetaframe);
    }
  }  
} inline init_metastack_v;

// ------------------------------------------------------------
// Internals - implementation of command_clause::invoke_command
// ------------------------------------------------------------

template <typename Answer, typename Cmd>
typename Cmd::out_type command_clause<Answer, Cmd>::invoke_command(
    std::list<cpp_effects_internals::metaframe_ptr>::iterator it, const Cmd& cmd)
{
  using namespace cpp_effects_internals;
  using Out = typename Cmd::out_type;

  // (continued from invoke_command) ...looking for [d]
  resumption_data<Out, Answer>& rd = this->resumptionBuffer;
  rd.stored_metastack.splice(rd.stored_metastack.begin(), metastack, metastack.begin(), it);
  // at this point: [a][b][c]; stored stack = [d][e][f][g.] 

  std::move(metastack.front()->fiber).resume_with([&](ctx::fiber&& prev) -> ctx::fiber {
    // at this point: [a][b][c.]; stored stack = [d][e][f][g.]
    rd.stored_metastack.front()->fiber = std::move(prev);
    // at this point: [a][b][c.]; stored stack = [d][e][f][g]

    // Keep the handler alive for the duration of the command clause call
    cpp_effects_internals::metaframe_ptr _(rd.stored_metastack.back());

    if constexpr (!std::is_void<Answer>::value) {
      *(static_cast<std::optional<Answer>*>(metastack.front()->return_buffer)) =
        this->handle_command(
            cmd, resumption<typename Cmd::template resumption_type<Answer>>(rd));
    } else {
      this->handle_command(cmd, resumption<typename Cmd::template resumption_type<Answer>>(rd));
    }
    return ctx::fiber();
  });

  // If the control reaches here, this means that the resumption is
  // being resumed at the moment, and so we no longer need the
  // resumption object.
  if constexpr (!std::is_void<Out>::value) {
    Out cmdResult = std::move(rd.command_result_buffer->value);
    rd.stored_metastack.clear();
    rd.command_result_buffer = {};
    return cmdResult;
  } else {
    rd.stored_metastack.clear();
  }
}

// ------------------------
// Internals - tail resumes
// ------------------------

// As there is no real forced TCO in C++, we need a separate mechanism
// for tail-resumptive handlers that will not build up call frames.

std::optional<resumption_base*> inline tail_resumption = {};

// ----------------
// End of internals
// ----------------

} // namespace cpp_effects_internals

// -----------------------------------
// API - implementation of resumptions
// -----------------------------------

class resumption_base {
  template <typename H> friend
  typename H::answer_type handle_with(
    int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);
  template <typename> friend class resumption;
  template <typename, typename> friend class resumption_data;
public:
  virtual ~resumption_base() { }
private:
  virtual void tail_resume() = 0;
};

template <typename Out, typename Answer>
class resumption_data final : public resumption_base {
  template <typename H>
  friend typename H::answer_type handle_with(
      int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);
  template <typename, typename> friend class cpp_effects_internals::command_clause;
  template <typename> friend class resumption;
private:
  std::optional<cpp_effects_internals::tangible<Out>> command_result_buffer;
  Answer resume();
  std::list<cpp_effects_internals::metaframe_ptr> stored_metastack;
  virtual void tail_resume() override;
};

template <typename Out, typename Answer>
class resumption<Answer(Out)> {
public:
  resumption() { }
  resumption(resumption_data<Out, Answer>* data) : data(data) { }
  resumption(resumption_data<Out, Answer>& data) : data(&data) { }
  resumption(std::function<Answer(Out)>);
  resumption(const resumption<Answer(Out)>&) = delete;
  resumption(resumption<Answer(Out)>&& other)
  {
    data = other.data;
    other.data = nullptr;
  }
  resumption& operator=(const resumption<Answer(Out)>&) = delete;
  resumption& operator=(resumption<Answer(Out)>&& other)
  {
    if (this != &other) {
      data = other.data;
      other.data = nullptr;
    }
    return *this;
  }
  ~resumption()
  {
    if (data) {
      data->command_result_buffer = {};

      // We move the resumption buffer out of the metaframe to break
      // the pointer/stack cycle.
      std::list<cpp_effects_internals::metaframe_ptr> _(std::move(data->stored_metastack));
    }
  }
  explicit operator bool() const
  {
    return data != nullptr && (bool)data->stored_metastack.back().fiber;
  }
  bool operator!() const
  {
    return data == nullptr || !data->stored_metastack.back().fiber;
  }
  resumption_data<Out, Answer>* release()
  {
    auto d = data;
    data = nullptr;
    return d;
  }
  Answer resume(Out cmdResult) &&
  {
    data->command_result_buffer->value = std::move(cmdResult);
    return release()->resume();
  }
  Answer tail_resume(Out cmdResult) &&;
private:
  resumption_data<Out, Answer>* data = nullptr;
};

template <typename Answer>
class resumption<Answer()> {
public:
  resumption() { }
  resumption(resumption_data<void, Answer>* data) : data(data) { }
  resumption(resumption_data<void, Answer>& data) : data(&data) { }
  resumption(std::function<Answer()>);
  resumption(const resumption<Answer()>&) = delete;
  resumption(resumption<Answer()>&& other)
  {
    data = other.data;
    other.data = nullptr;
  }
  resumption& operator=(const resumption<Answer()>&) = delete;
  resumption& operator=(resumption<Answer()>&& other)
  {
    if (this != &other) {
      data = other.data;
      other.data = nullptr;
    }
    return *this;
  }
  ~resumption()
  {
    if (data) {
      data->command_result_buffer = {};

      // We move the resumption buffer out of the metaframe to break
      // the pointer/stack cycle.
      std::list<cpp_effects_internals::metaframe_ptr> _(std::move(data->stored_metastack));
    }
  }
  explicit operator bool() const
  {
    return data != nullptr && (bool)data->stored_metastack.back().fiber;
  }
  bool operator!() const
  {
    return data == nullptr || !data->stored_metastack.back().fiber;
  }
  resumption_data<void, Answer>* release()
  {
    auto d = data;
    data = nullptr;
    return d;
  }
  Answer resume() &&
  {
    return release()->resume();
  }
  Answer tail_resume() &&;
private:
  resumption_data<void, Answer>* data = nullptr;
};

template <typename Out, typename Answer>
resumption<Answer(Out)>::resumption(std::function<Answer(Out)> func)
{
  resumption<Answer(Out)> r;

  struct Abort : command<> { };
  class HAbort : public flat_handler<void, Abort> {
    void handle_command(Abort, resumption<void()>) override { }
  };

  struct Arg : command<Out> { resumption<Answer(Out)>& res; };
  class HArg : public flat_handler<Answer, Arg> {
    Answer handle_command(Arg a, resumption<Answer(Out)> r) override
    { 
      a.res = std::move(r);
      invoke_command(Abort{});
    }
  };

  handle<HAbort>([&r, func](){
    handle<HArg>([&r, func](){
      return func(invoke_command(Arg{{}, r}));
    });
  });

  data = r.release();
}

template <typename Answer>
resumption<Answer()>::resumption(std::function<Answer()> func)
{
  resumption<Answer()> r;

  struct Abort : command<> { };
  class HAbort : public flat_handler<void, Abort> {
    void handle_command(Abort, resumption<void()>) override { }
  };

  struct Arg : command<> { resumption<Answer()>& res; };
  class HArg : public flat_handler<Answer, Arg> {
    Answer handle_command(Arg a, resumption<Answer()> r) override
    {
      a.res = std::move(r);
      invoke_command(Abort{});
    }
  };

  handle<HAbort>([&r, func](){
    handle<HArg>([&r, func](){
      invoke_command(Arg{{}, r});
      return func();
    });
  });

  data = r.release();
}

template <typename Out, typename Answer>
Answer resumption_data<Out, Answer>::resume()
{
  using namespace cpp_effects_internals;

  if constexpr (!std::is_void<Answer>::value) {
    std::optional<Answer> answer;
    void* prevBuffer = metastack.front()->return_buffer;
    metastack.front()->return_buffer = &answer;

    std::move(this->stored_metastack.front()->fiber).resume_with(
        [&](ctx::fiber&& prev) -> ctx::fiber {
      metastack.front()->fiber = std::move(prev);
      metastack.splice(metastack.begin(), this->stored_metastack);
      return ctx::fiber();
    });

    // Trampoline tail-resumes
    while (cpp_effects_internals::tail_resumption.has_value()) {
      resumption_base* temp = *cpp_effects_internals::tail_resumption;
      cpp_effects_internals::tail_resumption = {};
      temp->tail_resume();
    }

    metastack.front()->return_buffer = prevBuffer;
    return std::move(*answer);
  } else {
    std::move(this->stored_metastack.front()->fiber).resume_with(
        [&](ctx::fiber&& prev) -> ctx::fiber {
      metastack.front()->fiber = std::move(prev);
      metastack.splice(metastack.begin(), this->stored_metastack);
      return ctx::fiber();
    });

    // Trampoline tail-resumes
    while (cpp_effects_internals::tail_resumption.has_value()) {
      resumption_base* temp = *cpp_effects_internals::tail_resumption;
      cpp_effects_internals::tail_resumption = {};
      temp->tail_resume();
    }
  }
}

template <typename Out, typename Answer>
void resumption_data<Out, Answer>::tail_resume()
{
  using namespace cpp_effects_internals;

  std::move(this->stored_metastack.front()->fiber).resume_with(
      [&](ctx::fiber&& prev) -> ctx::fiber {
    metastack.front()->fiber = std::move(prev);
    metastack.splice(metastack.begin(), this->stored_metastack);
    return ctx::fiber();
  });
}

template <typename Out, typename Answer>
Answer resumption<Answer(Out)>::tail_resume(Out cmdResult) &&
{
  data->command_result_buffer->value = std::move(cmdResult);
  // Trampoline back to handle
  cpp_effects_internals::tail_resumption = release();
  if constexpr (!std::is_void<Answer>::value) {
    return Answer();
  }
}

template <typename Answer>
Answer resumption<Answer()>::tail_resume() &&
{
  // Trampoline back to handle
  cpp_effects_internals::tail_resumption = release();
  if constexpr (!std::is_void<Answer>::value) {
    return Answer();
  }
}

// --------------------------------
// API - implementation of handlers
// --------------------------------

// Handlers are metaframes that inherit a number of command
// clauses. Indeed, a handler is an object, which contains code (and
// data in the style of parametrised handlers), which is the thing
// pushed on the metastack.

template <typename Answer, typename Body, typename... Cmds>
class handler :
  public cpp_effects_internals::metaframe,
  public cpp_effects_internals::command_clause<Answer, Cmds>...
{
  template <typename H>
  friend typename H::answer_type handle_with(
      int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

  template <typename H, typename Cmd>
  friend typename Cmd::out_type static_invoke_command(int64_t goto_handler, const Cmd& cmd);

  template <typename H, typename Cmd>
  friend typename Cmd::out_type static_invoke_command(const Cmd& cmd);

public:
  using cpp_effects_internals::command_clause<Answer, Cmds>::handle_command...;
  using cpp_effects_internals::command_clause<Answer, Cmds>::invoke_command...;
public:
  using answer_type = Answer;
  using body_type = Body;
  virtual void debug_print() const override
  {
    std::cout << cpp_effects_internals::metaframe::label << ":" << typeid(*this).name();
    ((std::cout << "[" << typeid(Cmds).name() << "]"), ...);
    std::cout << std::endl;
  }
protected:
  virtual Answer handle_return(Body b) = 0;
private:
  Answer run_return(cpp_effects_internals::tangible<Body> b)
  {
    return handle_return(std::move(b.value));
  }
};

// We specialise for Body = void

template <typename Answer, typename... Cmds>
class handler<Answer, void, Cmds...> :
  public cpp_effects_internals::metaframe,
  public cpp_effects_internals::command_clause<Answer, Cmds>...
{
  template <typename H> friend
  typename H::answer_type handle_with(
      int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

  template <typename H, typename Cmd>
  friend typename Cmd::out_type static_invoke_command(int64_t goto_handler, const Cmd& cmd);

  template <typename H, typename Cmd>
  friend typename Cmd::out_type static_invoke_command(const Cmd& cmd);

public:
  using cpp_effects_internals::command_clause<Answer, Cmds>::handle_command...;
  using cpp_effects_internals::command_clause<Answer, Cmds>::invoke_command...;
public:
  using answer_type = Answer;
  using body_type = void;
  virtual void debug_print() const override
  {
    std::cout << cpp_effects_internals::metaframe::label << ":" << typeid(*this).name();
    ((std::cout << "[" << typeid(Cmds).name() << "]"), ...);
    std::cout << std::endl;
  }
protected:
  virtual Answer handle_return() = 0;
private:
  Answer run_return(cpp_effects_internals::tangible<void>) { return handle_return(); }
};

// A handler without the return clause

template <typename Answer, typename... Cmds>
class flat_handler : public handler<Answer, Answer, Cmds...> {
  Answer handle_return(Answer a) final override { return a; }
};

template <typename... Cmds>
class flat_handler<void, Cmds...> : public handler<void, void, Cmds...> {
  void handle_return() final override { }
};

// ---------------------------------
// API - implementation of functions
// ---------------------------------

// Misc

int64_t fresh_label()
{
  static int64_t freshCounter = -1;
  return --freshCounter;
}

void debug_print_metastack()
{
  using namespace cpp_effects_internals;

  for (auto frame : metastack) { frame->debug_print(); }
}

// Handling

template <typename H, typename... Args>
typename H::answer_type handle(
    int64_t label, std::function<typename H::body_type()> body, Args&&... args)
{
  if constexpr (!std::is_void<typename H::answer_type>::value) {
    return handle_with(label, body, std::make_shared<H>(std::forward<Args>(args)...));
  } else {
    handle_with(label, body, std::make_shared<H>(std::forward<Args>(args)...));
  }
}

template <typename H, typename... Args>
typename H::answer_type handle(std::function<typename H::body_type()> body, Args&&... args)
{
  if constexpr (!std::is_void<typename H::answer_type>::value) {
    return handle<H>(fresh_label(), body, std::forward<Args>(args)...);
  } else {
    handle<H>(fresh_label(), body, std::forward<Args>(args)...);
  }
}

template <typename H>
typename H::answer_type handle_with(
    int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler)
{
  using namespace cpp_effects_internals;
  using Answer = typename H::answer_type;
  using Body = typename H::body_type;

  // E.g. for different stack use policy
  // ctx::protected_fixedsize_stack pf(10000000);
  ctx::fiber bodyFiber{/*std::alocator_arg, std::move(pf),*/
      [&](ctx::fiber&& prev) -> ctx::fiber&& {
    metastack.front()->fiber = std::move(prev);
    handler->label = label;
    metastack.push_front(handler);

    cpp_effects_internals::tangible<Body> b(body);

    cpp_effects_internals::metaframe_ptr returnFrame(std::move(metastack.front()));
    metastack.pop_front();

    std::move(metastack.front()->fiber).resume_with([&](ctx::fiber&&) -> ctx::fiber {
      if constexpr (!std::is_void<Answer>::value) {
        *(static_cast<std::optional<Answer>*>(metastack.front()->return_buffer)) =
          std::static_pointer_cast<H>(returnFrame)->run_return(std::move(b));
      } else {
        std::static_pointer_cast<H>(returnFrame)->run_return(std::move(b));
      }
      return ctx::fiber();
    });
      
    // Unreachable: this fiber is now destroyed
    std::cerr << "error: impssible!\n";
    exit(-1);
  }};

  if constexpr (!std::is_void<Answer>::value) {
    std::optional<Answer> answer;
    void* prevBuffer = metastack.front()->return_buffer;
    metastack.front()->return_buffer = &answer;
    std::move(bodyFiber).resume();

    // Trampoline tail-resumes
    while (cpp_effects_internals::tail_resumption.has_value()) {
      resumption_base* temp = *cpp_effects_internals::tail_resumption;
      cpp_effects_internals::tail_resumption = {};
      temp->tail_resume();
    }

    metastack.front()->return_buffer = prevBuffer;
    return std::move(*answer);
  } else {
    std::move(bodyFiber).resume();

    // Trampoline tail-resumes
    while (cpp_effects_internals::tail_resumption.has_value()) {
      resumption_base* temp = *cpp_effects_internals::tail_resumption;
      cpp_effects_internals::tail_resumption = {};
      temp->tail_resume();
    }
  }
}

template <typename H>
typename H::answer_type handle_with(
    std::function<typename H::body_type()> body, std::shared_ptr<H> handler)
{
  if constexpr (!std::is_void<typename H::answer_type>::value) {
    return handle_with(fresh_label(), body, std::move(handler));
  } else {
    handle_with(fresh_label(), body, handler);
  }
}

template <typename H, typename... Args>
typename H::answer_type handle_ref(
    int64_t label, std::function<typename H::body_type(handler_ref)> body, Args&&... args)
{
  if constexpr (!std::is_void<typename H::answer_type>::value) {
    return handle_with_ref(label, body, std::make_shared<H>(std::forward<Args>(args)...));
  } else {
    handle_with_ref(label, body, std::make_shared<H>(std::forward<Args>(args)...));
  }
}

template <typename H, typename... Args>
typename H::answer_type handle_ref(
    std::function<typename H::body_type(handler_ref)> body, Args&&... args)
{
  if constexpr (!std::is_void<typename H::answer_type>::value) {
    return handle_ref<H>(fresh_label(), body, std::forward<Args>(args)...);
  } else {
    handle_ref<H>(fresh_label(), body, std::forward<Args>(args)...);
  }
}

template <typename H>
typename H::answer_type handle_with_ref(
    int64_t label,
    std::function<typename H::body_type(handler_ref)> body,
    std::shared_ptr<H> handler)
{
  return handle_with(label, [&](){
    auto href = find_handler(label);
    return body(href);
  }, std::move(handler));
}

template <typename H>
typename H::answer_type handle_with_ref(
    std::function<typename H::body_type(handler_ref)> body, std::shared_ptr<H> handler)
{
  if constexpr (!std::is_void<typename H::answer_type>::value) {
    return handle_with_ref(fresh_label(), body, std::move(handler));
  } else {
    handle_with_ref(fresh_label(), body, handler);
  }
}

// Lifting a function to a resumption by wrapping it in a handler

template <typename H, typename... Args>
resumption<typename H::answer_type()> wrap(
    int64_t label, std::function<typename H::body_type()> body, Args&&... args)
{
  return resumption<typename H::answer_type()>([=](){
    return handle<H>(label, body, std::forward<Args>(args)...);
  });
}

template <typename H, typename A, typename... Args>
resumption<typename H::answer_type()> wrap(
    int64_t label, std::function<typename H::body_type(A)> body, Args&&... args)
{
  return resumption<typename H::answer_type()>([=](A a){
    return handle<H>(label, std::bind(body, a), std::forward<Args>(args)...);
  });
}

template <typename H, typename... Args>
resumption<typename H::answer_type()> wrap(
    std::function<typename H::body_type()> body, Args&&... args)
{
  return resumption<typename H::answer_type()>([=](){
    return handle<H>(body, std::forward<Args>(args)...);
  });
}

template <typename H, typename A, typename... Args>
resumption<typename H::answer_type()> wrap(
    std::function<typename H::body_type(A)> body, Args&&... args)
{
  return resumption<typename H::answer_type()>([=](A a){
    return handle<H>(std::bind(body, a), std::forward<Args>(args)...);
  });
}

template <typename H>
resumption<typename H::answer_type()> wrap_with(
    int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler)
{
  return resumption<typename H::answer_type()>([=](){
    return handle_with<H>(label, body, handler);
  });
}

template <typename H, typename A>
resumption<typename H::answer_type()> wrap_with(
    int64_t label, std::function<typename H::body_type(A)> body, std::shared_ptr<H> handler)
{
  return resumption<typename H::answer_type()>([=](A a){
    return handle_with<H>(label, std::bind(body, a), handler);
  });
}

template <typename H>
resumption<typename H::answer_type()> wrap_with(
    std::function<typename H::body_type()> body, std::shared_ptr<H> handler)
{
  return resumption<typename H::answer_type()>([=](){
    return handle_with<H>(body, handler);
  });
}

template <typename H, typename A>
resumption<typename H::answer_type()> wrap_with(
    std::function<typename H::body_type(A)> body, std::shared_ptr<H> handler)
{
  return resumption<typename H::answer_type()>([=](A a){
    return handle_with<H>(std::bind(body, a), handler);
  });
}

// Invoking commands

// In the invoke_command... methods we rely on the virtual method of the
// metaframe, as at this point we cannot know what answer_type and
// body_type are.
//
// E.g. looking for d in [a][b][c][d][e][f][g.]
// ===>
// Run d.cmd in [a][b][c.] with r.stack = [d][e][f][g],
// where [_.] denotes a frame with invalid (i.e. current) fiber

template <typename Cmd>
typename Cmd::out_type invoke_command(int64_t goto_handler, const Cmd& cmd)
{
  using namespace cpp_effects_internals;

  // Looking for handler based on its label
  auto cond = [&](const cpp_effects_internals::metaframe_ptr& mf) {
    return mf->label == goto_handler;
  };
  auto it = std::find_if(metastack.begin(), metastack.end(), cond);
  if (auto canInvoke = std::dynamic_pointer_cast<can_invoke_command<Cmd>>(*it)) {
    return canInvoke->invoke_command(std::next(it), cmd);
  }
  std::cerr << "error: handler with id " << goto_handler
            << " does not handle " << typeid(Cmd).name() << std::endl;
  debug_print_metastack();
  exit(-1);
}

template <typename Cmd>
typename Cmd::out_type invoke_command(const Cmd& cmd)
{
  using namespace cpp_effects_internals;

  // Looking for handler based on the type of the command
  for (auto it = metastack.begin(); it != metastack.end(); ++it) {
    if (auto canInvoke = std::dynamic_pointer_cast<can_invoke_command<Cmd>>(*it)) {
      return canInvoke->invoke_command(std::next(it), cmd);
    }
  }
  debug_print_metastack();
  exit(-1);
}

template <typename Cmd>
typename Cmd::out_type invoke_command(handler_ref it, const Cmd& cmd)
{
  using namespace cpp_effects_internals;

  if (auto canInvoke = std::dynamic_pointer_cast<can_invoke_command<Cmd>>(*it)) {
    return canInvoke->invoke_command(std::next(it), cmd);
  }
  std::cerr << "error: selected handler does not handle " << typeid(Cmd).name() << std::endl;
  debug_print_metastack();
  exit(-1);
}

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(int64_t goto_handler, const Cmd& cmd)
{
  using namespace cpp_effects_internals;

  auto cond = [&](const cpp_effects_internals::metaframe_ptr& mf) {
    return mf->label == goto_handler;
  };
  auto it = std::find_if(metastack.begin(), metastack.end(), cond);
  if (it != metastack.end()) {
    return (static_cast<H*>(it->get()))->H::invoke_command(std::next(it), cmd);
  }
  std::cerr << "error: handler with id " << goto_handler
            << " does not handle " << typeid(Cmd).name() << std::endl;
  debug_print_metastack();
  exit(-1);
}

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(const Cmd& cmd)
{
  using namespace cpp_effects_internals;

  auto it = metastack.begin();
  return (static_cast<H*>(it->get()))->H::invoke_command(std::next(it), cmd);
}

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(handler_ref it, const Cmd& cmd)
{
  return (static_cast<H*>(it->get()))->H::invoke_command(std::next(it), cmd);
}

// Find a reference to a handler on the metastack

template <typename Cmd>
handler_ref find_handler()
{
  using namespace cpp_effects_internals;

  // Looking for handler based on the type of the command
  for (auto it = metastack.begin(); it != metastack.end(); ++it) {
    if (std::dynamic_pointer_cast<can_invoke_command<Cmd>>(*it)) {
      return it;
    }
  }

  std::cerr << "error: cpp_effects::find_handler did not find a handler" << std::endl;
  debug_print_metastack();
  exit(-1);
}

handler_ref find_handler(int64_t goto_handler)
{
  using namespace cpp_effects_internals;

  auto const cond = [&](const cpp_effects_internals::metaframe_ptr& mf) {
    return mf->label == goto_handler;
  };
  auto it = std::find_if(metastack.begin(), metastack.end(), cond);
  if (it != metastack.end()) { return it; }

  std::cerr << "error: cpp_effects::find_handler did not find a handler" << std::endl;
  debug_print_metastack();
  exit(-1);
}

} // namespace cpp_effects

#endif // CPP_EFFECTS_CPP_EFFECTS_H
