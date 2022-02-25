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

// Different stack use policies:
// #include <boost/context/protected_fixedsize_stack.hpp>
// #include <boost_context_fixedsize_stack>

#include <optional>
#include <functional>
#include <iostream> // for debugging only
#include <list>
#include <vector>
#include <tuple>
#include <typeinfo>
#include <typeindex>

namespace CppEffects {

namespace ctx = boost::context;

// -----------------------------------------------
// Auxiliary class to deal with voids in templates
// -----------------------------------------------

template <typename T>
struct Tangible {
  T value;
  Tangible() = delete;
  Tangible(T&& t) : value(std::move(t)) { }
  Tangible(const std::function<T()>& f) : value(f()) { }
};

template <>
struct Tangible<void> {
  Tangible() = default;
  Tangible(const std::function<void()>& f) { f(); }
};

// ------------------------------
// Mutually recursive definitions
// ------------------------------

class OneShot;
class Metaframe;
class InitMetastack;

using MetaframePtr = std::shared_ptr<Metaframe>;

// --------
// Commands
// --------

template <typename Out>
struct Command {
  using OutType = Out;
};

// -----------
// Resumptions
// -----------

class ResumptionBase {
  friend class OneShot;
public:
  virtual ~ResumptionBase() { }
protected:
  virtual void TailResume() = 0;
};

template <typename Out, typename Answer>
class Resumption : public ResumptionBase {
  friend class OneShot;
  template <typename, typename> friend class CmdClause;
protected:
  Resumption() { }
  std::list<MetaframePtr> storedMetastack;
  std::optional<Tangible<Out>> cmdResultTransfer; // Used to transfer data between fibers
  virtual Answer Resume();
  virtual void TailResume() override;
};

template <typename Out, typename Answer>
class PlainResumption : public Resumption<Out, Answer> {
  friend class OneShot;
  const std::function<Answer(Out)> func;
  PlainResumption(const std::function<Answer(Out)>& func) : func(func) { }
  virtual Answer Resume() override;
  virtual void TailResume() override;
};

template <typename Answer>
class PlainResumption<void, Answer> : public Resumption<void, Answer> {
  friend class OneShot;
  const std::function<Answer()> func;
  PlainResumption(const std::function<Answer()>& func) : func(func) { }
  virtual Answer Resume() override;
  virtual void TailResume() override;
};

// ----------
// Metaframes
// ----------

// Labels:
// 0   -- the initial fiber with no handler (the static in OneShot::Metastack())
// -1  -- invalid handler (used to shadow a handler in the return clause)
// >0  -- user-defined labels
// <0  -- auto-generated labels

class Metaframe {
  friend class OneShot;
  template <typename, typename> friend class CmdClause;
  friend class InitMetastack;
  template <typename, typename> friend class Resumption; 
public:
  virtual ~Metaframe() { }
  virtual void DebugPrint() const
  {
    std::cout << "[" << label << ":" << typeid(*this).name();
    if (!fiber) { std::cout << " (active)"; }
    std::cout << "]";
  }
protected:
  Metaframe() : label(0) { }
  int64_t label;
private:
  ctx::fiber fiber;
};

// When invoking a command in the client code, we know the type of the
// command, but we cannot know the type of the handler. In particular,
// we cannot know the answer type, hence we cannot simply up-cast the
// found handler to the class Handler<...>. Instead, Handler inherits
// (indirectly) from the class CanInvokeCmdClause, which allows us to
// call appropriate command clause of the handler without knowing the
// answer type.

template <typename Cmd>
class CanInvokeCmdClause {
  friend class OneShot;
protected:
  virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd) = 0;
};

// CmdClause is a class that allows us to define a handler with a
// command clause for a particular operation. It inherits from
// CanInvokeCmd, and overrides InvokeCmd, which means that the user,
// who cannot know the answer type of a handler, can call the command
// clause of the handler anyway, by up-casting to CanInvokeCmdClause.

