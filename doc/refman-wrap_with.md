# function `wrap_with`

[<< Back to reference manual](refman.md)


```cpp
template <typename H>
resumption<typename H::answer_type()> wrap_with(
    int64_t label, std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

template <typename H, typename A>
resumption<typename H::answer_type()> wrap_with(
    int64_t label, std::function<typename H::body_type(A)> body, std::shared_ptr<H> handler);

template <typename H>
resumption<typename H::answer_type()> wrap_with(
    std::function<typename H::body_type()> body, std::shared_ptr<H> handler);

template <typename H, typename A>
resumption<typename H::answer_type()> wrap_with(
    std::function<typename H::body_type(A)> body, std::shared_ptr<H> handler);
```

Similar to [`wrap`](refman-wrap.md) but with a particular handler object.
