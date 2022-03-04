# Quick Introduction (and Comparison with Effectful Functional Programming)

[<< Back to reference manual](refman.md)

This document gives a quick overview of the API of the library. It assumes that the reader has some basic understanding of algebraic effects and effect handlers.

Effect handlers first appeared in the area of functional programming (e.g., Eff, Koka, Multicore OCaml). There are, however, some differences between how handlers are usually formulated in FP and what our library provides, trying to combine effects with object-oriented programming - and C++ programming in particular.

## Commands

In our library, *commands* (aka *operations*) are defined as classes derived from the `Command` template. The type argument of the template is the return (*out*, as we call it here) type of the command. For example, we can formulate the usual interface for mutable state as follows:

```cpp
struct Put : Command<void> { int newState; };
struct Get : Command<int> { };
```

Note that arguments of commands are data members of these classes.

:point_right: In case you wonder, there is nothing magical in `Command`, it is defined as follows:

```cpp
template <typename Out>
struct Command {
  using OutType = Out;
};
```

To perform (*invoke*) a command, we need to use an object of such a "command" class. The out type of the command becomes the result of the invocation:

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

:dromedary_camel: To compare, the equivalent code in Multicore OCaml is as follows (compare [ocaml-multicore/effects-examples/state.ml](https://github.com/ocaml-multicore/effects-examples/blob/master/state.ml)):

```
(* definition of operations *)
effect Put : int -> unit
effect Get : int

(* functional interface *)
let put v = perform (Put v)
let get () = perform Get
```

## Handlers

In general, a *handler* is a piece of code that knows what to do with a set of commands. One can subsequently *handle* a computation by delimiting it using the handler. If in this computation an operation is invoked, the handler assumes control, receiving the command together with the computation captured as a *resumption*.

In our library, we take advantage of the basic principle of object-oriented programming, that is, packing together data and code. And so, a handler is a class, which contains member functions that know how to handle particular commands, but it can also encapsulate as much data and auxiliary definitions as the programmer sees fit. Then, one handles a computation using a particular *object* of that class. This object lives as long as the computation (actually, even longer), providing a persistent "context of execution" of the handled computation. It is self-explanatory if we implement a handler for state using a data member of the handler:

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
};
```

The most popular approach in FP makes effect handlers syntactically quite similar to exception handlers. It is quite different here: a handler can be defined by deriving from the `Handler` class. The definitions of "command" and "return" clauses are defined by overriding appropriate virtual functions in `Handler`, one for each command listed in the template parameter list.

We can handle a computation as follows:

To handle a computation, one needs to create a new object of the derived class:

```cpp
auto body = []() -> int { return get(put(get() * 2)); };
OneShot::Handle<State>(body, 10); // returns 20
```

Note that the `Handle` function will take care of creating the handler object for us, forwarding its second argument, `10`, to the constructor of `State`. The lifetime of this object is managed by the library, and the programmer does not have to worry about its lifetime.


## Resumptions

Resumptions represent suspended computations that "hang" on a command. In FP, they sometimes are simply functions, in which the type of the argument is the return type of the command. They can also be a separate type (as in, again, Eff or Multicore OCaml). In this library, they are a separate type:

```cpp
template <typename Out, typename Answer>
class Resumption;
```

We can *resume* (or "continue" as it is called in Multicore OCaml) a resumption using `OneShot::Resume`:

```cpp
template <typename Out, typename Answer>
static Answer OneShot::Resume(std::unique_ptr<Resumption<Out, Answer>> r, Out cmdResult);

// specialisation for Out = void
template <typename Answer>
static Answer OneShot::Resume(std::unique_ptr<Resumption<void, Answer>> r);
```

Resumptions in our library are one-shot, which means that you can resume each resumption only once. This is not only a technical matter, but more importantly it is in accordance with the C++ idiom RAII, which means that objects are destructed during unwinding of the stack, and so in principle you don't want to unwind the same stack twice. This is somewhat enforced by the use of `unique_ptr` - the `CommandClause` in the handler is given the "ownership" of the resumption in the form of a unique pointer, but if we want to resume, we needs to transfer the ownership of the resumption to the class `OneShot`. After this, the pointer becomes invalid, and so the user should not use it again.

## Other features of the library

Our library offers additional forms of handlers and clauses, which lead to better readability and better performance. For example, we can use  the `FlatHandler` class to indicate that the return clause is always identity. Moreover, the respective clauses for `Put` and `Get`, are self- and tail-resumptive, which means that they resume the same continuation that they obtain as an argument ("self-") and that they return the result of resuming ("tail-"). Such clauses can be simplified using the `Plain` modifier as follows:

```cpp
template <typename S>
struct Put : Command<void> {
  S newState;
};

template <typename S>
struct Get : Command<S> { };

template <typename S>
void put(S s) {
  OneShot::InvokeCmd(Put<S>{{}, s});
}

template <typename S>
S get() {
  return OneShot::InvokeCmd(Get<S>{});
}

template <typename Answer, typename S>
class HStateful : public FlatHandler<Answer, Plain<Put<S>>, Plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
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
```
