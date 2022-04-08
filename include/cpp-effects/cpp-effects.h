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

template <typename Out, typename Answer> class Resumption;

class ResumptionBase {
  friend class OneShot;
  template <typename, typename> friend class Resumption;
  template <typename, typename> friend class ResumptionData;
public:
  virtual ~ResumptionBase() { }
protected:
  virtual void TailResume() = 0;
};

template <typename Out, typename Answer>
class ResumptionData final : public ResumptionBase {
  friend class OneShot;
  template <typename, typename> friend class CmdClause;
  template <typename, typename> friend class Resumption;
protected:
  ResumptionData() { }
  std::list<MetaframePtr> storedMetastack;
  std::optional<Tangible<Out>> cmdResultTransfer; // Used to transfer data between fibers
  Answer Resume();
  virtual void TailResume() override;
};

template <typename Out, typename Answer>
class Resumption
{
  friend class OneShot;
public:
  Resumption() { }
  Resumption(ResumptionData<Out, Answer>* data) : data(data) { }
  Resumption(ResumptionData<Out, Answer>& data) : data(&data) { }
  Resumption(std::function<Answer(Out)>);
  Resumption(const Resumption<Out, Answer>&) = delete;
  Resumption(Resumption<Out, Answer>&& other)
  {
    data = other.data;
    other.data = nullptr;
  }
  Resumption& operator=(const Resumption<Out, Answer>&) = delete;
  Resumption& operator=(Resumption<Out, Answer>&& other)
  {
    if (this != &other) {
      data = other.data;
      other.data = nullptr;
    }
    return *this;
  }
  ~Resumption()
  {
    if (data) {
      data->cmdResultTransfer = {};

      // We move the resumption buffer out of the metaframe to break
      // the pointer/stack cycle.
      std::list<MetaframePtr> _(std::move(data->storedMetastack));
    }
  }
  explicit operator bool() const
  {
    return data != nullptr && (bool)data->storedMetastack.front().fiber;
  }
  bool operator!() const
  {
    return data == nullptr || !data->storedMetastack.front().fiber;
  }
  ResumptionData<Out, Answer>* Release()
  {
    auto d = data;
    data = nullptr;
    return d;
  }
  Answer Resume(Out cmdResult) &&
  {
    data->cmdResultTransfer->value = std::move(cmdResult);
    return Release()->Resume();
  }
  Answer TailResume(Out cmdResult) &&;
private:
  ResumptionData<Out, Answer>* data = nullptr;
};

template <typename Answer>
class Resumption<void, Answer>
{
  friend class OneShot;
public:
  Resumption() { }
  Resumption(ResumptionData<void, Answer>* data) : data(data) { }
  Resumption(ResumptionData<void, Answer>& data) : data(&data) { }
  Resumption(std::function<Answer()>);
  Resumption(const Resumption<void, Answer>&) = delete;
  Resumption(Resumption<void, Answer>&& other)
  {
    data = other.data;
    other.data = nullptr;
  }
  Resumption& operator=(const Resumption<void, Answer>&) = delete;
  Resumption& operator=(Resumption<void, Answer>&& other)
  {
    if (this != &other) {
      data = other.data;
      other.data = nullptr;
    }
    return *this;
  }
  ~Resumption()
  {
    if (data) {
      data->cmdResultTransfer = {};

      // We move the resumption buffer out of the metaframe to break
      // the pointer/stack cycle.
      std::list<MetaframePtr> _(std::move(data->storedMetastack));
    }
  }
  explicit operator bool() const
  {
    return data != nullptr && (bool)data->storedMetastack.front().fiber;;
  }
  bool operator!() const
  {
    return data == nullptr || !data->storedMetastack.front().fiber;
  }
  ResumptionData<void, Answer>* Release()
  {
    auto d = data;
    data = nullptr;
    return d;
  }
  Answer Resume() &&
  {
    return Release()->Resume();
  }
  Answer TailResume() &&;
private:
  ResumptionData<void, Answer>* data = nullptr;
};

// ----------
// Metaframes
// ----------

// Labels:
// 0   -- reserved for the initial fiber with no handler
// >0  -- user-defined labels
// <0  -- auto-generated labels

