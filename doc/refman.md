# cpp-effects: Reference Manual

## Structure of the library

**Namespace:** `CppEffects`

:memo: [`cpp-effects/cpp-effects.h`](refman-cpp-effects.md): The core of the library.

- [`Command`](refman-cpp-effects.md#class-command) - Classes derived from `Command` define commands.

- [`Handler`](refman-cpp-effects.md#class-handler) - Classes derived from `Handler` define handlers.

- [`FlatHandler`](refman-cpp-effects.md#class-flathandler) - Specialisation of `Handler` for generic handlers with identity return clause.

- [`Resumption`](refman-cpp-effects.md#classes-resumptionbase-and-resumption) - The class for suspended computations that are given to the user in handlers' command clauses.

- [`OneShot`](refman-cpp-effects.md#class-oneshot) - The actual interface (via static functions) of handling computations and invoking commands.

:memo: [`cpp-effects/clause-modifiers.h`](refman-clause-modifiers.md): Modifiers that force specific shapes of command clauses in handlers.

- [`NoResume`](refman-clause-modifiers.md#noresume-modifier) - Define command clauses that do not use their resumptions.

- [`Plain`](refman-clause-modifiers.md#plain-modifier) - Define command clauses that interpret commands as functions (i.e., clauses that are self- and tail-resumptive).

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
  return OneShot::InvokeCmd(Get{});
}
```

The relevant declarations are:

```cpp
template <typename Out>
struct Command {
  using OutType = Out;
};

template <typename Cmd>
static typename Cmd::OutType OneShot::InvokeCmd(const Cmd&);
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
};
```

To handle a computation, one needs to create a new object of the derived class:

```cpp
auto body = [](){ return get(put(get() + 1)); } 
OneShot::HandleWith(body, std::make_shared<State<int>>(10)); // returns 11
```

The lifetime of this object is managed by the library: it is deleted when the handled computation returns a value or a resumption that contains this handler is discontinued (e.g. is not resumed in a command clause).
