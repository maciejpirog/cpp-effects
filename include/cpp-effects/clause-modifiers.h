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
// struct MyCmd : Command<int> { };
// struct OtherCmd : Command<void> { };
// class MyHandler : public Handler <char, void, NoResume<MyCmd>>, OtherCmd> {
//   char CommandClause(MyCmd) override { ... }
// };
//
// Note that because we used the NoResume modifier, the type of
// CommandClause for MyCmd is now different.

#ifndef CPP_EFFECTS_CLAUSE_MODIFIERS_H
#define CPP_EFFECTS_CLAUSE_MODIFIERS_H

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
    std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd) final override
  {
    // (continued from OneShot::InvokeCmd) ...looking for [d]
    const auto jt = it.base();
    std::list<MetaframePtr> storedMetastack;
    storedMetastack.splice(
      storedMetastack.begin(), OneShot::Metastack, jt, OneShot::Metastack.end());
    // at this point: metastack = [a][b][c]; stored stack = [d][e][f][g.]
    std::swap(storedMetastack.back()->fiber, OneShot::Metastack.back()->fiber);
    // at this point: metastack = [a][b][c.]; stored stack = [d][e][f][g]

    if constexpr (!std::is_void<typename Cmd::OutType>::value) {
      typename Cmd::OutType a(CommandClause(cmd));
      std::swap(storedMetastack.back()->fiber, OneShot::Metastack.back()->fiber);
      // at this point: metastack = [a][b][c]; stored stack = [d][e][f][g.]
      OneShot::Metastack.splice(OneShot::Metastack.end(), storedMetastack);
      // at this point: metastack = [a][b][c][d][e][f][g.]
      return std::move(a);
    } else {
      CommandClause(cmd);
      std::swap(storedMetastack.back()->fiber, OneShot::Metastack.back()->fiber);
      OneShot::Metastack.splice(OneShot::Metastack.end(), storedMetastack);
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
    std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd) final override
  {
    // (continued from OneShot::InvokeCmd) ...looking for [d]
    auto jt = it.base();
    OneShot::Metastack.erase(jt, OneShot::Metastack.end());
    // at this point: metastack = [a][b][c]

    std::move(OneShot::Metastack.back()->fiber).resume_with([&](ctx::fiber&& /*prev*/) -> ctx::fiber {
      if constexpr (!std::is_void<Answer>::value) {
        *(static_cast<std::optional<Answer>*>(OneShot::Metastack.back()->returnBuffer)) =
          this->CommandClause(cmd);
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

// --------
// NoManage
// --------

// Specialisation for handlers that either:
//
// - Don't expose the resumption (i.e., all resumes happen within
//   command clauses),
// - Don't access the handler object after resume,
//
// which amounts to almost all practical uses of handlers. "NoManage"
// clause does not participate in the reference counting of handlers,
// saving a tiny bit of performance. The interface is exactly as in a
// regular command clause.
//
// Plain and NoResume clauses are automaticslly NoManage.

template <typename Cmd>
struct NoManage { };

template <typename Answer, typename Cmd>
class CmdClause<Answer, NoManage<Cmd>> : public CanInvokeCmdClause<Cmd> {
  friend class OneShot;
protected:
  virtual Answer CommandClause(Cmd, Resumption<typename Cmd::OutType, Answer>) = 0;
private:
  virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd) final override
  {
    using Out = typename Cmd::OutType;

    // (continued from OneShot::InvokeCmd) ...looking for [d]
    auto jt = it.base();
    ResumptionData<Out, Answer>& resumption = this->resumptionBuffer;
    resumption.storedMetastack.splice(
      resumption.storedMetastack.begin(), OneShot::Metastack, jt, OneShot::Metastack.end());
    // at this point: [a][b][c]; stored stack = [d][e][f][g.] 

    std::move(OneShot::Metastack.back()->fiber).resume_with([&](ctx::fiber&& prev) ->
        ctx::fiber {
      // at this point: [a][b][c.]; stored stack = [d][e][f][g.]
      resumption.storedMetastack.back()->fiber = std::move(prev);
      // at this point: [a][b][c.]; stored stack = [d][e][f][g]

      // We don't need to keep the handler alive for the duration of the command clause call
      // (compare CmdClause<Answer, Cmd>::InvokeCmd)

      if constexpr (!std::is_void<Answer>::value) {
        *(static_cast<std::optional<Answer>*>(OneShot::Metastack.back()->returnBuffer)) =
          this->CommandClause(cmd, Resumption<Out, Answer>(resumption));
      } else {
        this->CommandClause(cmd, Resumption<Out, Answer>(resumption));
      }
      return ctx::fiber();
    });

    // If the control reaches here, this means that the resumption is
    // being resumed at the moment, and so we no longer need the
    // resumption object.
    if constexpr (!std::is_void<Out>::value) {
      Out cmdResult = std::move(resumption.cmdResultTransfer->value);
      resumption.storedMetastack.clear();
      resumption.cmdResultTransfer = {};
      return cmdResult;
    } else {
      resumption.storedMetastack.clear();
    }
  }
  ResumptionData<typename Cmd::OutType, Answer> resumptionBuffer;
};

} // namespace CppEffects

#endif // CPP_EFFECTS_CLAUSE_MODIFIERS_H
