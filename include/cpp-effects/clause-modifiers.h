// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// This file contains modifiers that force specific shapes of command
// clauses in handlers, which is useful for readability (e.g., we can
// read from the type that we will not use the resumption) and
// performance (e.g., there is no need to allocate the resumption).
//
// Each modifier consists of a template, which marks a command in the
// type of the handler, and a specialisation of the CommandClause
// class that modifies the type of the clause in the definition of the
// handler. For example, we can specify that a particular handler will
// not need the resumption:
//
// struct MyCmd : command<int> { };
// struct OtherCmd : command<> { };
// class MyHandler : public handler<char, void, no_resume<MyCmd>>, OtherCmd> {
//   char handle_command(MyCmd) override { ... }
// };
//
// Note that because we used the no_resume modifier, the type of
// handle_command for MyCmd is now different.

#ifndef CPP_EFFECTS_CLAUSE_MODIFIERS_H
#define CPP_EFFECTS_CLAUSE_MODIFIERS_H

#include "cpp-effects/cpp-effects.h"

namespace cpp_effects {

// -----
// plain
// -----

// Specialisation for plain clauses, which interpret commands as
// functions (i.e., they are self- and tail-resumptive). No context
// switching, no allocation of resumption.

template <typename Cmd>
struct plain { };

namespace cpp_effects_internals {

template <typename Answer, typename Cmd>
class command_clause<Answer, plain<Cmd>> : public can_invoke_command<Cmd> {
  template <typename, typename, typename...> friend class ::cpp_effects::handler;
  template <typename, typename...> friend class ::cpp_effects::flat_handler;
protected:
  virtual typename Cmd::out_type handle_command(Cmd) = 0;
public:
  virtual typename Cmd::out_type invoke_command(
    std::list<metaframe_ptr>::iterator it, const Cmd& cmd) final override
  {
    // (continued from OneShot::InvokeCmd) ...looking for [d]
    std::list<metaframe_ptr> stored_metastack;
    stored_metastack.splice(
      stored_metastack.begin(), metastack, metastack.begin(), it);
    // at this point: metastack = [a][b][c]; stored stack = [d][e][f][g.]
    std::swap(stored_metastack.front()->fiber, metastack.front()->fiber);
    // at this point: metastack = [a][b][c.]; stored stack = [d][e][f][g]

    if constexpr (!std::is_void<typename Cmd::out_type>::value) {
      typename Cmd::out_type a(handle_command(cmd));
      std::swap(stored_metastack.front()->fiber, metastack.front()->fiber);
      // at this point: metastack = [a][b][c]; stored stack = [d][e][f][g.]
      metastack.splice(metastack.begin(), stored_metastack);
      // at this point: metastack = [a][b][c][d][e][f][g.]
      return a;
    } else {
      handle_command(cmd);
      std::swap(stored_metastack.front()->fiber, metastack.front()->fiber);
      metastack.splice(metastack.begin(), stored_metastack);
    }
  }
};

} // namespace cpp_effects_internals

// ---------
// no_resume
// ---------

// Specialisation for command clauses that do not use the
// resumption. This is useful for handlers that behave like exception
// handlers or terminate the "current thread".

template <typename Cmd>
struct no_resume { };

namespace cpp_effects_internals {

template <typename Answer, typename Cmd>
class command_clause<Answer, no_resume<Cmd>> : public can_invoke_command<Cmd> {
  template <typename, typename, typename...> friend class ::cpp_effects::handler;
  template <typename, typename...> friend class ::cpp_effects::flat_handler;
protected:
  virtual Answer handle_command(Cmd) = 0;
public:
  [[noreturn]] virtual typename Cmd::out_type invoke_command(
    std::list<metaframe_ptr>::iterator it, const Cmd& cmd) final override
  {
    // (continued from OneShot::InvokeCmd) ...looking for [d]
    metastack.erase(metastack.begin(), it);
    // at this point: metastack = [a][b][c]

    std::move(metastack.front()->fiber).resume_with([&](ctx::fiber&& /*prev*/) -> ctx::fiber {
      if constexpr (!std::is_void<Answer>::value) {
        *(static_cast<std::optional<Answer>*>(metastack.front()->return_buffer)) =
          this->handle_command(cmd);
      } else {
        this->handle_command(cmd);
      }
      return ctx::fiber();
    });

    // The current fiber is gone (because prev in the above goes out
    // of scope and is deleted), so this will never be reached.
    std::cerr << "Malformed no_resume handler" << std::endl;
    debug_print_metastack();
    exit(-1);
  }
};

} // namespace cpp_effects_internals

// ---------
// no_manage
// ---------

// Specialisation for handlers that either:
//
// - Don't expose the resumption (i.e., all resumes happen within
//   command clauses),
// - Don't access the handler object after resume,
//
// which amounts to almost all practical uses of handlers. "no_manage"
// clause does not participate in the reference counting of handlers,
// saving a tiny bit of performance. The interface is exactly as in a
// regular command clause.
//
// Plain and no_resume clauses are automaticslly no_manage.

template <typename Cmd>
struct no_manage { };

namespace cpp_effects_internals {

template <typename Answer, typename Cmd>
class command_clause<Answer, no_manage<Cmd>> : public can_invoke_command<Cmd> {
  template <typename, typename, typename...> friend class ::cpp_effects::handler;
  template <typename, typename...> friend class ::cpp_effects::flat_handler;
protected:
  virtual Answer handle_command(Cmd, ::cpp_effects::resumption<typename Cmd::template resumption_type<Answer>>) = 0;
public:
  virtual typename Cmd::out_type invoke_command(
    std::list<metaframe_ptr>::iterator it, const Cmd& cmd) final override
  {
    using Out = typename Cmd::out_type;

    // (continued from OneShot::InvokeCmd) ...looking for [d]
    resumption_data<Out, Answer>& resumption = this->resumptionBuffer;
    resumption.stored_metastack.splice(
      resumption.stored_metastack.begin(), metastack, metastack.begin(), it);
    // at this point: [a][b][c]; stored stack = [d][e][f][g.] 

    std::move(metastack.front()->fiber).resume_with([&](ctx::fiber&& prev) ->
        ctx::fiber {
      // at this point: [a][b][c.]; stored stack = [d][e][f][g.]
      resumption.stored_metastack.front()->fiber = std::move(prev);
      // at this point: [a][b][c.]; stored stack = [d][e][f][g]

      // We don't need to keep the handler alive for the duration of the command clause call
      // (compare command_clause<Answer, Cmd>::InvokeCmd)

      if constexpr (!std::is_void<Answer>::value) {
        *(static_cast<std::optional<Answer>*>(metastack.front()->return_buffer)) =
          this->handle_command(cmd, ::cpp_effects::resumption<typename Cmd::template resumption_type<Answer>>(resumption));
      } else {
        this->handle_command(cmd, ::cpp_effects::resumption<typename Cmd::template resumption_type<Answer>>(resumption));
      }
      return ctx::fiber();
    });

    // If the control reaches here, this means that the resumption is
    // being resumed at the moment, and so we no longer need the
    // resumption object.
    if constexpr (!std::is_void<Out>::value) {
      Out cmdResult = std::move(resumption.command_result_buffer->value);
      resumption.stored_metastack.clear();
      resumption.command_result_buffer = {};
      return cmdResult;
    } else {
      resumption.stored_metastack.clear();
    }
  }
  resumption_data<typename Cmd::out_type, Answer> resumptionBuffer;
};

} // namespace cpp_effects_internals

} // namespace CppEffects

#endif // CPP_EFFECTS_CLAUSE_MODIFIERS_H
