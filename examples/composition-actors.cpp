// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Erlang-style actors obtained by composition of two effects: threads and mutable state

// In this example, the dynamic binding of an effect to a handler is crucial. It is used by an
// actor to get the reference to the right mailbox in the "receive" function.

#include <iostream>
#include <any>
#include <queue>
#include <list>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

namespace eff = cpp_effects;

// -------------
// Mutable state
// -------------

template <typename S>
struct Put : eff::command<> {
  S newState;
};

template <typename S>
struct Get : eff::command<S> { };

template <typename S>
void put(S s) {
  eff::invoke_command(Put<S>{{}, s});
}

template <typename S>
S get() {
  return eff::invoke_command(Get<S>{});
}

template <typename Answer, typename S>
class HStateful : public eff::flat_handler<Answer, eff::plain<Put<S>>, eff::plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void handle_command(Put<S> p) final override
  {
    state = p.newState;
  }
  S handle_command(Get<S>) final override
  {
    return state;
  }
};

// -------------------
// Lightweight threads
// -------------------

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

using Res = eff::resumption<void()>;

class Scheduler : public eff::flat_handler<void, Yield, Fork, Kill> {
public:
  static void Start(std::function<void()> f)
  {
    queue.push_back(eff::wrap<Scheduler>(f));
    while (!queue.empty()) { // Round-robin scheduling
      auto resumption = std::move(queue.front());
      queue.pop_front();
      std::move(resumption).resume();
    }
  }
private:
  static std::list<Res> queue;
  void handle_command(Yield, Res r) override
  {
    queue.push_back(std::move(r));
  }
  void handle_command(Fork f, Res r) override
  {
    queue.push_back(std::move(r));
    queue.push_back(eff::wrap<Scheduler>(f.proc));
  }
  void handle_command(Kill, Res) override { }
};

std::list<Res> Scheduler::queue;

// ------
// Actors
// ------

using Pid = std::shared_ptr<std::queue<std::any>>;

Pid spawn(std::function<void()> body)
{
  auto msgQueue = std::make_shared<std::queue<std::any>>();
  fork([=]() {
    eff::handle<HStateful<void, Pid>>(body, msgQueue);
  });
  return msgQueue;
}

Pid self()
{
  return get<Pid>();
}

template <typename T>
T receive()
{
  auto q = get<Pid>();
  while (q->empty()) { yield(); }
  auto result = q->front();
  q->pop();
  return std::any_cast<T>(result);
}

template <typename T>
void send(Pid p, T msg)
{
  p->push(msg);
}

// ------------------
// Particular example
// ------------------

void pong()
{
  while (true) {
    auto [pid, n] = receive<std::tuple<Pid, int>>();
    if (n == 0) { return; }
    send<int>(pid, n);
  }
}

void ping()
{
  auto pongPid = spawn(pong);
  for (int i = 1; i <= 10; i++) {
    std::cout << "sent: " << i << ", ";
    send<std::tuple<Pid, int>>(pongPid, {self(), i});
    int response = receive<int>();
    std::cout << "received: " << response << std::endl;
  }
  send<std::tuple<Pid, int>>(pongPid, {self(), 0});
}

int main()
{
  Scheduler::Start(std::bind(spawn, ping));

  // Output:
  // sent: 1, received: 1
  // sent: 2, received: 2
  // sent: 3, received: 3
  // sent: 4, received: 4
  // sent: 5, received: 5
  // sent: 6, received: 6
  // sent: 7, received: 7
  // sent: 8, received: 8
  // sent: 9, received: 9
  // sent: 10, received: 10
}
