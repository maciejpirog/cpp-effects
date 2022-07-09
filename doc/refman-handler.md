# class `handler`

[<< Back to reference manual](refman.md)

Base class for handlers.

```cpp
template <typename Answer, typename Body, typename... Cmds>
class handler
{
public:
  using answer_type = Answer;
  using body_type = Body;
  virtual void debug_print() const;
protected:
  // For each Cmd in Cmds...
  virtual Answer handle_command(
      Cmd, resumption<typename Cmd::template resumption_type<Answer>>) = 0;

  virtual Answer handle_return(Body b) = 0;
};

// Specialisation for Body = void:

template <typename Answer, typename... Cmds>
class handler<Answer, void, Cmds...>
{
public:
  using answer_type = Answer;
  using body_type = void;
  virtual void debug_print() const;
protected:
  // For each Cmd in Cmds...
  virtual Answer handle_command(
      Cmd, resumption<typename Cmd::template resumption_type<Answer>>) = 0;

  virtual Answer handle_return() = 0;
};

```

- `typename Answer` - The overall answer type of a derived handler. Should be at least move-constructible and move-assignable.

- `typename Body` - The type of the handled computation. Should be at least move-constructible and move-assignable.

- `typename... Cmds` - The commands that are handled by this handler.

To define a handler for a set of commands, one needs to derive from `handler` and specify the command clauses and the return clause.

### :large_orange_diamond: handler<Answer, Body, Cmds...>::answer_type

```cpp
using answer_type = Answer;
```

Reveals the type of the overall answer of a handler.


### :large_orange_diamond: handler<Answer, Body, Cmds...>::body_type

```cpp
using body_type = Body;
```

Reveals the type of the handled computation.


### :large_orange_diamond: handler<Answer, Body, Cmds...>::handle_command

```cpp
virtual Answer command_clause(Cmd, resumption<typename Cmd::template resumption_type<Answer>>) = 0;
```

A handler handles a particular command `Cmd` if it inherits from `handler<..., Cmd, ...>` and overrides the overload of the member function `handle_command(Cmd, ...)` with the interpretation of the command.

- `Cmd c` - The handled command. The right clause is chosen via the overloading resolution mechanism out of the overloads of `handle_command` in the handler based on the type `Cmd`.

- `resumption<typename Cmd::template resumptionType<Answer>> r` - The captured resumption. When the user invokes a command, the control goes back to the handler and the computation delimited by the handler is captured as the resumption `r`. Since resumptions are one-shot, when we resume `r`, we have to give up the ownership of `r`, which makes `r` invalid.

- **return value** `Answer` - The overall result of handling the computation.

:information_source: Technically, `handle_command` is a virtual member function of `cpp_effects_internals::command_clause<Answer, Cmd>`, from which `handler` inherits for each `Cmd`. 

### :large_orange_diamond: handler<Answer, Body, Cmds...>::handle_return

```cpp
virtual Answer handle_return(Body b);  // if Body != void
virtual Answer handle_return();        // if Body == void
```

If a handled computation or resumed resumption gives a result without invoking a command, `handle_return` specifies the overall answer of the handler.

- `Body b` - the value that is the result of the handled computation.

- **return value** `Answer` - The overall value returned from handling of a computation.
