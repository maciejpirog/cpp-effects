# function `handle_with`

[<< Back to reference manual](refman.md)

```cpp
template <typename H>
typename H::answer_type handle_with(
    int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

template <typename H>
typename H::answer_type handle_with(
    std::function<typename H::body_type()> body, std::shared_ptr<H> handler);
```

Handle the computation `body` using the given handler of type `H`.

- `typename H` - The type of the handler that is used to handle `body`.

- `int64_t label` - Explicit label given to `handler`. If no label is given, this handler is used based on the types of the commands of `H` (the innermost handler that handles the invoked command is used).

- `std::function<typename H::body_type()> body` - The handled computation.

- `std::shared_ptr<H> handler` - The handler used to handle `body`.

- **Return value** `H::answer_type` - The final answer of the handler, returned by one of the overloads of `H::handle_command` or `H::handle_return`.
