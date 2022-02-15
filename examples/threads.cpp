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

using namespace CppEffects;

// ----------------------------------------
// Effect intarface for lightweight threads
// ----------------------------------------

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

// ---------------------
// Scheduler for threads
// ---------------------

using Res = std::unique_ptr<Resumption<void, void>>;

class Scheduler : public Handler<void, void, Yield, Fork> {
public:
  static void Start(std::function<void()> f)
  {
    Run(f);
    while (!queue.empty()) { // Round-robin scheduling
      auto resumption = std::move(queue.front());
      queue.pop_front();
      OneShot::Resume(std::move(resumption));
    }
  }
private:
  static std::list<Res> queue;
  static void Run(std::function<void()> f)
  {
    OneShot::Handle<Scheduler>(f);
  }
  void CommandClause(Yield, Res r) override
  {
    queue.push_back(std::move(r));
  }
  void CommandClause(Fork f, Res r) override
  {
    queue.push_back(std::move(r));
    queue.push_back(OneShot::MakeResumption<void>(std::bind(Run, f.proc)));
  }
  void ReturnClause() override { }
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
    fork(std::bind(worker, i));
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