class Metaframe {
  friend class OneShot;
  template <typename, typename> friend class CmdClause;
  friend class InitMetastack;
  template <typename, typename> friend class ResumptionData;
public:
  virtual ~Metaframe() { }
  virtual void DebugPrint() const
  {
    std::cout << "[" << label << ":" << typeid(*this).name();
    if (!fiber) { std::cout << " (active)"; }
    std::cout << "]";
  }
public:
  Metaframe() : label(0) { }
protected:
  int64_t label;
private:
  ctx::fiber fiber;
  void* returnBuffer;
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
  template <typename, typename, typename...> friend class Handler;
protected:
  virtual Answer CommandClause(Cmd, Resumption<typename Cmd::OutType, Answer>) = 0;
private:
  virtual typename Cmd::OutType InvokeCmd(
    std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd) final override;
  ResumptionData<typename Cmd::OutType, Answer> resumptionBuffer;
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
  using CmdClause<Answer, Cmds>::InvokeCmd...;
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
  using CmdClause<Answer, Cmds>::InvokeCmd...;
public:
  using AnswerType = Answer;
  using BodyType = void;
protected:
  virtual Answer ReturnClause() = 0;
private:
  Answer RunReturnClause(Tangible<void>) { return ReturnClause(); }
};

// A handler without the return clause

template <typename Answer, typename... Cmds>
class FlatHandler : public Handler<Answer, Answer, Cmds...> {
  Answer ReturnClause(Answer a) final override { return a; }
};

template <typename... Cmds>
class FlatHandler<void, Cmds...> : public Handler<void, void, Cmds...> {
  void ReturnClause() final override { }
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
  template <typename, typename> friend class ResumptionData;
  template <typename, typename> friend class ResuptionPtr;
  friend class InitMetastack;

public:
  static std::list<MetaframePtr> Metastack;

  static int64_t& FreshLabel()
  {
    static int64_t freshCounter = -1;
    return --freshCounter;
  }

  static std::optional<TailAnswer>& tailAnswer()
  {
    static std::optional<TailAnswer> answer = {};
    return answer;
  }

  OneShot() = delete;

  static void DebugPrintMetastack()
  {
    std::cerr << "metastack: ";
    for (auto x : Metastack) { x->DebugPrint(); }
    std::cerr << std::endl;
  }

