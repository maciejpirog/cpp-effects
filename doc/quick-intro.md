# Quick introduction (and comparison with effectful functional programming)

[<< Back to reference manual](refman.md)

This document gives a quick overview of the API of the library. It assumes that the reader has some basic understanding of algebraic effects and effect handlers.

Effect handlers first appeared in the area of functional programming (e.g., Eff, Koka, OCaml). There are, however, some differences between how handlers are usually formulated in FP and what our library provides, trying to combine effects with object-oriented programming - and C++ programming in particular.

## Namespace

The library lives in the namespace `cpp_effects`. We often abbreviate it to `eff`, that is:

```cpp
namespace eff = cpp_effects;
```

## Commands

In our library, *commands* (sometimes called *operations* in the handler literature) are defined as classes derived from the `command` template. The (optional) type argument of the template is the return type of the command. For example, we can formulate the usual interface for mutable state of type `int` as follows:

```cpp
struct Put : eff::command<> { int newState; };
struct Get : eff::command<int> { };
```

Data members of these classes are arguments of the commands.

:point_right: The `command` class is actually a wrapper for two types:

```cpp
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
```

To *perform* (or: *invoke*) a command, we need to use an object of such the derived class. The out type of the command becomes the result of the invocation:

```cpp
template <typename Cmd>
typename Cmd::out_type invoke_command(const Cmd& cmd);
```

For example:

```cpp
void put(int s) {
  eff::invoke_command(Put{{}, s});
}

template <typename S>
int get() {
  return eff::invoke_command(Get{});
}
```

:point_right: Note that the `{}` in `Put{{}, s}` is necessary to initialise (even if the initialisation is rather trivial), the `command` superclass.

:dromedary_camel: The equivalent code in OCaml is as follows (compare [ocaml-multicore/effects-examples/state.ml](https://github.com/ocaml-multicore/effects-examples/blob/master/state.ml)):

```ocaml
(* definition of operations *)
effect Put : int -> unit
effect Get : int

(* functional interface *)
let put v = perform (Put v)
let get () = perform Get
```

## Handlers

In general, a *handler* is a piece of code that knows what to do with a set of commands. One can subsequently *handle* a computation by delimiting it using the handler. If in this computation an operation is invoked, the handler assumes control, receiving the command together with the computation captured as a *resumption* (aka *continuation*).

In our library, we take advantage of the basic principle of object-oriented programming: packing together data and code. And so, a handler is a class that contains member functions that know how to handle particular commands, but it can also encapsulate as much data and auxiliary definitions as the programmer sees fit. Then, we can handle a computation using a particular *object* of that class. This object's life is managed by the library, providing a persistent "context of execution" of the handled computation. For example, we can define a handler for mutable state using a data member in the handler:

```cpp
template <typename T>  // the type of the handled computation
class State : public eff::handler<T, T, Put, Get> {
public:
  State(int initialState) : state(initialState) { }
private:
  int state;
  T handle_command(Get, eff::resumption<T(int)> r) override
  {
    return std::move(r).tail_resume(state);
  }
  T handle_command(Put p, eff::esumption<T()> r) override
  {
    state = p.newState;
    return std::move(r).tail_resume();
  }
  T handle_return(T v) override
  {
    return v;
  }
};
```

FP usually makes effect handlers syntactically quite similar to exception handlers. We take a different approach here: a handler can be defined by deriving from the `handler` class template. The user defines "command" and "return" clauses by overriding appropriate virtual functions in `handler`, one for each command listed in the template parameter list.

We can handle a computation as follows:

```cpp
int body()
{
  put(get() * 2);
  return get();
}

void foo() {
  // ...
  eff::handle<State<int>>(body, 10); // returns 20
}
```

The `handle` function will take care of creating the handler object for us, forwarding its subsequent arguments (`10`) to the constructor of `State`.

## Resumptions

A resumptions represents a suspended computation that "hangs" on a command. In FP, they can be functions, in which the type of the argument is the return type of the command. They can also be a separate type (as in, again, Eff or OCaml). In this library, they are a separate type:

```cpp
template <typename T>
class resumption;

template <typename Out, typename Answer>
class resumption<Answer(Out)> { ... };

template <typename Answer>
class resumption<Answer()> { ... };
```

We can *resume* (or *continue* as it is called in OCaml) a resumption using the `resume` member function:

```cpp
template <typename Out, typename Answer>
static Answer resumption<Answer(Out)>::resume(Out cmdResult) &&;

// or
template <typename Answer>
static Answer resumption<Answer()>::resume() &&;
```

Resumptions in our library are one-shot, which means that you can resume each one at most once. This is not only a technical matter, but more importantly it is in accordance with RAII: objects are destructed during unwinding of the stack, and so in principle you don't want to unwind the same stack twice. This is somewhat enforced by the fact that `resumption` is movable, but not copyable. The handler is given the "ownership" of the resumption (the `resumption` class is actually a form of a smart pointer), but if we want to resume, we need to give up the ownership of the resumption. After this, the resumption becomes invalid, and so the user should not use it again.

## Other features of the library

### Invoking and handling with a label and reference

One can give a **label** to a handler and when invoking a command, which allows to pair a command with a handler more directly (otherwise, the pairing is done via runtime type information). This is a form of dynamic "instances" of effects. For example:

```cpp
void foo()
{
  int64_t lbl = eff::fresh_label();
  eff::handle<SomeHandler>(lbl, []() {
    eff::invoke_command(lbl, SomeCmd{});
  });
}
```

If one knows the exact type of the handler when invoking a command, they can supply it, which will make the invoke more efficient:

```cpp
eff::static_invoke_command<SomeHandler>(lbl, SomeCmd{});
```

A more efficient, albeit more dangerous, is to keep a **reference** to the handler, which will be valid as long as the handler is on the stack (even if it was moved around on the stack, or moved to a resumption and back on the stack).

```cpp
void foo()
{
  eff::handle_Ref<SomeHandler>([](auto href) {
    eff::invoke_command(href, SomeCmd{});    // <---+
    eff::handle<SomeHandler>([]() {          //     |
      eff::invoke_command(href, SomeCmd{});  // <---+
    });                                      //     |
  });                                        //     These two refer to
}                                            //     the same handler
```

However, if the handler is not on the stack, the program might behave unpredictably with no warning.

### Other forms of handlers and clauses

Our library offers additional forms of handlers and clauses, which lead to better readability and better performance. For example, `flat_handler` implements handlers in which the return clause is identity, which is useful for polymorphic handlers in which the answer type can be `void`. Moreover, the respective clauses for `Put` and `Get` in the example above are self- and tail-resumptive, which means that they resume the same continuation that they obtain as an argument ("self-") and that they return the result of resuming ("tail-"). Such clauses can be simplified using the `plain` modifier as follows:

```cpp
#include "cpp-effects/clause-modifiers.h"

// ...

template <typename Answer>
class HStateful : public eff::flat_handler<Answer, eff::plain<Put>, eff::plain<Get>> {
public:
  HStateful(int initialState) : state(initialState) { }
private:
  int state;
  void handle_command(Put p) final override  // no explicit resumption
  {
    state = p.newState;
  }
  int handle_command(Get) final override     // ...same here
  {
    return state;
  }
};
```
