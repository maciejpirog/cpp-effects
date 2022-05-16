// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

/*
Example: A toy library for lightweight-threaded GUI in terminal

(toy = the only event is timer, and components are: text, blinking
text, spinner, and progress bar)

The programmer's interface is very loosely based on React with
functional components and hooks. The idea is that we supply a function
that renders the interface. This function can use "hooks", which
access pieces of data modified by threads. Such a thread can, for
example, describe an animation.

The point of this example is to show that effect handlers are useful
when one needs a customised scheduler for lightweight threads.
*/

#include <any>
#include <chrono>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <random>
#include <thread>
#include <tuple>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

using namespace CppEffects;
using namespace std::literals::chrono_literals;
namespace chrono = std::chrono;

// ---------
// Internals
// ---------

using TimePoint = chrono::time_point<std::chrono::system_clock>;

struct Sleep : Command<void> {
  const chrono::milliseconds time;
};

struct Fork : Command<void> {
  std::function<void()> proc;
  bool background = false;
};

struct Kill : Command<void> { };

void yield()
{
  OneShot::InvokeCmd(Sleep{{}, 0ms});
}

void sleep(chrono::milliseconds t)
{
  OneShot::InvokeCmd(Sleep{{}, t});
}

void fork(std::function<void()> proc)
{
  OneShot::InvokeCmd(Fork{{}, proc});
}

void kill()
{
  OneShot::InvokeCmd(Kill{});
}

using Res = Resumption<void, void>;

using INT = int;

struct ThreadInfo {
  TimePoint wakeAt;
  bool background;
  Res res;
};

class Scheduler : public FlatHandler<void, Sleep, Fork, Kill> {
public:
  static void Start(std::function<void()> f)
  {
    std::cout << "\x1B[?25l"; // hide cursor
    f();
    std::cout << std::flush;

    while (!queue.empty()) {
      // Check if anything left to run
      auto checkBg = [](ThreadInfo const& ti){ return !ti.background; };
      if (std::find_if(queue.begin(), queue.end(), checkBg) == queue.end()) {
        // Last render before goodbye
        Clear();
        f();
        break;
      }

      // Run an event
      auto now = chrono::system_clock::now();
      auto [t, background, resumption] = std::move(queue.front());
      queue.pop_front();
      if (t > now) { std::this_thread::sleep_for(t - now); }
      currentThread.background = background;
      std::move(resumption).Resume();

      // Render
      Clear();
      f();
      std::cout << std::flush;
    }
    std::cout << "\x1B[?25h"; // show cursor
  }
  static std::map<std::string, INT> hooks;
  static std::function<void()> root;
  static int lines;
  static ThreadInfo currentThread;
  static std::list<ThreadInfo> queue; 
  static void Clear()
  {
    for (int i = 0; i < lines; i++) {
      std::cout << "\033[2K" << "\r" << "\033[1F" ;
    }
    std::cout << "\033[2K" << "\r";
    lines = 0;
  } 
  static void Run(std::function<void()> f)
  {
    OneShot::Handle<Scheduler>(f);
  }
  static void Enqueue(TimePoint wakeAt, bool background, Res r)
  {
    auto pred = [&](auto&& b) { return wakeAt < b.wakeAt; };
    auto it = std::find_if(queue.begin(), queue.end(), pred);
    queue.insert(it, {wakeAt, background, std::move(r)});
  }
  void CommandClause(Sleep s, Res r) override
  {
    Enqueue(chrono::system_clock::now() + s.time, currentThread.background, std::move(r));
  }
  void CommandClause(Fork f, Res r) override
  {
    auto time = chrono::system_clock::now();
    Enqueue(time, currentThread.background, std::move(r));
    Enqueue(time, f.background, Res{std::bind(Run, f.proc)});
  }
  void CommandClause(Kill, Res) override { }
};

std::list<ThreadInfo> Scheduler::queue;
std::map<std::string, INT> Scheduler::hooks;
int Scheduler::lines = 0;
ThreadInfo Scheduler::currentThread;

// -------------------------------------------
// Interface for using hooks in user's program
// -------------------------------------------

// General function for using a hook

