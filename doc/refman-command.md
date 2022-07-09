# class `command`

[<< Back to reference manual](refman.md)

The base class for commands.

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

A class derived from `command` can be used as a command if it is at least copy-constructible.

**Type parameters:**

- `typename... Outs` - The return type of invoking the (derived) command (i.e., the return type of `invoke_command`). Should be at least move-constructible and move-assignable. Leave empty if the command should not return any value (in such case, the return type of `invoke_command` is `void`).

### :large_orange_diamond: out_type

```cpp
using out_type = Out;   // in command<Out>
using out_type = void;  // in command<>
```

The type returned by `invoke_command` called with the (drived) command.

### :large_orange_diamond: resumption_type

```cpp
template <typename Answer> using resumption_type = Answer(Out);  // in command<Out>
template <typename Answer> using resumption_type = Answer();     // in command<>
```

The type parameter of the resumption that captures a computation suspended by invoking the (derived) command. The type of the resumption depends also on the overall answer type of the handler, hence `resumption_type` is a template.

Compare the type of `handle_command` for a command `Cmd`:

```cpp
Answer handle_command(Cmd, resumption<typename Cmd::template resumption_type<Answer>>)
```

For example, for `Cmd : command<int>`, the type of the corresponding `handle_command` in a class `MyHandler : handler<bool, ..., Cmd, ...>` is

```cpp
bool MyHandler::handle_command(Cmd, resumption<bool(int)>)
```
