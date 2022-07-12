# function `fresh_label`

[<< Back to reference manual](refman.md)

```cpp
int64_t fresh_label();
```

When a command is invoked, the handler is chosen based on the type of the command using the usual "innermost" rule. However, one can use labels to directly match commands with handlers. The function `fresh_label` generates a unique label, which can be used later with the overloads with `label` arguments.

- **Return value** `int64_t` - The generated label.

