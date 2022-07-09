# function `wrap`

[<< Back to reference manual](refman.md)

```cpp
template <typename H, typename... Args>
resumption<typename H::answer_type()> wrap(
    int64_t label, std::function<typename H::body_type()> body, Args&&... args);

template <typename H, typename A, typename... Args>
resumption<typename H::answer_type(A)> wrap(
    int64_t label, std::function<typename H::body_type(A)> body, Args&&... args);

template <typename H, typename... Args>
resumption<typename H::answer_type()> wrap(
    std::function<typename H::body_type()> body, Args&&... args);

template <typename H, typename A, typename... Args>
resumption<typename H::answer_type(A)> wrap(
    std::function<typename H::body_type(A)> body, Args&&... args);
```

Wraps a computation in a handler, but doesn't execute it. Instead, the computation together with a handler are returned as a suspended computation (= [resumption](refman-resumption.md)).

- `typename H` - The type of the handler that is used to handle `body`.

- `typename... Args` - Arguments supplied to the constructor of `H`.

- `int64_t label` - Explicit label of the handler. If no label is given, this handler is used based on the types of the commandss of `H` (the innermost handler that handles the invoked command is used).

- `std::function<typename H::body_type()> body`, `std::function<typename H::body_type(A)> body`- The wrapped function.

- **Return value** `resumption<typename H::answer_type(A)>` - The resumption that corresponds to `body` wrapped in a handler `H`.

Semantically, for a function `std::function<T()> foo`, the expression

```cpp
wrap<H>(foo)
```

is equivalent to

```cpp
resumption<T()>([=](){ return handle<H>(foo); })
```

If the function has an argument, it becomes the `Out` type of the resumption. That is, for a function `std::function<T(A)> foo`, the expression

```cpp
wrap<H>(foo)
```

is equivalent to

```cpp
resumption<T(A)>([=](A a){ return handle<H>(std::bind(foo, a)); })
```
