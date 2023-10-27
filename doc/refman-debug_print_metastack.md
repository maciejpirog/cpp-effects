# function `debug_print_metastack`

[<< Back to reference manual](refman.md)

```cpp
void debug_print_metastack();
```

Prints out the current stack of handlers. Useful for "printf" debugging. The format of each frame is:

```
label:type_name[cmd]...[cmd]
```

where

- `label` is the label of the handler (automatically generated or given by the user when calling `handle` or `wrap`)

- `type_name` is the `typeid()::name` of the handler class,

- `[cmd]` is the list of `typeid()::name`s of the commads handled by the handler.

The stack grows bottom-to-top, while the bottom frame is always a dummy "global" handler with label `0`.

### Example

```cpp
struct Error : command<> { };

class ErrorHandler : public flat_handler<void, no_resume<Error>> {
  void handle_command(Error) override { }
};

template <typename S>
struct Put : command<> {
  S newState;
};

template <typename S>
struct Get : command<S> { };

template <typename Answer, typename S>
class Stateful : public flat_handler<Answer, plain<Put<S>>, plain<Get<S>>> {
public:
  Stateful(S initialState) : state(initialState) { }
private:
  S state;
  void handle_command(Put<S> p) final override
  {
    state = p.newState;
  }
  S handle_command(Get<S>) final override
  {
    return state;
  }
};

int main()
{
  handle<ErrorHandler>(100, []() {
    handle<ErrorHandler>(200, []() {
      handle<Stateful<void, int>>([]() {
        debug_print_metastack();
      }, 0);
    });
  });
}
```

Example output:

```
-2:8StatefulIviE[N10cpp_effects5plainI3PutIiEEE][N10cpp_effects5plainI3GetIiEEE]
200:12ErrorHandler[5Error]
100:12ErrorHandler[5Error]
0:N10cpp_effects9metaframeE
```