  template <typename H, typename... Args>
  static typename H::AnswerType Handle(int64_t label, std::function<typename H::BodyType()> body, Args&&... args)
  {
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return HandleWith(label, body, std::make_shared<H>(std::forward<Args>(args)...));
    } else {
      HandleWith(label, body, std::make_shared<H>(std::forward<Args>(args)...));
    }
  }

  template <typename H, typename... Args>
  static typename H::AnswerType Handle(std::function<typename H::BodyType()> body, Args&&... args)
  {
    if constexpr (!std::is_void<typename H::AnswerType>::value) {
      return Handle<H>(OneShot::FreshLabel(), body, std::forward<Args>(args)...);
    } else {
      Handle<H>(OneShot::FreshLabel(), body, std::forward<Args>(args)...);
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
      Metastack.back()->fiber = std::move(prev);
      handler->label = label;
      Metastack.push_back(handler);

      Tangible<Body> b(body);

      MetaframePtr returnFrame(std::move(Metastack.back()));
      Metastack.pop_back();

      std::move(Metastack.back()->fiber).resume_with([&](ctx::fiber&&) -> ctx::fiber {
        if constexpr (!std::is_void<Answer>::value) {
          *(static_cast<std::optional<Answer>*>(Metastack.back()->returnBuffer)) =
             std::static_pointer_cast<H>(returnFrame)->RunReturnClause(std::move(b));
        } else {
          std::static_pointer_cast<H>(returnFrame)->RunReturnClause(std::move(b));
        }
        return ctx::fiber();
      });
      
      // Unreachable: this fiber is now destroyed
      std::cerr << "error: impssible!\n";
      exit(-1);
    }};

    if constexpr (!std::is_void<Answer>::value) {
      std::optional<Answer> answer;
      void* prevBuffer = Metastack.back()->returnBuffer;
      Metastack.back()->returnBuffer = &answer;
      std::move(bodyFiber).resume();

      // Trampoline tail-resumes
      while (tailAnswer()) {
        TailAnswer tempTans = tailAnswer().value();
        tailAnswer() = {};
        tempTans.resumption->TailResume();
      }

      Metastack.back()->returnBuffer = prevBuffer;
      return std::move(*answer);
    } else {
      std::move(bodyFiber).resume();

      // Trampoline tail-resumes
      while (tailAnswer()) {
        TailAnswer tempTans = tailAnswer().value();
        tailAnswer() = {};
        tempTans.resumption->TailResume();
      }
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

  static std::list<MetaframePtr>::reverse_iterator FindHandler(int64_t gotoHandler)
  {
    auto cond = [&](const MetaframePtr& mf) { return mf->label == gotoHandler; };
    return std::find_if(Metastack.rbegin(), Metastack.rend(), cond);
  }

  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd)
  {
    if (auto canInvoke = std::dynamic_pointer_cast<CanInvokeCmdClause<Cmd>>(*it)) {
      return canInvoke->InvokeCmd(++it, cmd);
    }
    std::cerr << "error: selected handler does not handle " << typeid(Cmd).name() << std::endl;
    DebugPrintMetastack();
    exit(-1);
  }

  template <typename H, typename Cmd>
  static typename Cmd::OutType StaticInvokeCmd(std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd)
  {
    return (static_cast<H*>(it->get()))->H::InvokeCmd(std::next(it), cmd); // circumvent vtable
  }

  // In the InvokeCmd... methods we rely on the virtual method of the
  // metaframe, as at this point we cannot know what AnswerType and
  // BodyType are.
  //
  // E.g. looking for d in [a][b][c][d][e][f][g.]
  // ===>
  // Run d.cmd in [a][b][c.] with r.stack = [d][e][f][g],
  // where [_.] denotes a frame with invalid (i.e. current) fiber

  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(int64_t gotoHandler, const Cmd& cmd)
  {
    // Looking for handler based on its label
    auto cond = [&](const MetaframePtr& mf) { return mf->label == gotoHandler; };
    auto it = std::find_if(Metastack.rbegin(), Metastack.rend(), cond);
    if (auto canInvoke = std::dynamic_pointer_cast<CanInvokeCmdClause<Cmd>>(*it)) {
      return canInvoke->InvokeCmd(++it, cmd);
    }
    std::cerr << "error: handler with id " << gotoHandler
              << " does not handle " << typeid(Cmd).name() << std::endl;
    DebugPrintMetastack();
    exit(-1);
  }

  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(const Cmd& cmd)
  {
    // Looking for handler based on the type of the command
    for (auto it = Metastack.rbegin(); it != Metastack.rend(); ++it) {
      if (auto canInvoke = std::dynamic_pointer_cast<CanInvokeCmdClause<Cmd>>(*it)) {
        return canInvoke->InvokeCmd(++it, cmd);
      }
    }
    DebugPrintMetastack();
    exit(-1);
  }

  template <typename H, typename Cmd>
  static typename Cmd::OutType StaticInvokeCmd(int64_t gotoHandler, const Cmd& cmd)
  {
    auto cond = [&](const MetaframePtr& mf) { return mf->label == gotoHandler; };
    auto it = std::find_if(Metastack.rbegin(), Metastack.rend(), cond);
    if (it != Metastack.rend()) {
      return (static_cast<H*>(it->get()))->H::InvokeCmd(std::next(it), cmd); // circumvent vtable
    }
    std::cerr << "error: handler with id " << gotoHandler
              << " does not handle " << typeid(Cmd).name() << std::endl;
    DebugPrintMetastack();
    exit(-1);
  }

  template <typename H, typename Cmd>
  static typename Cmd::OutType StaticInvokeCmd(const Cmd& cmd)
  {
    auto it = Metastack.rbegin();
    return (static_cast<H*>(it->get()))->H::InvokeCmd(std::next(it), cmd); // circumvent vtable
  }
}; // class OneShot

