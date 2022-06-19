# cpp-effects
A library for programming with effect handlers in C++

![The C++Eff logo](doc/img/logo-250.png)

**Effect handlers** allow for programming with user-defined computational effects, with applications including custom lightweight concurrency (threads, async-await, actors, generators), error handling, dependency injection, etc. Effect handlers originate from the realm of functional programming, and the main goal of this **highly experimental** library is to explore how they fit in the more object-oriented setting of C++.

The library relies on modern C++ features (move semantics, variadic templates, compile-time evaluation) to achieve elegant programmer-level interface, memory management of handlers, and relative type-safety. Internally, it uses the [boost.context](https://www.boost.org/doc/libs/1_74_0/libs/context/doc/html/index.html) library for call-stack manipulation, and so it implements one-shot handlers only.

## Documentation

- [Reference](doc/refman.md) - A detailed explanation of the library's API and a short discussion about the overall design of the library. It is a suitable introduction for a reader already familiar with effect handlers (for example, as they are usually discussed in the context of functional programming).

- [Draft paper](https://homepages.inf.ed.ac.uk/slindley/papers/cppeff-draft-april2022.pdf) - A draft of a research paper that describes the usage and implementation of the library.

## A quick example: lightweight cooperative threads

As a sneak preview, we can use effect handlers to define our own tiny library for cooperative lightweight threads. The programmer's interface will consist of two functions, `yield` and `fork`, together with a class that implements a scheduler: 

```cpp
void yield();                          // Used by a thread to give up control
void fork(std::function<void()> proc); // Start a new thread

class Scheduler {
public:
  static void Start(std::function<void()> f);
};
```

The static member function `Start` initiates the scheduler with `f` as the body of the first thread. It returns when all threads finish their jobs.

To implement this interface, we first define two **commands**, which are data structures used for transferring control from the client code to the handler. We implement `yield` and `fork` to invoke these commands. (The name of the class `OneShot` is supposed to remind the programmer that we're dealing with one-shot handlers only, meaning you cannot resume the same resumption twice). 

```cpp
#include "cpp-effects/cpp-effects.h"
using namespace CppEffects;

struct Yield : Command<void> { };

struct Fork : Command<void> {
  std::function<void()> proc;
};

void yield()
{
  OneShot::InvokeCmd(Yield{});
}

void fork(std::function<void()> proc)
{
  OneShot::InvokeCmd(Fork{{}, proc});
}
```

We define the scheduler, which is a **handler** that can interpret the two commands by pushing the resumptions (i.e., captured continuations) to the queue.

```cpp
using Res = Resumption<void, void>;

class Scheduler : public FlatHandler<void, Yield, Fork> {
public:
  static void Start(std::function<void()> f)
  {
    // Create the first thread by wrapping the body in a handler
    queue.push_back(OneShot::Wrap<Scheduler>(f));
    
    // Round-robin scheduling
    while (!queue.empty()) {
      auto resumption = std::move(queue.front());
      queue.pop_front();
      std::move(resumption).Resume();
    }
  }
private:
  static std::list<Res> queue;
  void CommandClause(Yield, Res r) override
  {
    queue.push_back(std::move(r));
  }
  void CommandClause(Fork f, Res r) override
  {
    queue.push_back(std::move(r));
    queue.push_back(OneShot::Wrap<Scheduler>(f.proc));
  }
};

std::list<Res> Scheduler::queue;
```

And that's all it takes! We can now test our library by starting a few threads:

```cpp
void worker(int k)
{
  for (int i = 0; i < 10; ++i) {
    std::cout << k;
    yield();
  }
}

void starter()
{
  for (int i = 0; i < 5; ++i) {
    fork(std::bind(worker, i));
  }
}

int main()
{
  Scheduler::Start(starter);

  // Output:
  // 01021032104321043210432104321043210432104321432434
}
```

## Technical summary

- **Language:** C++17
- **Handlers**: deep, one-shot, stateful

## Using in your project

The library is header-only, so to use it just include the headers and link with boost.context. On most systems, boost is available via a package manager, e.g.,

- macOS: `brew install boost`

- Ubuntu: `apt-get install libboost-context-dev`

You can link with boost using cmake as follows. In your `CMakeLists.txt`, use the following:

```
FIND_PACKAGE (Boost 1.70 COMPONENTS context REQUIRED)

if (Boost_FOUND)
  link_libraries (Boost::context)
  add_executable (my_program my_program.cpp)
else()
  message (STATUS "Boost not found!")
endif()

```

## Building examples


This repository contains some examples, tests, and benchmarks. The easiest way to build these is to use `cmake`. You will need `cmake` and `boost` in any non-ancient versions. For example, the following should do the trick on macOS:

```bash
$ brew install cmake
$ brew install boost
$ cmake .
$ make
```

(On Ubuntu the package is called `libboost-context-dev`.)

You can check that it works by running an example. The following will run the `threads` example - you can see the interleaving of threads in the output:

```bash
$ bin/threads
01021032104321043210432104321043210432104321432434
```
