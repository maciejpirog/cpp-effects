# function `invoke_command`

[<< Back to reference manual](refman.md)

```cpp
template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(int64_t goto_handler, const Cmd& cmd);

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(const Cmd& cmd);

template <typename H, typename Cmd>
typename Cmd::out_type static_invoke_command(handler_ref it, const Cmd& cmd);
```

Used in a handled computation to invoke a particular command (similar to [`invoke_command`](refman-invoke_command.md)), but the handler with the given label is statically cast to `H`. The current computation (up  to and including the appropriate handler) is suspended, captured in a resumption, and the control goes to the handler.

- `typename H` - The type of the handler used to handled the command.

- `typename Cmd` - The type of the invoked command.

- `int64_t label` - The label of the handler to which the control should go. If there is no handler with label `label`, the program exits with exit code `-1`. If the inner-most handler with label `label` is not of type (derived from) `H`, it results in undefined behaviour. If `label` is note supplied, the inner-most handler is used.

- `handler_ref href` - A reference to the handler to which the control should go. See the documentation for [`handler_ref`](refman-handler_ref.md).

- `const Cmd& cmd` - The invoked command.

The point of `static_invoke_commnad` is that we often know upfront which handler will be used for a particular command. This way, instead of dynamically checking if a given handler is able to handle the invoke command, we can statically cast it to an appropriate handler `H`, trading dynamic type safety for performance.

- **Return value** `Cmd::out_type` - the value with which the suspended computation is resumed.

