# function `handle`

[<< Back to reference manual](refman.md)

```cpp
template <typename H, typename... Args>
typename H::answer_type handle(
    int64_t label, std::function<typename H::body_type()> body, Args&&... args);

template <typename H, typename... Args>
typename H::answer_type handle(std::function<typename H::body_type()> body, Args&&... args);
```

Create a new [handler](refman-handler.md) of type `H` and use it to handle the computation `body`.

- `typename H` - The type of the handler that is used to handle `body`.

- `typename... Args` - Arguments supplied to the constructor of `H`.

- `int64_t label` - Explicit label of the handler. If no label is given, this handler is used based on the types of the commandss of `H` (the innermost handler that handles the invoked command is used).

- `std::function<typename H::body_type()> body` - The handled computation.

- **Return value** `H::answer_type` - The final answer of the handler, returned by one of the overloads of `H::handle_command` or `H::handle_return`.

Note: `handle<H>(b)` is equivalent to `handle_with(b, std::make_shared<H>())`.
