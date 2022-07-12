# function `invoke_command`

[<< Back to reference manual](refman.md)

```cpp
template <typename Cmd>
typename Cmd::out_type invoke_command(int64_t goto_handler, const Cmd& cmd);

template <typename Cmd>
typename Cmd::out_type invoke_command(const Cmd& cmd);

template <typename Cmd>
typename Cmd::out_type invoke_command(handler_ref it, const Cmd& cmd);
```

Used in a handled computation to invoke a particular [command](refman-command.md). The current computation (up to and including the appropriate handler) is suspended, captured in a [resumption](refman-resumption.md), and the control goes to the handler.

- `typename Cmd` - The type of the invoked command.

- `int64_t label` - The label of the handler to which the control should go. If there is no handler with label `label` in the context or it does not handle the command `Cmd`, the program ends with exit code `-1`.

- `handler_ref href` - A reference to the handler to which the control should go. See the documentation for [`handler_ref`](refman-handler_ref.md).

- `const Cmd& cmd` - The invoked command.

- **Return value** `Cmd::out_type` - the value with which the suspended computation is resumed.