template <typename Answer, typename Cmd>
class CmdClause : public CanInvokeCmdClause<Cmd> {
  friend class OneShot;
protected:
  virtual Answer CommandClause(Cmd, std::unique_ptr<Resumption<typename Cmd::OutType, Answer>>) = 0;
private:
  virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd) override;
};

// --------
// Handlers
// --------

// Handlers are metaframes that inherit a number of command
// clauses. Indeed, a handler is an object, which contains code (and
// data in the style of parametrised handlers), which is *the* thing
// pushed on the metastack.

template <typename Answer, typename Body, typename... Cmds>
class Handler : public Metaframe, public CmdClause<Answer, Cmds>... {
  friend class OneShot;
  using CmdClause<Answer, Cmds>::CommandClause...;
public:
  using AnswerType = Answer;
  using BodyType = Body;
protected:
  virtual Answer ReturnClause(Body b) = 0;
private:
  Answer RunReturnClause(Tangible<Body> b) { return ReturnClause(std::move(b.value)); }
};

// We specialise for Body = void

template <typename Answer, typename... Cmds>
class Handler<Answer, void, Cmds...> : public Metaframe, public CmdClause<Answer, Cmds>... {
  friend class OneShot;
  using CmdClause<Answer, Cmds>::CommandClause...;
public:
  using AnswerType = Answer;
  using BodyType = void;
protected:
  virtual Answer ReturnClause() = 0;
private:
  Answer RunReturnClause(Tangible<void>) { return ReturnClause(); }
};

// ------------
// Tail resumes
// ------------

// As there is no real forced TCO in C++, we need a separate mechanism
// for tail-resumptive handlers that will not build up call frames.

struct TailAnswer {
  ResumptionBase* resumption;
};

// ----------------------
// Programmer's interface
// ----------------------

class OneShot {
  template <typename, typename, typename...> friend class Handler;
  template <typename, typename> friend class Resumption;
  friend class InitMetastack;

public:
  static std::list<MetaframePtr>& Metastack()
  {
     static auto initMetaframe = new Metaframe();
     static std::list<MetaframePtr> metastack{std::shared_ptr<Metaframe>(initMetaframe)};
     return metastack;
  };
  static void* answerPtr;
  static std::optional<TailAnswer> tailAnswer;
  static int64_t freshCounter;

  OneShot() = delete;

  static void DebugPrintMetastack()
  {
    std::cerr << "metastack: ";
    for (auto x : Metastack()) { x->DebugPrint(); }
    std::cerr << std::endl;
  }

