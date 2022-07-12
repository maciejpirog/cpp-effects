# class `resumption`

[<< Back to reference manual](refman.md)

A resumption represents a suspended computation. A resumption is given to the user either as an argument of `handler<...>::handle_command`, or can be lifted from a function using a constructor or `wrap`.

```cpp
template <typename T>
class resumption;

template <typename Out, typename Answer>
class resumption<Answer(Out)> {
public:
  resumption();
  resumption(resumption_data<Out, Answer>* data);
  resumption(std::function<Answer(Out)>);
  resumption(const resumption<Answer(Out)>&) = delete;
  resumption(resumption<Answer(Out)>&& other);
  
  resumption& operator=(const resumption<Answer(Out)>&) = delete;
  resumption& operator=(resumption<Answer(Out)>&& other);
  
  ~resumption();
  
  explicit operator bool() const;
  bool operator!() const;
  resumption_data<Out, Answer>* release();
  Answer resume(Out cmdResult) &&;
  Answer tail_resume(Out cmdResult) &&;
};

template <typename Answer>
class resumption<Answer()> {
public:
  resumption();
  resumption(resumption_data<void, Answer>* data);
  resumption(std::function<Answer()>);
  resumption(const resumption<Answer()>&) = delete;
  resumption(resumption<Answer()>&& other);
  
  resumption& operator=(const resumption<Answer()>&) = delete;
  resumption& operator=(resumption<Answer()>&& other);
  
  ~resumption();

  explicit operator bool() const;
  bool operator!() const;
  resumption_data<void, Answer>* release();
  Answer resume() &&;
  Answer tail_resume() &&;
};

```

Objects of the `resumption` class are movable but not copyable. This is because they represent suspended **one-shot** continuations.

The `resumption` class is actually a form of a smart pointer, so moving it around is cheap.

**Type parameters:**

- `typename T` - A function type that corresponds to the type of the suspended computation.

**Specialisations:**

- `resumption<Answer()>` - A computations that, when resumed, will return a value of type `Answer`.

- `resumption<Answer(Out)>` - A computation that needs a value of type `Out` to be resumed (`Out` is usually the output type of the operation on which the computation is suspended), and will return a value of type `Answer`.

### :large_orange_diamond: resumption<T>::resumption

```cpp
/* 1 */ resumption<Answer(Out)>::resumption()
  
/* 2 */ resumption<Answer(Out)>::resumption(resumption_data<Out, Answer>* data)

/* 3 */ resumption<Answer(Out)>::resumption(std::function<Answer(Out)> func)

/* 4 */ resumption<Answer()>::resumption(std::function<Answer()> func)
```

Constructors.

- `1` - Create a trivial **invalid** resumption.

- `2` - Create a resumption from data previously released with `release`.

- `3` - Lift a function to a resumption.

- `4` - As above, specialisation for `T Answer()`.

Arguments:

- `resumption_data<Out, Answer>* data` - Data previously released with `release`.

- `std::function<Answer(Out)> func` - The lifted function.

- `std::function<Answer()> func` - The lifted function (specialisation for `T == Answer()`).


### :large_orange_diamond: resumption<T>::operator bool

Check if the resumption is valid. The resumption becomes invalid if moved elsewhere (in particular, when resumed).

```cpp
explicit operator bool() const;
```

- **return value** `bool` - Indicates if the resumption is valid.

### :large_orange_diamond: resumption<T>::operator!

Check if the resumption is invalid. The resumption becomes invalid if moved elsewhere (in particular, when resumed).

```cpp
bool operator!() const;
```

- **return value** `bool` - Indicates if the resumption is invalid.

### :large_orange_diamond: resumption<T>::release

```cpp
resumption_data<Out, Answer>* release();
```

releases the pointer to the suspended computation.

- **return value** [`resumption_data<Out, Answer>*`](refman-resumption_data.md) - The released pointer.

**Warning:** :warning: Never use `delete` on the released pointer! If you want to get rid of it safely, wrap it back in a dummy `resumption` value, and let its destructor do the job. For example:

```cpp
void foo(resumption<int()> r)
{
  auto ptr = r.release();
  // ...
  resumption<int()>{ptr};
}
```

### :large_orange_diamond: resumption<T>::resume

```cpp
Answer resumption<Answer(Out)>::resume(Out cmdResult) &&

Answer resumption<Answer()>::resume() &&
```

resume the suspended computation captured in the resumption.

- `Out cmdResult` - The value that is returned by the
  [command](refman-command.md) on which the resumption "hangs".

- **Return value** `Answer` - The result of the resumed computation.

### :large_orange_diamond: resumption<T>::tail_resume

```cpp
Answer resumption<Answer(Out)>::tail_resume(Out cmdResult) &&

Answer resumption<Answer()>::tail_resume() &&
```

Use to resume the suspended computation captured in the resumption in a tail position in the command clause. This is to be used **only inside a command clause** as the returned expression. Semantically, for an rvalue reference `r`, the expressions `return r.resume(...);` and `return r.tail_resume(...);` are semantically equivalent, but the latter does not build up the call stack.

- `Out cmdResult` - The value that is returned by the [command](refman-command.md) on which the resumption "hangs".

- **Return value** `Answer` - The result of the resumed computation.

Tail-resumes are useful in command clauses such as:

```cpp
int handle_command(SomeCommand, resumption<int()> r) override
{
  // do sth
  return std::move(r).tail_resume();
}
```

In this example, we will build up the call stack until the entire handler returns a final answer (resulting in a "stack leak"). The library has a separate trampolining mechanism built in to avoid this:

```cpp
  // do sth
  return std::move(r).tail_resume();
```

**NOTE:** `tail_resume` can be used only if `Answer` is trivially constructible. Consider the following command clause:

```cpp
class H : Handler<Answer, void, Op> {
  // ...
  Answer handle_command(Op, resumption<Answer()> r) override
  {
    return std::move(r).tail_resume();
  }
}
```

What happens behind the scenes is that `tail_resume` returns a trivial value of type `Answer`, while the real resuming happens in a trampoline hidden in the [`handle`](refman-handle.md) function.