template <typename Answer, typename Cmd>
typename Cmd::OutType CmdClause<Answer, Cmd>::InvokeCmd(
  std::list<MetaframePtr>::reverse_iterator it, const Cmd& cmd)
{
  using Out = typename Cmd::OutType;

  // (continued from OneShot::InvokeCmd) ...looking for [d]
  auto jt = it.base();
  ResumptionData<Out, Answer>& resumption = this->resumptionBuffer;
  resumption.storedMetastack.splice(
    resumption.storedMetastack.begin(), OneShot::Metastack, jt, OneShot::Metastack.end());
  // at this point: [a][b][c]; stored stack = [d][e][f][g.] 

  std::move(OneShot::Metastack.back()->fiber).resume_with([&](ctx::fiber&& prev) -> ctx::fiber {
    // at this point: [a][b][c.]; stored stack = [d][e][f][g.]
    resumption.storedMetastack.back()->fiber = std::move(prev);
    // at this point: [a][b][c.]; stored stack = [d][e][f][g]

    // Keep the handler alive for the duration of the command clause call
    MetaframePtr _(resumption.storedMetastack.front());

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

template <typename Out, typename Answer>
Resumption<Out, Answer>::Resumption(std::function<Answer(Out)> func)
{
  Resumption<Out, Answer> resumption;

  struct Abort : Command<void> { };
  class HAbort : public FlatHandler<void, Abort> {
    void CommandClause(Abort, Resumption<void, void>) override { }
  };

  struct Arg : Command<Out> { Resumption<Out, Answer>& res; };
  class HArg : public FlatHandler<Answer, Arg> {
    Answer CommandClause(Arg a, Resumption<Out, Answer> r) override
    { 
      a.res = std::move(r);
      OneShot::InvokeCmd(Abort{});
    }
  };

  OneShot::Handle<HAbort>([&resumption, func](){
    OneShot::Handle<HArg>([&resumption, func](){
      return func(OneShot::InvokeCmd(Arg{{}, resumption}));
    });
  });

  data = resumption.Release();
}

template <typename Answer>
Resumption<void, Answer>::Resumption(std::function<Answer()> func)
{
  Resumption<void, Answer> resumption;

  struct Abort : Command<void> { };
  class HAbort : public FlatHandler<void, Abort> {
    void CommandClause(Abort, Resumption<void, void>) override { }
  };

  struct Arg : Command<void> { Resumption<void, Answer>& res; };
  class HArg : public FlatHandler<Answer, Arg> {
    Answer CommandClause(Arg a, Resumption<void, Answer> r) override
    {
      a.res = std::move(r);
      OneShot::InvokeCmd(Abort{});
    }
  };

  OneShot::Handle<HAbort>([&resumption, func](){
    OneShot::Handle<HArg>([&resumption, func](){
      OneShot::InvokeCmd(Arg{{}, resumption});
      return func();
    });
  });

  data = resumption.Release();
}

template <typename Out, typename Answer>
Answer ResumptionData<Out, Answer>::Resume()
{
  if constexpr (!std::is_void<Answer>::value) {
    std::optional<Answer> answer;
    void* prevBuffer = OneShot::Metastack.back()->returnBuffer;
    OneShot::Metastack.back()->returnBuffer = &answer;

    std::move(this->storedMetastack.back()->fiber).resume_with(
        [&](ctx::fiber&& prev) -> ctx::fiber {
      OneShot::Metastack.back()->fiber = std::move(prev);
      OneShot::Metastack.splice(OneShot::Metastack.end(), this->storedMetastack);
      return ctx::fiber();
    });

    // Trampoline tail-resumes
    while (OneShot::tailAnswer()) {
      TailAnswer tempTans = OneShot::tailAnswer().value();
      OneShot::tailAnswer() = {};
      tempTans.resumption->TailResume();
    }

    OneShot::Metastack.back()->returnBuffer = prevBuffer;
    return std::move(*answer);
  } else {
    std::move(this->storedMetastack.back()->fiber).resume_with(
        [&](ctx::fiber&& prev) -> ctx::fiber {
      OneShot::Metastack.back()->fiber = std::move(prev);
      OneShot::Metastack.splice(OneShot::Metastack.end(), this->storedMetastack);
      return ctx::fiber();
    });

    // Trampoline tail-resumes
    while (OneShot::tailAnswer()) {
      TailAnswer tempTans = OneShot::tailAnswer().value();
      OneShot::tailAnswer() = {};
      tempTans.resumption->TailResume();
    }
  }
}

template <typename Out, typename Answer>
void ResumptionData<Out, Answer>::TailResume()
{
  std::move(this->storedMetastack.back()->fiber).resume_with(
      [&](ctx::fiber&& prev) -> ctx::fiber {
    OneShot::Metastack.back()->fiber = std::move(prev);
    OneShot::Metastack.splice(OneShot::Metastack.end(), this->storedMetastack);
    return ctx::fiber();
  });
}

template <typename Out, typename Answer>
Answer Resumption<Out, Answer>::TailResume(Out cmdResult) &&
{
  data->cmdResultTransfer->value = std::move(cmdResult);
  // Trampoline back to Handle
  OneShot::tailAnswer() = TailAnswer{Release()};
  if constexpr (!std::is_void<Answer>::value) {
    return Answer();
  }
}

template <typename Answer>
Answer Resumption<void, Answer>::TailResume() &&
{
  // Trampoline back to Handle
  OneShot::tailAnswer() = TailAnswer{Release()};
  if constexpr (!std::is_void<Answer>::value) {
    return Answer();
  }
}

// --------------
// Initialisation
// --------------

inline std::list<MetaframePtr> OneShot::Metastack;

class InitMetastack
{
public:
  InitMetastack()
  {
    if (OneShot::Metastack.empty()) {
      auto initMetaframe = std::make_shared<Metaframe>();
      OneShot::Metastack.push_back(initMetaframe);
    }
  }  
} inline initMetastack;

} // namespace CppEffects

#endif // CPP_EFFECTS_CPP_EFFECTS_H