  template <typename H>
  static typename H::AnswerType Handle(int64_t label, std::function<typename H::BodyType()> body)
  {
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return HandleWith(label, body, std::make_shared<H>());
    } else {
      HandleWith(label, body, std::make_shared<H>());
    }
  }

  template <typename H>
  static typename H::AnswerType Handle(std::function<typename H::BodyType()> body)
  {
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return Handle<H>(OneShot::FreshLabel(), body);
    } else {
      Handle<H>(OneShot::FreshLabel(), body);
    }
  }

  template <typename H>
  static typename H::AnswerType HandleWith(
    int64_t label, std::function<typename H::BodyType()> body, std::shared_ptr<H> handler)
  {
    using Answer = typename H::AnswerType;
    using Body = typename H::BodyType;

    // E.g. for different stack use policy
    // ctx::protected_fixedsize_stack pf(10000000);
    ctx::fiber bodyFiber{/*std::alocator_arg, std::move(pf),*/
        [&](ctx::fiber&& prev) -> ctx::fiber&& {
      Metastack().back()->fiber = std::move(prev);
      handler->label = label;
      Metastack().push_back(handler);
      Tangible<Body> b(body);
      // The rest of the fiber might live in a resumption, after
      // the caller is long gone, hence we cannot use arguments
      // of HandleWith by reference from now on. The return
      // clause is executed in the same fiber, but it cannot use
      // commands of the handler.
      Metastack().back()->label = -1;
      if constexpr (!std::is_void<Answer>::value) {
        *(static_cast<std::optional<Answer>*>(answerPtr)) = 
          std::static_pointer_cast<H>(Metastack().back())->RunReturnClause(std::move(b));
      } else {
        std::static_pointer_cast<H>(Metastack().back())->RunReturnClause(std::move(b));
      }
      Metastack().pop_back();
      std::move(Metastack().back()->fiber).resume();
      
      // This will never be reached, because this fiber will have been destroyed.
      std::cerr << "error: malformed handler\n";
      exit(-1);
    }};
    
    if constexpr (!std::is_void<Answer>::value) {
      std::optional<Answer> answer;
      void* oldPtr = OneShot::answerPtr;
      OneShot::answerPtr = &answer;

      std::move(bodyFiber).resume();

      // Trampoline tail-resumes
      while (tailAnswer) {
        TailAnswer tempTans = tailAnswer.value();
        tailAnswer = {};
        tempTans.resumption->TailResume();
      }

      Answer a = std::move(**(static_cast<std::optional<Answer>*>(OneShot::answerPtr)));
      OneShot::answerPtr = oldPtr;
      return a;
    } else {
      std::move(bodyFiber).resume();

      // Trampoline tail-resumes
      while (tailAnswer) {
        TailAnswer tempTans = tailAnswer.value();
        tailAnswer = {};
        tempTans.resumption->TailResume();
      }
      return;
    }
  }

  template <typename H>
  static typename H::AnswerType HandleWith(
    std::function<typename H::BodyType()> body, std::shared_ptr<H> handler)
  {
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return HandleWith(OneShot::FreshLabel(), body, std::move(handler));
    } else {
      HandleWith(OneShot::FreshLabel(), body, handler);
    }
  }

  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(int64_t gotoHandler, const Cmd& cmd)
  {
    // We rely on the virtual method of the metaframe, as at this
    // point we cannot know what AnswerType and BodyType are.

    // E.g. looking for d in [a][b][c][d][e][f][g.]
    // ===>
    // Run d.cmd in [a][b][c.] with r.stack = [d][e][f][g],
    // where [_.] denotes a frame with invalid (i.e. current) fiber

    if (gotoHandler == 0) {
      // Looking for handler based on the type of the command
      for (auto it = Metastack().rbegin(); it != Metastack().rend(); ++it) {
        if ((*it)->label == -1) { continue; }
        if (auto canInvoke = std::dynamic_pointer_cast<CanInvokeCmdClause<Cmd>>(*it)) {
          return canInvoke->InvokeCmd(++it, cmd);
        }
      }
      std::cerr << "error: no handler for command " << typeid(Cmd).name() << std::endl;
      DebugPrintMetastack();
      exit(-1);
    } else {
      // Looking for handler based on its label
      auto cond = [&](MetaframePtr mf) {
        return mf->label != -1 && mf->label == gotoHandler;
      };
      auto it = std::find_if(Metastack().rbegin(), Metastack().rend(), cond);
      if (auto canInvoke = std::dynamic_pointer_cast<CanInvokeCmdClause<Cmd>>(*it)) {
        return canInvoke->InvokeCmd(++it, cmd);
      }
      std::cerr << "error: handler with id " << gotoHandler
                << " does not handle " << typeid(Cmd).name() << std::endl;
      DebugPrintMetastack();
      exit(-1);
    }
  }

  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(const Cmd& cmd)
  {
    return OneShot::InvokeCmd<Cmd>(0, cmd);
  }

  template <typename Out, typename Answer>
  static Answer Resume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult)
  {
    r->cmdResultTransfer->value = std::move(cmdResult);
    return r.release()->Resume();
  }

  template <typename Answer>
  static Answer Resume(std::unique_ptr<Resumption<void, Answer>> r)
  {
    return r.release()->Resume();
  }

  template <typename Out, typename Answer>
  static Answer TailResume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult)
  {
    r->cmdResultTransfer->value = std::move(cmdResult);
    // Trampoline back to Handle
    tailAnswer = TailAnswer{r.release()};
    if constexpr (!std::is_void<Answer>::value) {
      return Answer();
    }
  }

  template <typename Answer>
  static Answer TailResume(std::unique_ptr<Resumption<void, Answer>> r)
  {
    // Trampoline back to Handle
    tailAnswer = TailAnswer{r.release()};
    if constexpr (!std::is_void<Answer>::value) {
      return Answer();
    }
  }

  template <typename Out, typename Answer>
  static std::unique_ptr<Resumption<Out, Answer>> MakeResumption(std::function<Answer(Out)> func)
  {
    Resumption<Out, Answer>* pr = new PlainResumption<Out, Answer>(func);
    return std::unique_ptr<Resumption<Out, Answer>>(pr);
  }

  template <typename Answer>
  static std::unique_ptr<Resumption<void, Answer>> MakeResumption(std::function<Answer()> func)
  {
    Resumption<void, Answer>* pr = new PlainResumption<void, Answer>(func);
    return std::unique_ptr<Resumption<void, Answer>>(pr);
  }

  static int64_t FreshLabel()
  {
    return --freshCounter;
  }

}; // class OneShot

