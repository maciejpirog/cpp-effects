# class `no_resume` (clause modifier)

[<< Back to reference manual](refman.md)

The `no_resume` modifier indicates that a particular implementation of `handle_command` in a handler will not need the resumption.

```cpp
template <typename Cmd>
struct no_resume { };
```

Usage:

```cpp
struct Error : command<> { };

class Cancel : public flat_handler<void, no_resume<Error>> {
  void handle_command(Error) override  // no "resumption" argument
  {
    std::cout << "Error!" << std::endl;
  }
};

int main()
{
  std::cout << "Welcome!" << std::endl;
  handle<Cancel>([](){
    std::cout << "So far so good..." << std::endl;
    invoke_command(Error{}); 
    std::cout << "I made it!" << std::endl;
  });
  std::cout << "Bye!" << std::endl;
}
```

Output:

```
Welcome!
So far so good...
Error!
Bye!
```
