# cpp-effects: Reference Manual

## Overview

Header: `cpp-effects/cpp-effects.h`

Namespace: `CppEffects`

The classes in the interface of the library are:

- `Command` - Classes derived from `Command` define commands.

- `Handler` - Classes derived from `Handler` define handlers.

- `Resumption` - The class for suspended computations that are given to the user in handlers' command clauses.

- `OneShot` - The actual interface (via static functions) of handling computations and invoking commands.

## Quick comparison with the more traditional functional approach

Effect handlers first appeared in the area of functional programming (FP). There are, however, some differences between how handlers are usually formulated in FP and how they look like in this library, which is more object-oriented-oriented.

#### Commands

In FP, commands (also called "operations" in the literature) are sometimes syntactically quite like functions: they have a function type, and you call them as if they were ordinary functions. More often, however, you need an additional keyword to invoke a command, such as `perform` in Eff or Multicore OCaml. For example, one can define the following in Multicore OCaml (compare [state.ml](https://github.com/ocaml-multicore/effects-examples/blob/master/state.ml)):

```
(* definition of operations *)
effect Put : int -> unit
effect Get : int

(* functional interface *)
let put v = perform (Put v)
let get () = perform Get
```

In this library, commands are defined as classes that derive from `Command`. To perform ("invoke" in the parlance we use here) a command, you need to pass an object of such a class. The return type of a command is the type argument to `Command` (which is a one-argument template), while its arguments are data members:

```cpp
/* definition of commands */
struct Put : Command<void> { int newState; };
struct Get : Command<int> { };

/* functional interface */
void put(int v) {
  OneShot::InvokeCmd(Put{{}, v});
}
int get() {
  OneShot::InvokeCmd(Get{});
}
```

The relevant declarations are:

```cpp
template <typename Out>
struct Command {
  using OutType = Out;
};

template <typename Cmd>
static typename Cmd::OutType OneShot::InvokeCmd(Cmd&& cmd);
```

#### Resumptions

Resumptions represent suspended computations that "hang" on a command. In FP, they sometimes are simply functions, in which the type of the argument is the return type of the command. They can also be a separate type (as in, again, Eff or Multicore OCaml). In this library, they are a separate type:

```cpp
template <typename Out, typename Answer>
class Resumption;
```

We can resume (or "continue" as called in Multicore OCaml) a resumption using `OneShot::Resume`:

```cpp
template <typename Out, typename Answer>
static Answer OneShot::Resume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

template <typename Answer>
static Answer OneShot::Resume(std::unique_ptr<Resumption<void, Answer>> r);
```

#### Handlers

The most popular design in FP is that effect handlers look quite similar to exception handlers. Here, the story is quite different: a handler can be defined by deriving from the `Handler` class. The definitions of "command" and "return" clauses are defined by overriding appropriate virtual functions in `Handler`. Then, a computation is handled by an **object** of that derived class. Thus, the derived class that defines a handler can have its own internal state, auxiliary members, friendships, etc., which leads to a very convenient generalisation of the notion of "parameterised" handlers.

For example, one can define a handler for `Put` and `Get` as follows:

```cpp
template <typename T> // the type of the handled computation
class State : public Handler<T, T, Put, Get> {
public:
  State(int initialState) : state(initialState) { }
private:
  int state;
  T CommandClause(Get, std::unique_ptr<Resumption<int, T>> r) override
  {
    return OneShot::TailResume(std::move(r), state);
  }
  T CommandClause(Put p, std::unique_ptr<Resumption<void, T>> r) override
  {
    state = p.newState;
    return OneShot::TailResume(std::move(r));
  }
  T ReturnClause(T v) override
  {
    return v;
  }
}
```

## class `Command`

The base class for commands.

```cpp
template <typename Out>
struct Command {
  using OutType = Out;
};
```

- `typename Out` - The return type when invoking the (derived) command (i.e., the return type of `OneShot::InvokeCmd`).

#### OutType

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

- `typename Answer` - The overall answer type of a handler that inherits from a particular `CmdClause`.

- `typename Cmd` - The handled command (a class that inherits from `Command`).


#### CommandClause

```
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

- `typename Answer` - The overall answer type of a derived handler.

- `typename Body` - The type of the handled computation.

- `typename... Cmds` - The commands that are handled by this handler.

To define a handler for a set of commands, one needs to derive from `Handler` and specify the command clauses (inherited from `CmdClause`'s) and the return clause.

#### AnswerType

```cpp
using AnswerType = Answer;
```

Reveals the type of the overall answer of a handler.


#### BodyType

```cpp
using BodyType = Body;
```

Reveals the type of the handled computation.

#### ReturnCluase

```cpp
virtual Answer ReturnClause(Body b); // if Body != void
virtual Answer ReturnClause();       // if Body == void
```

If a handled computation gives a result without invoking an operation, `ReturnClause` specifies the overall answer of the handler.

- `Body b` - the value that is the result of the handled computation.

- **return value** `Answer` - The overall answer of the handler.

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
    std::function<typename H::BodyType()> body, std::unique_ptr<H> handler);
  
  template <typename H>
  static typename H::AnswerType HandleWith(
    int64_t label, std::function<typename H::BodyType()> body, std::unique_ptr<H> handler);
	
  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(Cmd&& cmd);
  
  template <typename Cmd>
  static typename Cmd::OutType InvokeCmd(int64_t label, Cmd&& cmd);
  
  template <typename Out, typename Answer>
  static std::unique_ptr<Resumption<Out, Answer>> MakeResumption(std::function<Answer(Out)> func);

  template <typename Answer>
  static std::unique_ptr<Resumption<void, Answer>> MakeResumption(std::function<Answer()> func);
  
  template <typename Out, typename Answer>
  static Answer Resume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

  template <typename Answer>
  static Answer Resume(std::unique_ptr<Resumption<void, Answer>> r);

  template <typename Out, typename Answer>
  static Answer TailResume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

  template <typename Answer>
  static Answer TailResume(std::unique_ptr<Resumption<void, Answer>> r);
};

#### FreshLabel

```cpp
static int64_t FreshLabel();
```

Generate a unique label for a handler, which can be used later with the overloads with `label` arguments.

- **Return value** `int64_t` - The generated label.

#### Handle

```cpp
template <typename H>
static typename H::AnswerType Handle(std::function<typename H::BodyType()> body);
  
template <typename H>
static typename H::AnswerType Handle(int64_t label, std::function<typename H::BodyType()> body);
```

Create a new handler of type `H` (using its trivial constructor) and use it to hadle the computation `body`.

- `typename H` - The type of the handler that is used to handle `body`.

- `int64_t label` - Explicit label of the handler. If no label is given, this handler is used based on the types of the operations of `H` (the innermost handler that handles an invoked operation is used).

- `std::function<typename H::BodyType()> body` - The handled computation.

- **Return value** `H::AnswerType` - The final answer of the handler, returned by one of the overloads of `H::CommandClause` or `H::ReturnClause`.

Note: `OneShot::Handle<H>(b)` is equivalent to `OneShot::Handle(b, std::make_unique<H>())`.

#### HandleWith

```cpp
  template <typename H>
  static typename H::AnswerType HandleWith(
    std::function<typename H::BodyType()> body, std::unique_ptr<H> handler);
  
  template <typename H>
  static typename H::AnswerType HandleWith(
    int64_t label, std::function<typename H::BodyType()> body, std::unique_ptr<H> handler);
```

Hadle the computation `body` using the given handler of type `H`.

- `typename H` - The type of the handler that is used to handle `body`.

- `int64_t label` - Explicit label given to `handler`. If no label is given, this handler is used based on the types of the operations of `H` (the innermost handler that handles an invoked operation is used).

- `std::function<typename H::BodyType()> body` - The handled computation.

- `std::unique_ptr<H> handler` - The handler used to handle `body`.

- **Return value** `H::AnswerType` - The final answer of the handler, returned by one of the overloads of `H::CommandClause` or `H::ReturnClause`.

#### InvokeCmd

```cpp
template <typename Cmd>
static typename Cmd::OutType InvokeCmd(Cmd&& cmd);
  
template <typename Cmd>
static typename Cmd::OutType InvokeCmd(int64_t label, Cmd&& cmd);
```

Used in a handled computation to invoke a particular command. The current computation (up  to and including the appropriate handler) is suspended, captured in a resumption, and the control goes to the handler.

- `typename Cmd` - The type of the invoked command.

- `int64_t label` - The label of the handler to which the control should go. If there is no handler with label `label` in the context or it does not handle the operation `Cmd`, the program ends with exit code `-1`.

- `Cmd&& cmd` - The invoked command.

- **Return value** `Cmd::OutType` - the value with which the suspended computation is resumed (the argument to `OneShot::Resume`).

#### MakeResumption

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

#### Resume

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

#### TailResume

```cpp
template <typename Out, typename Answer>
static Answer TailResume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

// overload for Out == void
template <typename Answer>
static Answer TailResume(std::unique_ptr<Resumption<void, Answer>> r);
```

Tail-resume a resumption. This is to be used **only inside a command clause** as the last thing in its body. Tail-resumes are useful in command clauses such as:

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

- `typename Out` - The input type of the resumption. (Note that the name `Out` is used throughout the library for the output type of a command, which is the **input** type of a resumption, hence the possibly confusing name.)

- `typename Answer` - The answer type of the resulting resumption.

- `std::unique_ptr<Resumption<Out, Answer>> r` - The resumed resumption.

- `Out cmdResult` - The value that is returned by the command on which the resumption "hangs".

- **Return value** `Answer` - The result of the resumed computation.