template <typename Answer, typename Cmd>
typename Cmd::OutType CmdClause<Answer, Cmd>::InvokeCmd(
  std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd)
{
  // (continued from OneShot::InvokeCmd) ...looking for [d]
  auto jt = it.base();
  auto resumption = new Resumption<typename Cmd::OutType, Answer>();  // See (NOTE) below
  resumption->storedMetastack.splice(
    resumption->storedMetastack.begin(), OneShot::Metastack(), jt, OneShot::Metastack().end());
  // at this point: [a][b][c]; stored stack = [d][e][f][g.] 

  std::move(OneShot::Metastack().back()->fiber).resume_with([&](ctx::fiber&& prev) -> ctx::fiber {
    // at this point: [a][b][c.]; stored stack = [d][e][f][g.]
    resumption->storedMetastack.back()->fiber = std::move(prev);      // (A)
    // at this point: [a][b][c.]; stored stack = [d][e][f][g]
    if constexpr (!std::is_void<Answer>::value) {
      *(static_cast<std::optional<Answer>*>(OneShot::answerPtr)) =
        this->CommandClause(cmd,                                      // (B)
          std::unique_ptr<Resumption<typename Cmd::OutType, Answer>>(resumption));
    } else {
      this->CommandClause(cmd,
        std::unique_ptr<Resumption<typename Cmd::OutType, Answer>>(resumption));
    }
    return ctx::fiber();
  });

  // If the control reaches here, this means that the resumption is   // (C)
  // being resumed at the moment, and so we no longer need the
  // resumption object, because the fiber is no longer valid.
  if constexpr (!std::is_void<typename Cmd::OutType>::value) {
    typename Cmd::OutType cmdResult = std::move(resumption->cmdResultTransfer->value);
    delete resumption;                                                // (D)
    return cmdResult;
  } else {
    delete resumption;
  }
}