template <typename F, typename... Args>
INT useHook(bool background, std::string hid, F const& /*std::function<void(INT&, Args&&...)>*/ hook, INT init, Args&&... args)
{
  if (Scheduler::hooks.count(hid) == 0)
  {
    Scheduler::hooks[hid] = init;
    Scheduler::currentThread.background = background;
    Scheduler::Run(std::bind(hook, std::ref(Scheduler::hooks[hid]), std::forward<Args>(args)...));
  }
  return Scheduler::hooks[hid];
}

// Use a foreground hook
//
// (The program runs until the end of all foreground hooks. They are
// useful for threads that provide control of the program.)

template <typename F, typename... Args>
INT useHook(std::string hid, F const& hook, INT init, Args&&... args)
{
  return useHook(false, hid, hook, init, std::forward<Args>(args)...);
}

// Use a background hook
//
// (Background hooks are useful for looped animations that end
// together with the rest of the program.)

template <typename F, typename... Args>
INT useBackgroundHook(std::string hid, F const& hook, INT init, Args&&... args)
{
  return useHook(true, hid, hook, init, std::forward<Args>(args)...);
}

// -----------------
// Examples of hooks
// -----------------

void animate(int& from, int to, chrono::milliseconds interval)
{
  yield();
  while (from < to)
  {
    from += 1;
    sleep(interval);
  }
}

void animateLoop(int& from, int to, chrono::milliseconds interval)
{
  yield();
  while (true) {
    from += 1; 
    from %= to + 1;
    sleep(interval);
  }
}

// ----------------------
// Examples of components
// ----------------------

void text(std::string s, bool bold = false)
{
  if (bold) { std::cout << "\033[1m"; } // bold
  std::cout << s;
  if (bold) { std::cout << "\033[22m"; } // reset
}

void space()
{
  text(" ");
}

void newLine()
{
  Scheduler::lines++;
  text("\n");
}

void progressBar(int val, int width = 40)
{
  std::cout << "[";
  if (val < 100)
  {
    std::cout << "\033[0;31m"; // red
  } else {
    std::cout << "\033[0;32m"; // green
  }
  for (int i = 0; i < width; i++)
  {
    if (((i * 100) / width) < val) {
      std::cout << "━";
    } else {
      std::cout << " ";
    }
  }
  std::cout << "\033[0m"; // reset
  std::cout << "]";
}

void spinner()
{
  auto d = useBackgroundHook("spinner", animateLoop, 0, 3, 200ms);
  const char chars[] = {'|', '/', '-', '\\'};
  std::cout << "\033[1m" << chars[d] << "\033[22m"; // bold, reset
}

void blinkText(std::string s)
{
  auto d = useBackgroundHook("blinkText", animateLoop, 0, 1, 800ms);
  if (d) {
    std::cout << s;
  } else {
    for (size_t i = 0; i < s.length(); i++) { std::cout << " "; }
  }
}

// ------------------
// Particular example
// ------------------

// A user-defined component

void myBar(int val, int width = 40)
{
  progressBar(val, width);
  if (val < 100) {
    text(" ");
    text(std::to_string(val));
    text("%");
  } else {
    text(" done", true);
  }
  newLine();
}

// This is a particular program

void interface()
{
  int a = useHook("progressA", animate, 0, 100, 30ms);
  int b = useHook("progressB", animate, 0, 100, 100ms);
  int c = useHook("progressC", animate, 0, 100, 60ms);
  int d = useHook("progressD", animate, 0, 100, 40ms);
  int e = useHook("progressE", animate, 0, 100, 85ms);
  auto sum = a + b + c + d + e;
  if (sum < 500) {
    spinner();
    space();
  }
  text("progress: ");
  text(std::to_string((a + b + c + d + e) / 5));
  text("%");
  newLine();

  myBar(a, 30);
  myBar(b, 30);
  myBar(c, 30);
  myBar(d, 30);
  myBar(e, 30);

  if (sum < 500) {
    blinkText("WAIT...");
  } else {
    text("ALL DONE!");
  }
}

int main()
{
  Scheduler::Start(interface);

  std::cout << std::endl; 
}
