# class `flat_handler`

Base class for [handlers](refman-handler.md) in which the return clause is identity. It is useful for handlers that are generic in the answer type, and spares the programmer writing a separate specialisation for when the answer type is `void`.

```cpp
template <typename Answer, typename... Cmds>
class flat_handler : public handler<Answer, Answer, Cmds...> {
  Answer handle_return(Answer a) final override { return a; }
};

template <typename... Cmds>
class flat_handler<void, Cmds...> : public handler<void, void, Cmds...> {
  void handle_return() final override { }
};
```

- `typename Answer` - The overall answer type of a derived handler and the type of the handled computation. Should be at least move-constructible and move-assignable.

- `typename... Cmds` - The commands that are handled by this handler.


The `handle_return` functions are overridden as:

```cpp
template <typename Answer, typename... Cmds>
Answer flat_handler<Answer, Cmds...>::handle_Return(Answer a)
{
  return a;
}

template <typename... Cmds>
void flat_handler<void, Cmds...>::handle_return() { }
```

<details>
  <summary><strong>Example</strong></summary>

Consider the following tick handler that is generic in the return type:

```cpp
struct Tick : command<int> { };

template <typename T>
class Counter : public flat_handler <T, plain<Tick>> {
  int counter = 0;
  int handle_command(Tick) final override
  {
    return ++counter;
  }
};
```

With the `Handler` class we would have to provide a separate specialisation for `void`:

```cpp
struct Tick : command<int> { };

template <typename T>
class Counter : public handler<T, plain<Tick>> {
  int counter = 0;
  int handle_command(Tick) final override
  {
    return ++counter;
  }
  T handle_return(T a)
  {
    return a;
  }
};

template <>
class Counter<void> : public handler<void, plain<Tick>> {
  int counter = 0;
  int handle_command(Tick) final override
  {
    return ++counter;
  }
  void handle_return() { }
};
```

</details>
