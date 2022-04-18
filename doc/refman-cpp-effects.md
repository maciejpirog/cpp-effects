# cpp-effects/cpp-effects.h

[<< Back to reference manual](refman.md)

This header contains the basic classes used in the library.

## class `Command`

The base class for commands.

```cpp
template <typename Out>
struct Command {
  using OutType = Out;
};
```

- `typename Out` - The return type when invoking the (derived) command (i.e., the return type of [`OneShot::InvokeCmd`](refman-cpp-effects.md#large_orange_diamond-oneshotinvokecmd)). Should be at least move-constructible and move-assignable.

**NOTE:** A class derived from `Command` can be used as a command if it is at least copy-constructible.

#### :large_orange_diamond: Command<Out>::OutType

```cpp
using OutType = Out;
```

Reveals the return type of the command.

## class `Resumption`

Resumption represents a captured continuation, that is, a suspended computation. A resumption is given to the user either as the argument of [`Handler::CommandClause`](refman-cpp-effects.md#large_orange_diamond-cmdclauseanswercmdcommandclause), or can be lifted from a function using a constructor.

```cpp
template <typename Out, typename Answer>
class Resumption
{
public:
  Resumption();
  
  Resumption(ResumptionData<Out, Answer>* data) : data(data);
  
  Resumption(std::function<Answer(Out)>);

  Resumption(const Resumption<Out, Answer>&) = delete;
  
  Resumption& operator=(const Resumption<Out, Answer>&) = delete;
  
  Resumption(Resumption<Out, Answer>&& other);

  Resumption& operator=(Resumption<Out, Answer>&& other);

  explicit operator bool() const;

  bool operator!() const;

  ResumptionData<Out, Answer>* Release();
  
  Answer Resume(Out cmdResult) &&;

  Answer TailResume(Out cmdResult) &&;
};

// Specialisation for void
template <typename Answer>
class Resumption<void, Answer>
{
public:
  // Same as the general version, except for:
  
  Resumption(std::function<Answer()>);
  
  Answer Resume() &&;

  Answer TailResume() &&;
};
```

Objects of the `Resumption` class are movable but not copyable. This is because they represent suspended **one-shot** continuations.

The `Resumption` class is actually a form of a smart pointer, so moving it around is cheap.

- `typename Out` - In a proper resumption, the output type of the command that suspended the computation. In a plain resumption, the input type of the lifted function.

- `typename Answer` - The return type of the suspended computation (i.e., the return type of `Resumption<Out, Answer>::Resume`).


#### :large_orange_diamond: Resumption<Out, Answer>::Resumption()

```cpp
/* 1 */ Resumption<Out, Answer>::Resumption()
  
/* 2 */ Resumption<Out, Answer>::Resumption(ResumptionData<Out, Answer>* data)

/* 3 */ Resumption<Out, Answer>::Resumption(std::function<Answer(Out)> func)

/* 4 */ Resumption<void, Answer>::Resumption(std::function<Answer()> func)
```

Constructors.

- `1` - Create a trivial **invalid** resumption.

- `2` - Create a resumption from data previously released with `Release`.

- `3` - Lift a function to a resumption.

- `4` - As above, specialisation for `Out == void`.

Arguments:

- `ResumptionData<Out, Answer>* data` - Data previously released with `Release`.

- `std::function<Answer(Out)> func` - The lifted function.

- `std::function<Answer()> func` - The lifted function (specialisation for `Out == void`).


#### :large_orange_diamond: Resumption<Out, Answer>::operator bool

Check if the resumption is valid. The resumption becomes invalid if moved elsewhere (in particular, when resumed).

```cpp
explicit operator bool() const;
```

- **return value** `bool` - Indicates if the resumption is valid.

#### :large_orange_diamond: Resumption<Out, Answer>::operator!

Check if the resumption is invalid. The resumption becomes invalid if moved elsewhere (in particular, when resumed).

```cpp
bool operator!() const;
```

- **return value** `bool` - Indicates if the resumption is invalid.

#### :large_orange_diamond: Resumption<Out, Answer>::Release

```cpp
ResumptionData<Out, Answer>* Release();
```

Releases the pointer to the suspended computation.

- **return value** `ResumptionData<Out, Answer>*` - The released pointer.

**Warning:** :warning: Never use `delete` on the released pointer! If you want to get rid of it safely, wrap it back in a dummy `Resumption` value, and let its destructor do the job. For example:

```cpp
void foo(Resumption<void, int> r)
{
  auto ptr = r.Release();
  // ...
  Resumption<void, int>{ptr};
}
```

#### :large_orange_diamond: Resumption<Out, Answer>::Resume

```cpp
Answer Resumption<Out, Answer>::Resume(Out cmdResult) &&

// overload for Out == void
Answer Resumption<void, Answer>::Resume() &&
```

Resume the suspended computation captured in the resumption.

- `Out cmdResult` - The value that is returned by the command on which the resumption "hangs".

- **Return value** `Answer` - The result of the resumed computation.

#### :large_orange_diamond: Resumption<Out, Answer>::TailResume

```cpp
Answer Resumption<Out, Answer>::TailResume(Out cmdResult) &&

// overload for Out == void
Answer Resumption<void, Answer>::TailResume() &&
```

Use to resume the suspended computation captured in the resumption in a tail position in the command clause. This is to be used **only inside a command clause** as the returned expression. Semantically, for an rvalue reference `r`, the expressions `return r.Resume(...);` and `return r.TailResume(...);` are semantically equivalent, but the latter does not build up the call stack.

- `Out cmdResult` - The value that is returned by the command on which the resumption "hangs".

- **Return value** `Answer` - The result of the resumed computation.


Tail-resumes are useful in command clauses such as:

```cpp
int CommandClause(SomeCommand, Resumption<void, int> r) override
{
  // do sth
  return std::move(r).TailResume();
}
```

In this example, we will build up the call stack until the entire handler returns a final answer (resulting in a "stack leak"). The library has a separate trampolining mechanism built in to avoid this:

```cpp
  // do sth
  return std::move(r).TailResume();
```

**NOTE:** `TailResume` can be used only if `Answer` is trivially constructible. Consider the following command clause:

```cpp
class H : Handler<Answer, void, Op> {
  // ...
  Answer CommandClause(Op, Resumption<void, Answer> r) override
  {
    return std::move(r).TailResume();
  }
}
```

What happens behind the scenes is that `TailResume` returns a trivial value of type `Answer`, while the real resuming happens in a trampoline hidden in the `OneShot::Handle` function.

## classes `ResumptionData` and `ResumptionBase`

Classes that represents "bare" captured continuations that are not memory-managed by the library.

```cpp
class ResumptionBase {
public:
  virtual ~ResumptionBase() { }
};

template <typename Out, typename Answer>
class ResumptionData : public ResumptionBase
{
};
```

- `typename Out` - In a proper resumption, the output type of the command that suspended the computation. In a plain resumption, the input type of the lifted function.

- `typename Answer` - The return type of the suspended computation (i.e., the return type of `Resumption<Out, Answer>::Resume` applied to the resumption).

## class `CmdClause`

The class for a command clause for a single command in a handler. A handler can inherit from a few `CmdClause`'s to be able to handle multiple commands.

```cpp
template <typename Answer, typename Cmd>
class CmdClause {
protected:
  virtual Answer CommandClause(Cmd, Resumption<typename Cmd::OutType, Answer>) = 0;
};
```

- `typename Answer` - The overall answer type of a handler that inherits from a particular `CmdClause`. Should be at least move-constructible.

- `typename Cmd` - The handled command (a class that inherits from `Command`).


#### :large_orange_diamond: CmdClause<Answer,Cmd>::CommandClause

```cpp
virtual Answer CommandClause(Cmd c, Resumption<typename Cmd::OutType, Answer> r);
```

A handler handles a particular command `Cmd` if it inherits from `CmdClause<..., Cmd>` and overrides `CommandClause` with the "interpretation" of the command.

- `Cmd c` - The handled command. The object `c` contains "arguments" of the command. When a handler inherits from a number of `CmdClause`'s, the right clause is chosen via the overloading resolution mechanism out of the overloads of `CommandClauses` in the handler based on the type `Cmd`.

- `Resumption<typename Cmd::OutType, Answer> r` - The captured resumption. When the user invokes a command, the control goes back to the handler and the computation delimited by the handler is captured as the resumption `r`. Since resumptions are one-shot, when we resume `r`, we have to give up the ownership of `r`, which makes `r` invalid.

- **return value** `Answer` - The overall result of handling the computation.

## class `Handler`

Base class for handlers.

```cpp
template <typename Answer, typename Body, typename... Cmds>
class Handler : public CmdClause<Answer, Cmds>... {
public:
  using AnswerType = Answer;
  using BodyType = Body;
protected:
  virtual Answer ReturnClause(Body b) = 0;
};

// Specialisation for Body == void:
template <typename Answer, typename... Cmds>
class Handler<Answer, void, Cmds...> : public CmdClause<Answer, Cmds>... {
public:
  using AnswerType = Answer;
  using BodyType = void;
protected:
  virtual Answer ReturnClause() = 0;
};
```

- `typename Answer` - The overall answer type of a derived handler. Should be at least move-constructible and move-assignable.

- `typename Body` - The type of the handled computation. Should be at least move-constructible and move-assignable.

- `typename... Cmds` - The commands that are handled by this handler.

To define a handler for a set of commands, one needs to derive from `Handler` and specify the command clauses (inherited from `CmdClause`'s) and the return clause.

#### :large_orange_diamond: Handler<Answer, Body, Cmds...>::AnswerType

```cpp
using AnswerType = Answer;
```

Reveals the type of the overall answer of a handler.


#### :large_orange_diamond: Handler<Answer, Body, Cmds...>::BodyType

```cpp
using BodyType = Body;
```

Reveals the type of the handled computation.

#### :large_orange_diamond: Handler<Answer, Body, Cmds...>::ReturnCluase

```cpp
virtual Answer ReturnClause(Body b); // if Body != void
virtual Answer ReturnClause();       // if Body == void
```

If a handled computation gives a result without invoking an operation, `ReturnClause` specifies the overall answer of the handler.

- `Body b` - the value that is the result of the handled computation.

- **return value** `Answer` - The overall answer of the handler.

## class `FlatHandler`

Base class for handlers in which the return clause is identity. It is useful for handlers that are generic in the answer type, and spares the programmer writing a separate specialisation for when the answer type is `void`.

```cpp
template <typename Answer, typename... Cmds>
class FlatHandler : public Handler<Answer, Answer, Cmds>... {
  Answer ReturnClause(Answer a) final override;
};

template <typename... Cmds>
class FlatHandler<void, Cmds...> : public Handler<void, void, Cmds>... {
  void ReturnClause() final override;
};
```

- `typename Answer` - The overall answer type of a derived handler and the type of the handled computation. Should be at least move-constructible and move-assignable.

- `typename... Cmds` - The commands that are handled by this handler.


The `ReturnClause`-s are defined as:

```cpp
template <typename Answer, typename... Cmds>
Answer FlatHandler<Answer, Cmds...>::ReturnClause(Answer a)
{
  return a;
}

template <typename... Cmds>
void FlatHandler<void, Cmds...>::ReturnClause() { }
```

<details>
  <summary><strong>Example</strong></summary>

Consider the following tick handler that is generic in the return type:

```cpp
struct Tick : Command<int> { };

template <typename T>
class Counter : public FlatHandler <T, Plain<Tick>> {
  int counter = 0;
  int CommandClause(Tick) final override
  {
    return ++counter;
  }
};
```

With the `Handler` class we would have to provide a separate specialisation for `void`:

```cpp
struct Tick : Command<int> { };

template <typename T>
class Counter : public Handler<T, Plain<Tick>> {
  int counter = 0;
  int CommandClause(Tick) final override
  {
    return ++counter;
  }
  T ReturnClause(T a)
  {
    return a;
  }
};

template <>
class Counter<void> : public Handler<void, Plain<Tick>> {
  int counter = 0;
  int CommandClause(Tick) final override
  {
    return ++counter;
  }
  void ReturnClause() { }
};
```

</details>

## class `OneShot`

The interface to deal with handling and invoking commands.

```cpp
class OneShot {
public:
  OneShot() = delete;
  
  static void DebugPrintMetastack();
  
  static int64_t FreshLabel();
  
  template <typename H, typename... Args>
  static typename H::AnswerType Handle(std::function<typename H::BodyType()> body, Args&&... args);

  template <typename H, typename... Args>
  static typename H::AnswerType Handle(int64_t label, std::function<typename H::BodyType()> body, Args&&... args);
  
  template <typename H>
  static typename H::AnswerType HandleWith(
    std::function<typename H::BodyType()> body, std::shared_ptr<H> handler);
  
  template <typename H>
  static typename H::AnswerType HandleWith(
    int64_t label, std::function<typename H::BodyType()> body, std::shared_ptr<H> handler);
	
  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(const Cmd& cmd);
  
  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(int64_t label, const Cmd& cmd);
  
  template <typename H, typename Cmd>
  static typename Cmd::OutType StaticInvokeCmd(const Cmd& cmd);
  
  template <typename H, typename Cmd>
  static typename Cmd::OutType StaticInvokeCmd(int64_t gotoHandler, const Cmd& cmd);
};
```

#### :large_orange_diamond: OneShot::DebugPrintMetastack

```cpp
static void DebugPrintMetastack();
```

Prints out the current stack of handlers. Useful for "printf" debugging. The format of each frame is:

```
[label:type_name]
```

where `label` is the label of the handler (given by the user or automatically generated by `OneShot::Handle`), and `tpye_name` is the `std::typeinfo::name` of the handler. The stack grows left-to-right, while the first frame is always a dummy "global" handler with label `0`.

<details>
  <summary><strong>Example</strong></summary>

```cpp
struct Error : Command<void> { };

class ErrorHandler : public FlatHandler<void, NoResume<Error>> {
  void CommandClause(Error) override { }
};

template <typename S>
struct Put : Command<void> {
  S newState;
};

template <typename S>
struct Get : Command<S> { };

template <typename Answer, typename S>
class Stateful : public FlatHandler<Answer, Plain<Put<S>>, Plain<Get<S>>> {
public:
  Stateful(S initialState) : state(initialState) { }
private:
  S state;
  void CommandClause(Put<S> p) final override
  {
    state = p.newState;
  }
  S CommandClause(Get<S>) final override
  {
    return state;
  }
};

int main()
{
  OneShot::Handle<ErrorHandler>(100, []() {
    OneShot::Handle<ErrorHandler>(200, []() {
      OneShot::Handle<Stateful<void, int>>([]() {
        OneShot::DebugPrintMetastack();
      }, 0);
    });
  });
}
```

Output:

```
metastack: [0:N10CppEffects9MetaframeE][100:12ErrorHandler][200:12ErrorHandler][-2:8StatefulIviE (active)]
```

</details>



#### :large_orange_diamond: OneShot::FreshLabel

```cpp
static int64_t FreshLabel();
```

When a command is invoked, the handler is chosen based on the type of the operation using the usual "innermost" rule. However, one can use labels to directly match operations with handlers. The function `FreshLabel` generates a unique label, which can be used later with the overloads with `label` arguments.

- **Return value** `int64_t` - The generated label.

#### :large_orange_diamond: OneShot::Handle

```cpp
template <typename H, typename... Args>
static typename H::AnswerType Handle(std::function<typename H::BodyType()> body, Args&&... args);
  
template <typename H, typename... Args>
static typename H::AnswerType Handle(int64_t label, std::function<typename H::BodyType()> body, Args&&... args);
```

Create a new handler of type `H` and use it to handle the computation `body`.

- `typename H` - The type of the handler that is used to handle `body`.

- `typename... Args` - Arguments supplied to the constructor of `H`.

- `int64_t label` - Explicit label of the handler. If no label is given, this handler is used based on the types of the operations of `H` (the innermost handler that handles an invoked operation is used).

- `std::function<typename H::BodyType()> body` - The handled computation.

- **Return value** `H::AnswerType` - The final answer of the handler, returned by one of the overloads of `H::CommandClause` or `H::ReturnClause`.

Note: `OneShot::Handle<H>(b)` is equivalent to `OneShot::Handle(b, std::make_sharede<H>())`.

#### :large_orange_diamond: OneShot::HandleWith

```cpp
  template <typename H>
  static typename H::AnswerType HandleWith(
    std::function<typename H::BodyType()> body, std::shared_ptr<H> handler);
  
  template <typename H>
  static typename H::AnswerType HandleWith(
    int64_t label, std::function<typename H::BodyType()> body, std::shared_ptr<H> handler);
```

Hadle the computation `body` using the given handler of type `H`.

- `typename H` - The type of the handler that is used to handle `body`.

- `int64_t label` - Explicit label given to `handler`. If no label is given, this handler is used based on the types of the operations of `H` (the innermost handler that handles an invoked operation is used).

- `std::function<typename H::BodyType()> body` - The handled computation.

- `std::shared_ptr<H> handler` - The handler used to handle `body`.

- **Return value** `H::AnswerType` - The final answer of the handler, returned by one of the overloads of `H::CommandClause` or `H::ReturnClause`.

#### :large_orange_diamond: OneShot::InvokeCmd

```cpp
template <typename Cmd>
static typename Cmd::OutType InvokeCmd(const Cmd& cmd);
  
template <typename Cmd>
static typename Cmd::OutType InvokeCmd(int64_t label, const Cmd& cmd);
```

Used in a handled computation to invoke a particular command. The current computation (up  to and including the appropriate handler) is suspended, captured in a resumption, and the control goes to the handler.

- `typename Cmd` - The type of the invoked command.

- `int64_t label` - The label of the handler to which the control should go. If there is no handler with label `label` in the context or it does not handle the operation `Cmd`, the program ends with exit code `-1`.

- `const Cmd& cmd` - The invoked command.

- **Return value** `Cmd::OutType` - the value with which the suspended computation is resumed.


#### :large_orange_diamond: OneShot::StaticInvokeCmd

```cpp
template <typename H, typename Cmd>
static typename Cmd::OutType StaticInvokeCmd(const Cmd& cmd);
  
template <typename H, typename Cmd>
static typename Cmd::OutType StaticInvokeCmd(int64_t gotoHandler, const Cmd& cmd);
```

Used in a handled computation to invoke a particular command (similar to [`OneShot::InvokeCmd`](#large_orange_diamond-oneshotinvokecmd)), but the handler with the given label is statically cast to `H`. The current computation (up  to and including the appropriate handler) is suspended, captured in a resumption, and the control goes to the handler.

- `typename H` - The type of the handler used to handled the command.

- `typename Cmd` - The type of the invoked command.

- `int64_t label` - The label of the handler to which the control should go. If there is no handler with label `label`, the program exits with exit code `-1`. If the inner-most handler with label `label` is not of type (derived from) `H`, it results in undefined behaviour. If `label` is note supplied, the inner-most handler is used.

- `const Cmd& cmd` - The invoked command.

The point of `StaticInvokeCmd` is that we often know upfront which handler will be used for a particular command. This way, instead of dynamically checking if a given handler is able to handle the invoke command, we can statically cast it to an appropriate handler `H`, trading dynamic type safety for performance.

- **Return value** `Cmd::OutType` - the value with which the suspended computation is resumed.

