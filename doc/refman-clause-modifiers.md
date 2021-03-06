# cpp-effects/clause-modifiers.h

[<< Back to reference manual](refman.md)

This file contains modifiers that force specific shapes of command clauses in handlers, which is useful for readability (e.g., we can read from the type that we will not use the resumption) and performance (e.g., we don't allocate resumptions that are never used).

Each modifier consists of a template, which marks a command in the type of the handler, and a specialisation of the [`CmdClause`](refman-cpp-effects.md#class-cmdclause) class that modifies the type of the clause in the definition of the handler. For example, we can use the `NoResume` modifier to specify that a particular handler will not need the resumption:

```cpp
struct MyCmd : Command<int> { };
struct OtherCmd : Command<> { };

class MyHandler : public Handler <char, void, NoResume<MyCmd>, OtherCmd> {
  ...
  char CommandClause(OtherCmd, Resumption<char()>) { ... }  // regular clause
  char CommandClause(MyCmd) { ... }                         // no resumption
};
```

Because we used the `NoResume` modifier, the type of `CommandClause` for `MyCmd` is now different.


## `NoResume` modifier

Specialisation for command clauses that do not use the resumption. This is useful for handlers that behave like exception handlers or terminate the "current thread".

```cpp
template <typename Cmd>
struct NoResume { };

template <typename Answer, typename Cmd>
class CmdClause<Answer, NoResume<Cmd>> {
protected:
  virtual Answer CommandClause(Cmd) = 0;
};
```

The command clause does not receive the current resumption, but it is allowed to resume any other resumption inside its body.

<details>
  <summary><strong>Example</strong></summary>

```cpp
struct Error : Command<> { };

class Cancel : public Handler<void, void, NoResume<Error>> {
  void CommandClause(Error) override // no "resumption" argument
  {
    std::cout << "Error!" << std::endl;
  }
  void ReturnClause() override {}
};

int main()
{
  std::cout << "Welcome!" << std::endl;
  OneShot::Handle<Cancel>([](){
    std::cout << "So far so good..." << std::endl;
    OneShot::InvokeCmd(Error{}); 
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

</details>


## `Plain` modifier

Specialisation for plain clauses, which interpret commands as functions (i.e., they are self- and tail-resumptive). No context switching, no explicit resumption.

```cpp
template <typename Cmd>
struct Plain { };

template <typename Answer, typename Cmd>
class CmdClause<Answer, Plain<Cmd>> {
protected:
  virtual typename Cmd::OutType CommandClause(Cmd) = 0;
};
```

Note that the return type of the command clause is now the return type of the command.

<details>
  <summary><strong>Example</strong></summary>

```cpp
struct Add : Command<int> {
  int x, y;
};

class Calculator : public Handler <void, void, Plain<Add>> {
  int CommandClause(Add c) override  // - no "resumption" argument
                                     // - return type that of the command, not handler
  {
    return c.x + c.y;
  }
  void ReturnClause() override { }
};

int main()
{
  OneShot::Handle<Calculator>([](){
    std::cout << "2 + 5 = " << OneShot::InvokeCmd<Add>({{}, 2, 5}) << std::endl;
  });
}
```

Output:

```
2 + 5 = 7
```

</details>


## `NoManage` modifier

