// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Lightweight async-await with global randomised scheduler

#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <variant>
#include <vector>

#include "cpp-effects/cpp-effects.h"

namespace eff = cpp_effects;

// ----------------------------
// Async-await with a scheduler
// ----------------------------

using Res = eff::resumption<void()>;

template <typename T>
class Scheduler;

struct GenericFuture {
  std::vector<Res> awaiting;
};

template <typename T>
class Future : public GenericFuture {
  template <typename> friend class Scheduler;
public:
  T Value() const { return value.value(); }
  operator bool() const { return (bool)value; }
private:
  std::optional<T> value;
};

struct Yield : eff::command<> { };

struct Await : eff::command<> {
  GenericFuture* future;
};

std::vector<Res> queue;

template <typename T>
class Scheduler : public eff::handler<void, T, Yield, Await> {
  template <typename TT> friend Future<TT>* async(std::function<TT()> f);
public:
  Scheduler(Future<T>* currentFuture) : currentFuture(currentFuture) { }
  static T Run(std::function<T()> f)
  {
    Future<T> future;
    Scheduler<T>::Run(&future, f);
    return future.Value();
  }
private:
  Future<T>* currentFuture;
  static void Run(Future<T>* future, std::function<T()> f)
  {
    eff::handle<Scheduler<T>>(f, future);
  }
  void wakeRandom()
  {
    if (queue.empty()) { return; }
    auto it = queue.begin();
    std::advance(it, rand() % queue.size());
    auto resumption = std::move(*it);
    queue.erase(it);
    std::move(resumption).resume();
  }
  void handle_command(Yield, Res r) override
  {
    queue.push_back(std::move(r));
    wakeRandom();
  }
  void handle_command(Await f, Res r) override
  {
    f.future->awaiting.push_back(std::move(r));
    wakeRandom();
  }
  void handle_return(T val) override
  {
    std::move(currentFuture->awaiting.begin(), currentFuture->awaiting.end(),
      std::back_inserter(queue));
    currentFuture->value = val;
    wakeRandom();
  }
};

void yield()
{
  eff::invoke_command(Yield{});
}

template <typename T>
T await(Future<T>* future)
{
  if (*future) { return future->Value(); }
  eff::invoke_command(Await{{}, future});
  return future->Value();
}

// The pointer returned by async is not unique. It is shared between
// the user and the scheduler. Only when the future is fulfilled, the
// scheduler forgets the pointer and the user is safe to delete the
// future.

template <typename T>
Future<T>* async(std::function<T()> f)
{
  auto future = new Future<T>;
  queue.push_back({[f, future]() {
    Scheduler<T>::Run(future, f);
  }});
  return future;
} 

// ------------------
// Particular example
// ------------------

int worker()
{
  for (int i = 0; i < 30; i++) {
    std::cout << "." << std::flush;
    yield();
  }
  return 100;
}

std::monostate starter()
{
  auto future = async<int>(worker);
  std::cout << "[worker started]" << std::flush;

  for (int i = 0; i < 5; i++) {
    if (!*future) {
      std::cout << std::endl << "[no value yet]" << std::flush;
      yield();
    }
  }
  std::cout << std::endl << "[I'd better wait]" << std::flush;
  int workerResult = await(future);
  delete future;
  std::cout << std::endl << "[worker returned " << workerResult << "]" << std::flush;
  return {};
}

int main()
{
  srand(11);
  Scheduler<std::monostate>::Run(starter);

  std::cout << std::endl;

  // Output:
  // [worker started]
  // [no value yet]
  // [no value yet]....
  // [no value yet]..
  // [no value yet]
  // [no value yet].
  // [I'd better wait].......................
  // [worker returned 100]
}
