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

- `typename Out` - The return type when invoking the (derived) command (i.e., the return type of `OneShot::InvokeCmd`). Should be at least move-constructible and move-assignable.

**NOTE:** A class derived from `Command` can be used as a command if it is at least copy-constructible.

#### :large_orange_diamond: Command<Out>::OutType

```cpp
using OutType = Out;
```

Reveals the return type of the command.

## classes `ResumptionBase` and `Resumption`

Resumption represents a captured continuation. A resumption is given to the user either as the argument of `Handler::CmdClause` (a proper resumption) or can be lifted from a function using `OneShot::MakeResumption` (a plain resumption).

```cpp
class ResumptionBase {
public:
  virtual ~ResumptionBase() { }
};

template <typename Out, typename Answer>
class Resumption : public ResumptionBase
{
  Resumption() = delete;
};
```

- `typename Out` - In a proper resumption, the output type of the command that suspended the computation. In a plain resumption, the input type of the lifted function.

- `typename Answer` - The return type of the suspended computation (i.e., the return type of `OneShot::Resume` applied to the resumption).

## class `CmdClause`

The class for a command clause for a single command in a handler. A handler can inherit from a few `CmdClause`'s to be able to handle multiple commands.

```cpp
template <typename Answer, typename Cmd>
class CmdClause {
protected:
  virtual Answer CommandClause(Cmd, std::unique_ptr<Resumption<typename Cmd::OutType, Answer>>) = 0;
};
```

- `typename Answer` - The overall answer type of a handler that inherits from a particular `CmdClause`. Should be at least move-constructible.

- `typename Cmd` - The handled command (a class that inherits from `Command`).


#### :large_orange_diamond: CmdClause<Answer,Cmd>::CommandClause

```cpp
virtual Answer CommandClause(Cmd c, std::unique_ptr<Resumption<typename Cmd::OutType, Answer>> r);
```

A handler handles a particular command `Cmd` if it inherits from `CmdClause<..., Cmd>` and overrides `CommandClause` with the "interpretation" of the command.

- `Cmd c` - The handled command. The object `c` contains "arguments" of the command. When a handler inherits from a number of `CmdClause`'s, the right clause is chosen via the overloading resolution mechanism out of the overloads of `CommandClauses` in the handler based on the type `Cmd`.

- `std::unique_ptr<Resumption<typename Cmd::OutType, Answer>> r` - The captured resumption. When the user invokes a command, the control goes back to the handler and the computation delimited by the handler is captured as the resumption `r`. Since resumptions are one-shot, when we resume `r` using `OneShot::Resume`, we have to transfer the ownership of the pointer `r` back to the class `OneShot`, which makes resuming `r` twice illegal.

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

- `typename Answer` - The overall answer type of a derived handler and the type of the handled computstion. Should be at least move-constructible and move-assignable.

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

The interface to deal with handlers and resumptions.

```cpp
class OneShot {
public:
  OneShot() = delete;
  
  static int64_t FreshLabel();
  
  template <typename H>
  static typename H::AnswerType Handle(std::function<typename H::BodyType()> body);  
  
  template <typename H>
  static typename H::AnswerType Handle(int64_t label, std::function<typename H::BodyType()> body);
  
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
  
  template <typename Out, typename Answer>
  static std::unique_ptr<Resumption<Out, Answer>> MakeResumption(std::function<Answer(Out)> func);

  template <typename Answer>
  static std::unique_ptr<Resumption<void, Answer>> MakeResumption(std::function<Answer()> func);
  
  template <typename Out, typename Answer>
  static Answer Resume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

  template <typename Answer>
  static Answer Resume(std::unique_ptr<Resumption<void, Answer>> r);
  
  template <typename H, typename Cmd>
  static typename Cmd::OutType StaticInvokeCmd(const Cmd& cmd);
  
  template <typename H, typename Cmd>
  static typename Cmd::OutType StaticInvokeCmd(int64_t gotoHandler, const Cmd& cmd);

  template <typename Out, typename Answer>
  static Answer TailResume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

  template <typename Answer>
  static Answer TailResume(std::unique_ptr<Resumption<void, Answer>> r);
};
```

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

