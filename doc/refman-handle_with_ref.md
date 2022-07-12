# function `handle_with_ref`

[<< Back to reference manual](refman.md)

:warning: This feature is experimental!

```cpp
template <typename H, typename... Args>
typename H::answer_type handle_ref(
    int64_t label, std::function<typename H::body_type(handler_ref)> body, Args&&... args);

template <typename H, typename... Args>
typename H::answer_type handle_ref(
    std::function<typename H::body_type(handler_ref)> body, Args&&... args);
```

Similar to [`handle_with`](refman-handle_with.md), but reveals the [handler reference](refman-handler_ref.md) to the installed handler as an argument to the body.