/* [NOTE] ... on memory management of resumptions

The fact that call stacks are now first class introduces an
interesting subtlety: one of the rare occasions when the code

void foo() 
{
  auto p = new C();
  ...
  delete p;
}

is correct, while a local value or a local unique pointer is not!

This is because we create a resumption, but the resumption contains a
(unique!) pointer to the fiber in which InvokeCmd's frame lives
[A]. Then, the resumption is given to the outside world as a unique
pointer [B]. Technically, it is not a unique pointer, because the
pointer "resumption" still lives in the suspended fiber, but it makes
the intent clear: the user has to either delete the resumption or
resume it once.

If the user deletes the resumption, its destructor deletes the fiber,
and so the "resumption" pointer simply disappears. Everything is fine:
both the resumption and the fiber are gone, and since the user had
unique ownership of the resumption, and the resumption had unique
ownership of the fiber, everything is tidy. But imagine what would
happen if the resumption was a local variable in InvokeCmd: In such a
case, the destructor of the resumption deletes the fiber, which means
that the stack is unwound, which means that "resumption"'s target
(i.e. the resumption) is deleted, which... is already happening at the
moment! Everything ends in a disaster.

In the other case, when the user resumes the resumption, the control
goes back to InvokeCmd [C]. The user gives up the ownership of the
resumption, which means that "resumption" is now a truly unique
pointer, and we can safely delete it [D], as it won't be needed any
more. Because the resumption has unique ownership of the suspended
fiber, there is no other way to resume the fiber than by resuming this
particular resumption, which means that it is safe to delete it now.
*/

template <typename Out, typename Answer>
Answer Resumption<Out, Answer>::Resume()
{
  if constexpr (!std::is_void<Answer>::value) {
    std::optional<Answer> answer;
    void* oldPtr = OneShot::answerPtr;
    OneShot::answerPtr = &answer;

    std::move(this->storedMetastack.back()->fiber).resume_with(
        [&](ctx::fiber&& prev) -> ctx::fiber {
      OneShot::Metastack().back()->fiber = std::move(prev);
      OneShot::Metastack().splice(OneShot::Metastack().end(), this->storedMetastack);
      return ctx::fiber();
    });
  
    Answer a = std::move(**(static_cast<std::optional<Answer>*>(OneShot::answerPtr)));
    OneShot::answerPtr = oldPtr;
    return a;
  } else {
    std::move(this->storedMetastack.back()->fiber).resume_with(
        [&](ctx::fiber&& prev) -> ctx::fiber {
      OneShot::Metastack().back()->fiber = std::move(prev);
      OneShot::Metastack().splice(OneShot::Metastack().end(), this->storedMetastack);
      return ctx::fiber();
  });
  }
}

template <typename Out, typename Answer>
void Resumption<Out, Answer>::TailResume()
{
  std::move(this->storedMetastack.back()->fiber).resume_with(
      [&](ctx::fiber&& prev) -> ctx::fiber {
    OneShot::Metastack().back()->fiber = std::move(prev);
    OneShot::Metastack().splice(OneShot::Metastack().end(), this->storedMetastack);
    return ctx::fiber();
  });
}

template <typename Out, typename Answer>
Answer PlainResumption<Out, Answer>::Resume()
{
  auto ans = std::move(this->cmdResultTransfer->value);
  auto f = func;
  delete this;
  if constexpr (!std::is_void<Answer>::value) {
    return f(std::move(ans));
  } else {
    f(std::move(ans));
  }
}

template <typename Out, typename Answer>
void PlainResumption<Out, Answer>::TailResume()
{
  if constexpr (!std::is_void<Answer>::value) {
    *(static_cast<std::optional<Answer>*>(OneShot::answerPtr)) =
      func(std::move(this->cmdResultTransfer->value));
  } else {
    func(std::move(this->cmdResultTransfer->value));
  }
  delete this;
}

// Overloads for commands with Out = void

template <typename Answer>
Answer PlainResumption<void, Answer>::Resume()
{
  auto f = func;
  delete this;
  if constexpr (!std::is_void<Answer>::value) {
    return f();
  } else {
    f();
  }
}

template <typename Answer>
void PlainResumption<void, Answer>::TailResume()
{
  if constexpr (!std::is_void<Answer>::value) {
     *(static_cast<std::optional<Answer>*>(OneShot::answerPtr)) = func();
  } else {
    func();
  }
  delete this;
}

} // namespace CppEffects

#endif // CPP_EFFECTS_CPP_EFFECTS_H
