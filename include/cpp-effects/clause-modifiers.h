// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Modifiers that force specific shapes of command clauses in
// handlers, which is useful for readability (e.g., we can read from
// the type that we will not use the resumption) and performance
// (e.g., there is no need to allocate the resumption).
//
// Each clause consists of a template that modifies the type of the
// clause in the definition of a handler. For example, we can specify
// that a particular handler will not need the resumption:
//
// struct MyCmd : Command<int> { };
// struct OtherCmd : Command<void> { };
// class MyHandler : public Handler <char, void, NoResume<MyCmd>>, OtherCmd> {
//   char CommandClause(MyCmd) override { ... }
//};
//
// Note that because we used the NoResume modifier, the type of
// CommandClause for MyCmd is now different.

#ifndef CPP_EFFECTS_SPECIAL_CLAUSES_H
#define CPP_EFFECTS_SPECIAL_CLAUSES_H

#include "cpp-effects/cpp-effects.h"

namespace CppEffects {

// -----
// Plain
// -----

// Specialisation for plain clauses, which interpret commands as
// functions (i.e., they are self- and tail-resumptive). No context
// switching, no allocation of resumption.

template <typename Cmd>
struct Plain { };

template <typename Answer, typename Cmd>
class CmdClause<Answer, Plain<Cmd>> : public CanInvokeCmdClause<Cmd> {
  friend class OneShot;
protected:
  virtual typename Cmd::OutType CommandClause(Cmd) = 0;
private:
  virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframeBase*>::reverse_iterator it, const Cmd& cmd) override
  {
    // (continued from OneShot::InvokeCmd) ...looking for [d]
    const auto jt = it.base();
    std::list<MetaframeBase*> storedMetastack;
    storedMetastack.splice(
      storedMetastack.begin(), OneShot::Metastack(), jt, OneShot::Metastack().end());
    // at this point: metastack = [a][b][c]; stored stack = [d][e][f][g.]
    std::swap(storedMetastack.back()->fiber, OneShot::Metastack().back()->fiber);
    // at this point: metastack = [a][b][c.]; stored stack = [d][e][f][g]

    if constexpr (!std::is_void<typename Cmd::OutType>::value) {
      typename Cmd::OutType a(CommandClause(cmd));
      std::swap(storedMetastack.back()->fiber, OneShot::Metastack().back()->fiber);
      // at this point: metastack = [a][b][c]; stored stack = [d][e][f][g.]
      OneShot::Metastack().splice(OneShot::Metastack().end(), storedMetastack);
      // at this point: metastack = [a][b][c][d][e][f][g.]
      return std::move(a);
    } else {
      CommandClause(cmd);
      std::swap(storedMetastack.back()->fiber, OneShot::Metastack().back()->fiber);
      OneShot::Metastack().splice(OneShot::Metastack().end(), storedMetastack);
    }
  }
};

// --------
// NoResume
// --------

// Specialisation for command clauses that do not use the
// resumption. This is useful for handlers that behave like exception
// handlers or terminate the "current thread".

template <typename Cmd>
struct NoResume { };

template <typename Answer, typename Cmd>
class CmdClause<Answer, NoResume<Cmd>> : public CanInvokeCmdClause<Cmd> {
  friend class OneShot;
protected:
  virtual Answer CommandClause(Cmd) = 0;
private:
  [[noreturn]] virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframeBase*>::reverse_iterator it, const Cmd& cmd) override
  {
    // (continued from OneShot::InvokeCmd) ...looking for [d]
    auto jt = it.base();
    OneShot::Metastack().erase(jt, OneShot::Metastack().end());
    // at this point: metastack = [a][b][c]
  
    std::move(OneShot::Metastack().back()->fiber).resume_with([&](ctx::fiber&& /*prev*/) -> ctx::fiber {
      if constexpr (!std::is_void<Answer>::value) {
        OneShot::transferBuffer = new Transfer<Answer>(this->CommandClause(cmd));
      } else {
        this->CommandClause(cmd);
      }
      return ctx::fiber();
    });

    // The current fiber is gone (because prev in the above goes out
    // of scope and is deleted), so this will never be reached.
    std::cerr << "Malformed NoResume handler" << std::endl;
    OneShot::DebugPrintMetastack();
    exit(-1);
  }
};

} // namespace CppEffects

#endif // CPP_EFFECTS_SPECIAL_CLAUSES_H
