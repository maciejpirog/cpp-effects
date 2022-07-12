# type `handler_ref`

[<< Back to reference manual](refman.md)

Abstract reference to an active handler.

```cpp
using handler_ref = ...;
```

A `handler_ref` is an abstract reference to an active
[handler](refman-handler.md), which can be used to tie the handler to
a [command](refman-command.md), without the need to look up the
handler every time we use [`invoke_command`](refman-invoke_command.md)
or [`static_invoke_command`](refman-static_invoke_command.md).

:bangbang: Handler references are valid if the referenced handler is
active. Invoking a command with a reference to a handler of a
computation that has already ended or is captured in a resumption will
cause undefined behaviour.

A handler reference should not be confused with:

- A reference to an object of a class that inherits from
  [`handler`](refman-handler.md), although it is not far from the
  truth: `handler_ref` can be thought of as a form of an iterator that
  points to an object, not a reference to the object itself.

- A label, since labels are not necessarily unique, and invoking a
  command with a label still involves looking up a handler with the
  given label on the stack.

<details>
  <summary><strong>Example</strong></summary>
  
```cpp
using namespace cpp_effects;

struct Ask : command<int> { };
struct AddOneMoreHandler : command<> { };

template <typename T>
class Reader : public flat_handler<T, Ask, AddOneMoreHandler> {
public:
  Reader(int val) : val(val) { }
private:
  int val;
  T handle_command(Ask, resumption<T(int)> r) override
  {
    return std::move(r).Resume(val);
  }
  T handle_command(AddOneMoreHandler, resumption<T()> r) override
  {
    return handle<Reader<void>>(300, [r = r.release()]() {
      resumption res(r);
      std::move(res).resume();
    }, 300);
  }
};

int main()
{
  handle_ref<Reader<void>>(100, [](auto href) {
    debug_print_metastack();
    std::cout << invoke_command(href, Ask{}) << std::endl;
    invoke_command(AddOneMoreHandler{});
    handle<Reader<void>>([=]() {
      debug_print_metastack();
      std::cout << invoke_command(href, Ask{}) << std::endl;
      return 0;
    }, 200);
  }, 100);
}
```

Example output:

```
100:6ReaderIvE[3Ask][17AddOneMoreHandler]
0:N10CppEffects9MetaframeE
100
-2:6ReaderIvE[3Ask][17AddOneMoreHandler]
100:6ReaderIvE[3Ask][17AddOneMoreHandler]
300:6ReaderIvE[3Ask][17AddOneMoreHandler]
0:N10CppEffects9MetaframeE
100
```

</details>
