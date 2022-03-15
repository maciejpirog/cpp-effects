# cpp-effects: Reference Manual

## Introduction

:boom: [Quick intro](quick-intro.md) (and comparison with effectful functional programming) - A brief overview of the API of the library. It assumes that the reader has some basic understanding of algebraic effects and effect handlers.

## Structure of the library

**Namespace:** `CppEffects`

:memo: [`cpp-effects/cpp-effects.h`](refman-cpp-effects.md) - The core of the library:

- [`Command`](refman-cpp-effects.md#class-command) - Classes derived from `Command` define commands.

- [`Handler`](refman-cpp-effects.md#class-handler) - Classes derived from `Handler` define handlers.

- [`FlatHandler`](refman-cpp-effects.md#class-flathandler) - Specialisation of `Handler` for generic handlers with identity return clause.

- [`Resumption`](refman-cpp-effects.md#class-resumption) - Suspended computation, given to the user in handlers' command clauses.

- [`ResumptionData` and `ResumptionBase`](refman-cpp-effects.md#classes-resumptiondata-and-resumptionbase) - "Bare" captured continuations that are not memory-managed by the library.

- [`OneShot`](refman-cpp-effects.md#class-oneshot) - The actual interface (via static functions) of handling computations and invoking commands.

:memo: [`cpp-effects/clause-modifiers.h`](refman-clause-modifiers.md) - Modifiers that force specific shapes of command clauses in handlers:

- [`NoResume`](refman-clause-modifiers.md#noresume-modifier) - Command clause that does not use its resumptions.

- [`Plain`](refman-clause-modifiers.md#plain-modifier) - Command clause that interprets a command as a function (i.e., a self- and tail-resumptive clause).
