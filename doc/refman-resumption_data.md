# classes `resumption_data` and `resumption_base`

[<< Back to reference manual](refman.md)

Classes that represents "bare" captured continuations that are not memory-managed by the library.

```cpp
class resumption_base {
public:
  virtual ~resumption_base() { }
};

template <typename Out, typename Answer>
class resumption_data : public resumption_base { };
```

- `typename Out` - In a proper resumption, the output type of the command that suspended the computation. In a plain resumption, the input type of the lifted function. Can be `void`.

- `typename Answer` - The return type of the suspended computation.
