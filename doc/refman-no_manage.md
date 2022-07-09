# class `no_manage` (clause modifier)

[<< Back to reference manual](refman.md)

Specialisation for handlers that either:

- Don't expose the resumption (i.e., all resumes happen within command
  clauses),

- Don't access the handler object after resume,

which amounts to almost all practical use-cases of handlers. A
`no_manage` clause does not participate in the reference-counting
memory management of handlers, saving a tiny bit of performance.

```cpp
template <typename Cmd>
struct no_manage { };
```

Usage:

```cpp
struct MyCmd : command<int> { };

class MyHandler : flat_handler<bool, no_manage<MyCmd>> {
  virtual bool handle_command(MyCmd, resumption<int> r) override
  {
    std::move(r).resume(3);
	return 3;
  }
}
```

The `handle_command`function is the same as in a usual handler, but it is not memory-managed.

:information_source: [`plain`](refman-plain.md) and [`no_resume`](refman-no_resume.md) clauses are automatically `no_manage`.
