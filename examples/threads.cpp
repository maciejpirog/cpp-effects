// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Lightweight threads with global round-robin scheduler

#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <random>
#include <tuple>

#include "cpp-effects/cpp-effects.h"
//#include "cpp-effects/clause-modifiers.h"

namespace eff = cpp_effects;

// ----------------------------------------
// Effect intarface for lightweight threads
// ----------------------------------------

struct Yield : eff::command<> { };

struct Fork : eff::command<> {
  std::function<void()> proc;
};

struct Kill : eff::command<> { };

void yield()
{
  eff::invoke_command(Yield{});
}

void fork(std::function<void()> proc)
{
  eff::invoke_command(Fork{{}, proc});
}

void kill()
{
  eff::invoke_command(Kill{});
}

// ---------------------
// Scheduler for threads
// ---------------------

using Res = eff::resumption<void()>;

class Scheduler : public eff::handler<void, void, Yield, Fork, Kill> {
public:
  static void Start(std::function<void()> f)
  {
    queue.push_back(eff::wrap<Scheduler>(f));
    
    while (!queue.empty()) {
      auto resumption = std::move(queue.front());
      queue.pop_front();
      std::move(resumption).resume();
    }
  }
private:
  static std::list<Res> queue;
  
  void handle_command(Yield, Res r) override {
    queue.push_back(std::move(r));
  }
  
  void handle_command(Fork f, Res r) override {
    queue.push_back(std::move(r));
    queue.push_back(eff::wrap<Scheduler>(f.proc));
  }
  
  void handle_command(Kill, Res) override { }
  
  void handle_return() override { }
};

std::list<Res> Scheduler::queue;

// ------------------
// Particular example
// ------------------

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
    fork([=](){ worker(i); });
  }
}

int main()
{
  Scheduler::Start(starter);

  std::cout << std::endl; 

  // Output:
  // 01021032104321043210432104321043210432104321432434
}

// -----
// Notes
// -----

/*
We can implement randomised scheduleing e.g. like this:

  static void Start(std::function<void()> f)
  {
    Run(f);
    while (!queue.empty()) { // Randomised scheduling
      auto it = queue.begin();
      std::advance(it, rand() % queue.size());
      auto resumption = std::move(*it);
      queue.erase(it);
      OneShot::Resume(std::move(resumption));
    }
  }

In which case the output is something like:
00001002134110400012112432132113322433334244424234
*/
