// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Main header file for the library

#ifndef HANDLER_H
#define HANDLER_H

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
  Tangible() = default;
  Tangible(T&& t) : value(std::move(t)) { }
  Tangible(const std::function<T()>& f) { value = f(); }
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
class MetaframeBase;
class InitMetastack;

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
  template <typename, typename, typename...> friend struct Metaframe;
  template <typename, typename> friend class CmdClause;
protected:
  Resumption() { }
  std::list<MetaframeBase*> storedMetastack;
  Tangible<Out> cmdResultTransfer; // Used to transfer data between fibers
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

// --------------------------------
// Transferring data between fibers
// --------------------------------

// "Transfers" are heap-allocated ephemeral objects, which are read
// and then deleted shortly afterwards. Their purpose is to enable
// communication between fibers via a global pointer to a Transfer.

struct TransferBase { };

template <typename A>
struct Transfer : TransferBase {
  Transfer() = delete;
  Transfer(const A&) = delete;
  Transfer& operator=(const Transfer& other) = delete;
  Transfer& operator=(Transfer&& other) = delete;
  Transfer(A&& v) : value(std::move(v)) { }
  Tangible<A> value;
  A GetValue()
  {
    Tangible<A> temp = std::move(value);
    delete this;
    return std::move(temp.value);
  }
};

// ----------
// Metaframes
// ----------

// Labels:
// 0   -- the initial fiber with no handler (set up by InitMetastack)
// -1  -- invalid handler (used to shadow a handler in the return clause)
// >0  -- user-defined labels
// <0  -- auto-generated labels

class MetaframeBase {
  friend class OneShot;
  template <typename, typename> friend class CmdClause;
  friend class InitMetastack;
  template <typename, typename> friend class Resumption; 
public:
  virtual ~MetaframeBase() { }
  void DebugPrint() const { std::cout << "[" << label << "," << (bool)fiber << "]"; }
protected:
  const std::vector<std::type_index> handledCmds;
  MetaframeBase(std::vector<std::type_index> handledCmds) : handledCmds(handledCmds) { }
  MetaframeBase() = default;
  int64_t label;
private:
  ctx::fiber fiber;
};

// When invoking a command in the client code, we know the type of the
// command, but we cannot know the type of the handler, hence the
// handler needs to inherit from CanInvokeCmdClause, which does not
// need the "Answer" type.

template <typename Cmd>
class CanInvokeCmdClause {
  friend class OneShot;
protected:
  virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframeBase*>::reverse_iterator it, Cmd&& cmd) = 0;
};

template <typename Answer, typename Cmd>
class CmdClause : public CanInvokeCmdClause<Cmd> {
  friend class OneShot;
protected:
  virtual Answer CommandClause(Cmd, std::unique_ptr<Resumption<typename Cmd::OutType, Answer>>) = 0;
private:
  virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframeBase*>::reverse_iterator it, Cmd&& cmd) override;
};

// --------
// Handlers
// --------

// Handlers are metaframes that inherit a number of command
// clauses. Indeed, a handler is an object, which contains code (and
// data in the style of parametrised handlers), which is *the* thing
// pushed on the metastack.

template <typename Answer, typename Body, typename... Cmds>
class Handler : public MetaframeBase, public CmdClause<Answer, Cmds>... {
  friend class OneShot;
  using CmdClause<Answer, Cmds>::CommandClause...;
public:
  using AnswerType = Answer;
  using BodyType = Body;
  Handler() : MetaframeBase({typeid(Cmds)...}) { }
protected:
  virtual Answer ReturnClause(Body b) = 0;
private:
  Answer RunReturnClause(Tangible<Body> b) { return ReturnClause(std::move(b.value)); }
};

// We specialise for Body = void

template <typename Answer, typename... Cmds>
class Handler<Answer, void, Cmds...> : public MetaframeBase, public CmdClause<Answer, Cmds>... {
  friend class OneShot;
  using CmdClause<Answer, Cmds>::CommandClause...;
public:
  using AnswerType = Answer;
  using BodyType = void;
  Handler() : MetaframeBase({typeid(Cmds)...}) { }
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
  static std::list<MetaframeBase*> Metastack;
  static InitMetastack im; // No static constructors in C++, hence we do the usual trick
  static TransferBase* transferBuffer;
  static std::optional<TailAnswer> tailAnswer;
  static int64_t freshCounter;

  OneShot() = delete;

  static void DebugPrintMetastack()
  {
    for (auto x : Metastack) {
      x->DebugPrint();
    }
  }

