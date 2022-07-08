# cpp-effects: Reference Manual

## Introduction

:boom: [Quick intro](quick-intro.md) (and comparison with effectful functional programming) - A brief overview of the API of the library. It assumes that the reader has some basic understanding of algebraic effects and effect handlers.


## Structure of the library

**Namespace:** `cpp_effects`

:memo: [`cpp-effects/cpp-effects.h`](refman-cpp-effects.md) - The core of the library:

- [`class command`](refman-cpp-effects.md#class-command) - Classes derived from `command` define commands.

- [`class flat_handler`](refman-cpp-effects.md#class-flathandler) - Specialisation of `handler` for generic handlers with identity return clause.

- [`class handler`](refman-cpp-effects.md#class-handler) - Classes derived from `handler` define handlers.

- [`typename handler_ref`](refman-cpp-effects.md#class-handlerref) - Abstract reference to an active handler.

- [`class resumption`](refman-cpp-effects.md#class-resumption) - Suspended computation, given to the user in handlers' command clauses.

- [`class resumption_data` and `class resumption_base`](refman-cpp-effects.md#classes-resumptiondata-and-resumptionbase) - "Bare" captured continuations that are not memory-managed by the library.

- `namespace cpp_effects_internal` - Details of the implementation, exposed for experimentation.

- Functions:

  * `debug_print_metastack` - Print out the current stack of handlers. Useful for "printf" debugging.
  
  * `fresh_label` - Generates a unique label that identifies a handler.
  
  * `handle` - Creates a new handler object and uses it to handle a computation.
  
  * `handle_ref` - Similar to `handle`, but reveals a reference to the handler.
  
  * `handle_with` - Handles a computation using a given handler object.
  
  * `handle_with_ref` - Handles a computation using a particular handler object and reveals a reference to the handler.
  
  * `wrap` - Lifts a function to a resumption handled by a new handler object.
  
  * `wrap_with` - Lifts a function to a resumption handled by a given handler object.
  
  * `invoke_command` - Used in a handled computation to invoke a particular command, suspend the computation, and transfer control to the handler.
  
  * `static_invoke_command` - Similar to `invoke_commad`, but explicitly gives the type of the handler object (not type-safe, but more efficient).

:memo: [`cpp-effects/clause-modifiers.h`](refman-clause-modifiers.md) - Modifiers that force specific shapes of command clauses in handlers:

- [`no_manage`](refman-clause-modifiers.md#nomanage-modifier) - Command clause that does not memory-manage the handler.

- [`no_resume`](refman-clause-modifiers.md#noresume-modifier) - Command clause that does not use its resumptions.

- [`plain`](refman-clause-modifiers.md#plain-modifier) - Command clause that interprets a command as a function (i.e., a self- and tail-resumptive clause).
