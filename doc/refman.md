# cpp-effects: Reference Manual

## Introduction

:rocket: [Quick intro](quick-intro.md) (and comparison with effectful functional programming) - A brief overview of the API of the library. It assumes that the reader has some basic understanding of algebraic effects and effect handlers.


## Structure of the library

**Namespace:** `cpp_effects`

:memo: [`cpp-effects/cpp-effects.h`](../include/cpp-effects/cpp-effects.h) - The core of the library:

- class [`command`](refman-command.md) - Classes derived from `command` define commands.

- class [`flat_handler`](refman-flat_handler.md) - Version of `handler` for generic handlers with identity return clause.

- class [`handler`](refman-handler.md) - Classes derived from `handler` define handlers.

- type [`handler_ref`](refman-handler_ref.md) - Abstract reference to an active handler.

- class [`resumption`](refman-resumption.md) - Suspended computation, given to the user in command clauses of a handler.

- classes [`resumption_data`](refman-resumption_data.md) and [`resumption_base`](refman-resumption_data.md) - "Bare" captured continuations that are not memory-managed by the library.

- namespace `cpp_effects_internal` - Details of the implementation, exposed for experimentation.

- functions:

  * [`debug_print_metastack`](refman-debug_print_metastack.md) - Prints out the current stack of handlers. Useful for "printf" debugging.
  
  * [`fresh_label`](refman-fresh_label.md) - Generates a unique label that identifies a handler.
  
  * [`handle`](refman-handle.md) - Creates a new handler object and uses it to handle a computation.
  
  * [`handle_ref`](refman-handle_ref.md) - Similar to `handle`, but reveals a reference to the handler.
  
  * [`handle_with`](refman-handle_with.md) - Handles a computation using a given handler object.
  
  * [`handle_with_ref`](refman-handle_with_ref.md) - Handles a computation using a particular handler object and reveals a reference to the handler.
  
  * [`wrap`](refman-wrap.md) - Lifts a function to a resumption handled by a new handler object.
  
  * [`wrap_with`](refman-wrap_with.md) - Lifts a function to a resumption handled by a given handler object.
  
  * [`invoke_command`](refman-invoke_command.md) - Used in a handled computation to invoke a particular command, suspend the computation, and transfer control to the handler.
  
  * [`static_invoke_command`](refman-static_invoke_command.md) - Similar to `invoke_commad`, but explicitly gives the type of the handler object (not type-safe, but more efficient).

:memo: [`cpp-effects/clause-modifiers.h`](../include/cpp-effects/clause-modifiers.h) - Modifiers that force specific shapes or properties of command clauses in handlers:

- [`no_manage`](refman-no_manage.md) - Command clause that does not memory-manage the handler.

- [`no_resume`](refman-no_resume.md) - Command clause that does not use its resumptions.

- [`plain`](refman-plain.md) - Command clause that interprets a command as a function (i.e., a self- and tail-resumptive clause).