  template <typename H>
  static typename H::AnswerType Handle(int64_t label, std::function<typename H::BodyType()> body)
  {
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return HandleWith(label, body, std::make_unique<H>());
    } else {
      HandleWith(label, body, std::make_unique<H>());
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
    int64_t label, std::function<typename H::BodyType()> body, std::unique_ptr<H> handler)
  {
    // E.g. for different stack use policy
    // ctx::protected_fixedsize_stack pf(10000000);
    ctx::fiber bodyFiber{/*std::alocator_arg, std::move(pf),*/
        [&](ctx::fiber&& prev) -> ctx::fiber&& {
      Metastack.back()->fiber = std::move(prev);
      handler->label = label;
      Metastack.push_back(handler.release());
      Tangible<typename H::BodyType> b(body);
      // The rest of the fiber might live in a resumption, after
      // the caller is long gone, hence we cannot use arguments
      // of HandleWith by reference from now on. The return
      // clause is executed in the same fiber, but it cannot use
      // commands of the handler.
      Metastack.back()->label = -1;
      Tangible<typename H::AnswerType> a;
      if constexpr (!std::is_void<typename H::AnswerType>::value) {
        a.value = static_cast<H*>(Metastack.back())->RunReturnClause(std::move(b));
      } else {
        static_cast<H*>(Metastack.back())->RunReturnClause(std::move(b));
      }
      delete Metastack.back();
      Metastack.pop_back();
      if constexpr (!std::is_void<typename H::AnswerType>::value) {
        std::move(Metastack.back()->fiber).resume_with(
            [&](ctx::fiber&& /*thisHandler*/) -> ctx::fiber {
          // Here is the end of life of this handler's body: "thisHandler" goes
          // out of scope and the fiber is destroyed.
          OneShot::transferBuffer = new Transfer<typename H::AnswerType>(std::move(a.value));
          return ctx::fiber();
        });
      } else {
        std::move(Metastack.back()->fiber).resume();
      }

      // This will never be reached, because this fiber will have been destroyed.
      std::cerr << "error: malformed handler\n";
      exit(-1);
    }};

    std::move(bodyFiber).resume();

    // Trampoline tail-resumes
    while (tailAnswer) {
      TailAnswer tempTans = tailAnswer.value();
      tailAnswer = {};
      tempTans.resumption->TailResume();
    }
    
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return std::move(static_cast<Transfer<typename H::AnswerType>*>(transferBuffer)->GetValue());
    } else {
      return;
    }
  }

  template <typename H>
  static typename H::AnswerType
  HandleWith(std::function<typename H::BodyType()> body, std::unique_ptr<H> handler)
  {
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return HandleWith(OneShot::FreshLabel(), body, std::move(handler));
    } else {
      HandleWith(OneShot::FreshLabel(), body, std::move(handler));
    }
  }

  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(int64_t gotoHandler, Cmd&& cmd)
  {
    // E.g. looking for d in [a][b][c][d][e][f][g.]
    // ===>
    // Run d.cmd in [a][b][c.] with r.stack = [d][e][f][g],
    // where [_.] denotes a frame with invalid (i.e. current) fiber
    auto cond = [&](MetaframeBase* mf) {
      return mf->label != -1 &&
        (gotoHandler == 0
        ? std::find(mf->handledCmds.begin(), mf->handledCmds.end(), typeid(Cmd))
          != mf->handledCmds.end()
        : mf->label == gotoHandler);
    };
    auto it = std::find_if(Metastack.rbegin(), Metastack.rend(), cond);
    if (it == Metastack.rend()) {
      std::cerr << "error: no handler for " << typeid(Cmd).name() << std::endl;
      exit(-1);
    }
    // Now we rely on the virtual method of the metaframe, as at this
    // point we cannot know what AnswerType and BodyType are.
    auto canInvoke = dynamic_cast<CanInvokeCmdClause<Cmd>*>(*it);
    if (canInvoke) {
      return canInvoke->InvokeCmd(it, std::forward<Cmd>(cmd));
    } else {
      std::cerr << "error: handler with id " << gotoHandler
                << " does not handle " << typeid(Cmd).name() << std::endl;
      exit(-1);
    }
  }

  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(Cmd&& cmd)
  {
    return OneShot::InvokeCmd<Cmd>(0, std::forward<Cmd>(cmd));
  }

  template <typename Out, typename Answer>
  static Answer Resume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult)
  {
    r->cmdResultTransfer.value = std::move(cmdResult);
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
    r->cmdResultTransfer.value = std::move(cmdResult);
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

class InitMetastack {
  friend class OneShot;

  InitMetastack()
  {
    // There is always at least one metaframe on the stack
    auto mf = new MetaframeBase;
    mf->label = 0;
    OneShot::Metastack.push_back(mf);
  }

  ~InitMetastack()
  {
    for (auto i : OneShot::Metastack) { delete i; }
  }
};

template <typename Answer, typename Cmd>
typename Cmd::OutType CmdClause<Answer, Cmd>::InvokeCmd(
  std::list<MetaframeBase*>::reverse_iterator it, Cmd&& cmd)
{
  auto jt = it.base();
  --jt;
  auto resumption = new Resumption<typename Cmd::OutType, Answer>();
  resumption->storedMetastack.splice(
    resumption->storedMetastack.begin(), OneShot::Metastack, jt, OneShot::Metastack.end());
  // at this point: [a][b][c]; stored stack = [d][e][f][g.]
  
  std::move(OneShot::Metastack.back()->fiber).resume_with([&](ctx::fiber&& prev) -> ctx::fiber {
    // at this point: [a][b][c.]; stored stack = [d][e][f][g.]
    resumption->storedMetastack.back()->fiber = std::move(prev); 
    // at this point: [a][b][c.]; stored stack = [d][e][f][g]
    if constexpr (!std::is_void<Answer>::value) {
      OneShot::transferBuffer = new Transfer<Answer>(
        this->CommandClause(
          std::forward<Cmd>(cmd),
          std::unique_ptr<Resumption<typename Cmd::OutType, Answer>>(resumption)));
    } else {
      this->CommandClause(
        std::forward<Cmd>(cmd),
        std::unique_ptr<Resumption<typename Cmd::OutType, Answer>>(resumption));
    }
    return ctx::fiber();
  });

  // If the control reaches here, this means that the resumption is
  // being resumed at the moment, and so we no longer need the
  // resumption object, because the fiber is no longer valid.
  if constexpr (!std::is_void<typename Cmd::OutType>::value) {
    typename Cmd::OutType cmdResult = std::move(resumption->cmdResultTransfer.value);
    delete resumption;
    return cmdResult;
  } else {
    delete resumption;
  }
}

template <typename Out, typename Answer>
Answer Resumption<Out, Answer>::Resume()
{
  std::move(this->storedMetastack.back()->fiber).resume_with(
      [&](ctx::fiber&& prev) -> ctx::fiber {
    OneShot::Metastack.back()->fiber = std::move(prev);
    OneShot::Metastack.splice(OneShot::Metastack.end(), this->storedMetastack);
    return ctx::fiber();
  });
  
  if constexpr (!std::is_void<Answer>::value) {
    return std::move(static_cast<Transfer<Answer>*>(OneShot::transferBuffer)->GetValue());
  } else {
    return;
  }
}

template <typename Out, typename Answer>
void Resumption<Out, Answer>::TailResume()
{
  // Delete current transfer buffer
  if constexpr (!std::is_void<Answer>::value) {
    static_cast<Transfer<Answer>*>(OneShot::transferBuffer)->GetValue();
  }

  std::move(this->storedMetastack.back()->fiber).resume_with(
      [&](ctx::fiber&& prev) -> ctx::fiber {
    OneShot::Metastack.back()->fiber = std::move(prev);
    OneShot::Metastack.splice(OneShot::Metastack.end(), this->storedMetastack);
    return ctx::fiber();
  });
}

template <typename Out, typename Answer>
Answer PlainResumption<Out, Answer>::Resume()
{
  auto ans = std::move(this->cmdResultTransfer.value);
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
  // Delete current transfer buffer
  if constexpr (!std::is_void<Answer>::value) {
    static_cast<Transfer<Answer>*>(OneShot::transferBuffer)->GetValue();
  }

  if constexpr (!std::is_void<Answer>::value) {
    OneShot::transferBuffer = new Transfer<Answer>(func(std::move(this->cmdResultTransfer.value)));
  } else {
    func(std::move(this->cmdResultTransfer.value));
  }
  delete this;
}

// Overloads for commands with out type void

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
  // Delete current transfer buffer
  if constexpr (!std::is_void<Answer>::value) {
    static_cast<Transfer<Answer>*>(OneShot::transferBuffer)->GetValue();
  }

  if constexpr (!std::is_void<Answer>::value) {
    OneShot::transferBuffer = new Transfer<Answer>(func());
  } else {
    func();
  }
  delete this;
}

} // namespace CppEffects

#endif // HANDLER_H
