# class `plain` (clause modifier)

[<< Back to reference manual](refman.md)

A clause modifier used for `handle_command` functions that interpret commands as functions (i.e., they are self- and tail-resumptive). No context switching, no explicit resumption.

```cpp
template <typename Cmd>
struct plain { };
```

Usage:

```cpp
struct Add : command<int> {
  int x, y;
};

class Calculator : public flat_handler<void, plain<Add>> {
  int handle_command(Add c) override  // - no "resumption" argument
                                      // - return type that of the command, not handler
  {
    return c.x + c.y;
  }
};

int main()
{
  handle<Calculator>([](){
    std::cout << "2 + 5 = " << invoke_command<Add>({{}, 2, 5}) << std::endl;
  });
}
```

Output:

```
2 + 5 = 7
```