- **Return value** `Cmd::OutType` - the value with which the suspended computation is resumed (the argument to `OneShot::Resume`).

#### :large_orange_diamond: OneShot::MakeResumption

```cpp
template <typename Out, typename Answer>
static std::unique_ptr<Resumption<Out, Answer>> MakeResumption(std::function<Answer(Out)> func);

// overload for Out == void
template <typename Answer>
static std::unique_ptr<Resumption<void, Answer>> MakeResumption(std::function<Answer()> func);
```

Lift a function to a resumption.

- `typename Out` - The input type of the lifted function. (Note that the name `Out` is used throughout the library for the output type of a command, which is the **input** type of a resumption, hence the possibly confusing name.)

- `typename Answer` - The output type of the lifted function, and so the answer type of the resulting resumption.

- `std::function<Answer(Out)> func` - The lifted function.

- `std::function<Answer()> func` - The lifted function (an overload for `Out == void`).

- **Return value** `std::unique_ptr<Resumption<Out, Answer>>` - the resulting resumption.

#### :large_orange_diamond: OneShot::Resume

```cpp
template <typename Out, typename Answer>
static Answer Resume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

// overload for Out == void
template <typename Answer>
static Answer Resume(std::unique_ptr<Resumption<void, Answer>> r);
```

Resume a resumption.

- `typename Out` - The input type of the resumption. (Note that the name `Out` is used throughout the library for the output type of a command, which is the **input** type of a resumption, hence the possibly confusing name.)

- `typename Answer` - The answer type of the resulting resumption.

- `std::unique_ptr<Resumption<Out, Answer>> r` - The resumed resumption.

- `Out cmdResult` - The value that is returned by the command on which the resumption "hangs".

- **Return value** `Answer` - The result of the resumed computation.

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

- **Return value** `Cmd::OutType` - the value with which the suspended computation is resumed (the argument to `OneShot::Resume`).


#### :large_orange_diamond: OneShot::TailResume

```cpp
template <typename Out, typename Answer>
static Answer TailResume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

// overload for Out == void
template <typename Answer>
static Answer TailResume(std::unique_ptr<Resumption<void, Answer>> r);
```

Tail-resume a resumption. This is to be used **only inside a command clause** as the returned expression. Semantically, `return OneShot::Resume(...);` is equivalent to `return OneShot::TailResume(...);`, but it does not build up the call stack.


- `typename Out` - The input type of the resumption. (Note that the name `Out` is used throughout the library for the output type of a command, which is the **input** type of a resumption, hence the possibly confusing name.)

- `typename Answer` - The answer type of the resulting resumption.

- `std::unique_ptr<Resumption<Out, Answer>> r` - The resumed resumption.

- `Out cmdResult` - The value that is returned by the command on which the resumption "hangs".

- **Return value** `Answer` - The result of the resumed computation.


Tail-resumes are useful in command clauses such as:

```cpp
int CommandClause(SomeCommand, std::unique_ptr<Resumption<void, int>> r) override
{
  // do sth
  return OneShot::Resume(std::move(r));
}
```

In this example, we will build up the call stack until the entire handler returns a final answer (a "stack leak"). The library has a separate trampolining mechanism to avoid this with:

```cpp
  // do sth
  return OneShot::TailResume(std::move(r));
```

**NOTE:** `TailResume` can be used only if `Answer` is trivially constructible. Consider the following command clause:

```cpp
class H : Handler<Answer, void, Op> {
  // ...
  Answer CommandClause(Op, std::unique_ptr<Resumption<void, Answer>> r) override
  {
    return OneShot::TailResume(std::move(r));
  }
}
```

What happens behind the scenes is that `TailResume` returns a trivial value of type `Answer`, while the real resuming happens in a trampoline hidden in the implementation of `OneShot`.
