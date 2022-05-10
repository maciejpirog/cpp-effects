# Quick Introduction (and Comparison with Effectful Functional Programming)

[<< Back to reference manual](refman.md)

This document gives a quick overview of the API of the library. It assumes that the reader has some basic understanding of algebraic effects and effect handlers.

Effect handlers first appeared in the area of functional programming (e.g., Eff, Koka, Multicore OCaml). There are, however, some differences between how handlers are usually formulated in FP and what our library provides, trying to combine effects with object-oriented programming - and C++ programming in particular.

## Commands

In our library, *commands* (often known as *operations*) are defined as classes derived from the `Command` template. The type argument of the template is the return type of the command. For example, we can formulate the usual interface for mutable state of type `int` as follows:

```cpp
struct Put : Command<void> { int newState; };
struct Get : Command<int> { };
```

Data members of these classes are arguments of the commands.

:point_right: There is nothing magical in `Command`. It is defined as follows:

```cpp
template <typename Out>
struct Command {
  using OutType = Out;
};
```

To *perform* (or: *invoke*) a command, we need to use an object of such a "command" class. The out type of the command becomes the result of the invocation:

```cpp
template <typename Cmd>
static typename Cmd::OutType OneShot::InvokeCmd(const Cmd&);
```

For example:

```cpp
void put(int v)
{
  OneShot::InvokeCmd(Put{{}, v});
}

int get()
{
  return OneShot::InvokeCmd(Get{});
}
```

:dromedary_camel: The equivalent code in Multicore OCaml is as follows (compare [ocaml-multicore/effects-examples/state.ml](https://github.com/ocaml-multicore/effects-examples/blob/master/state.ml)):

```
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
template <typename T> // the type of the handled computation
class State : public Handler<T, T, Put, Get> {
public:
  State(int initialState) : state(initialState) { }
private:
  int state;
  T CommandClause(Get, Resumption<int, T> r) override
  {
    return std::move(r).TailResume(state);
  }
  T CommandClause(Put p, Resumption<void, T> r) override
  {
    state = p.newState;
    return std::move(r).TailResume();
  }
  T ReturnClause(T v) override
  {
    return v;
  }
};
```

FP usually makes effect handlers syntactically quite similar to exception handlers. We take a different approach here: a handler can be defined by deriving from the `Handler` class. The user defines "command" and "return" clauses by overriding appropriate virtual functions in `Handler`, one for each command listed in the template parameter list.

We can handle a computation as follows:

```cpp
auto body = []() -> int {
  put(get() * 2);
  return get();
};
OneShot::Handle<State<int>>(body, 10); // returns 20
```

The `Handle` function will take care of creating the handler object for us, forwarding its subsequent arguments (`10`) to the constructor of `State`.

## Resumptions

A resumptions represents a suspended computation that "hangs" on a command. In FP, they can be functions, in which the type of the argument is the return type of the command. They can also be a separate type (as in, again, Eff or Multicore OCaml). In this library, they are a separate type:

```cpp
template <typename Out, typename Answer>
class Resumption;
```

We can *resume* (or *continue* as it is called in Multicore OCaml) a resumption using the `Resume` member function:

```cpp
template <typename Out, typename Answer>
static Answer Resumption<Out, Answer>::Resume(Out cmdResult) &&;

// specialisation for Out = void
template <typename Answer>
static Answer Resumption<void, Answer>::Resume() &&;
```

Resumptions in our library are one-shot, which means that you can resume each one at most once. This is not only a technical matter, but more importantly it is in accordance with RAII: objects are destructed during unwinding of the stack, and so in principle you don't want to unwind the same stack twice. This is somewhat enforced by the fact that `Resumption` is movable, but not copyable. The handler is given the "ownership" of the resumption (the `Resumption` class is actually a form of a smart pointer), but if we want to resume, we need to give up the ownership of the resumption. After this, the resumption becomes invalid, and so the user should not use it again.

## Other features of the library

### Invoking and handling with a label and reference

One can give a **label** to a handler and when invoking a command, which allows to pair a command with a handler more directly (otherwise, the pairing is done via runtime type information). This is a form of dynamic "instances" of effects. For example:

```cpp
void foo()
{
  int64_t lbl = OneShot::FreshLabel();
  OneShot::Handle<SomeHandler>(lbl, []() {
    OneShot::InvokeCmd(lbl, SomeCmd{});
  });
}
```

If one knows the exact type of the handler when invoking a command, they can supply it, which will make the invoke more efficient:

```cpp
OneShot::StaticInvokeCmd<SomeHandler>(lbl, SomeCmd{});
```

A more efficient, albeit more dangerous, is to keep a **reference** to the handler, which will be valid as long as the handler is on the stack (even if it was moved around on the stack, or moved to a resumption and back on the stack).

```cpp
void foo()
{
  OneShot::HandleRef<SomeHandler>([](auto href) {
    OneShot::InvokeCmd(href, SomeCmd{});    // <---+
    OneShot::Handle<SomeHandler>([]() {     //     |
      OneShot::InvokeCmd(href, SomeCmd{});  // <---+
    });                                     //     |
  });                                       //     These two refer to
}                                           //     the same handler
```

However, if the handler is not on the stack, the program might behave unpredictably with no warning.

### Other forms of handlers and clauses

Our library offers additional forms of handlers and clauses, which lead to better readability and better performance. For example, `FlatHandler` implements handlers in which the return clause is identity, which is useful for polymorphic handlers in which the answer type can be `void`. Moreover, the respective clauses for `Put` and `Get` in the example above are self- and tail-resumptive, which means that they resume the same continuation that they obtain as an argument ("self-") and that they return the result of resuming ("tail-"). Such clauses can be simplified using the `Plain` modifier as follows:

```cpp
template <typename Answer>
class HStateful : public FlatHandler<Answer, Plain<Put>, Plain<Get>> {
public:
  HStateful(int initialState) : state(initialState) { }
private:
  int state;
  void CommandClause(Put p) final override  // no explicit resumption
  {
    state = p.newState;
  }
  int CommandClause(Get) final override  // ...same here
  {
    return state;
  }
};
```
