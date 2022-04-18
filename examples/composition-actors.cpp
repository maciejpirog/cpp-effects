#include <iostream>
#include <any>
#include <queue>
#include <list>

#include "cpp-effects/cpp-effects.h"
#include "cpp-effects/clause-modifiers.h"

using namespace CppEffects;

// -------------
// Mutable state
// -------------

template <typename S>
struct Put : Command<void> {
  S newState;
};

template <typename S>
struct Get : Command<S> { };

template <typename S>
void put(S s) {
  OneShot::InvokeCmd(Put<S>{{}, s});
}

template <typename S>
S get() {
  return OneShot::InvokeCmd(Get<S>{});
}

template <typename Answer, typename S>
class HStateful : public FlatHandler<Answer, Plain<Put<S>>, Plain<Get<S>>> {
public:
  HStateful(S initialState) : state(initialState) { }
private:
  S state;
  void CommandClause(Put<S> p) final override
  {
    state = p.newState;
  }
  S CommandClause(Get<S>) final override
  {
    return state;
  }
};

// -------------------
// Lightweight threads
// -------------------

struct Yield : Command<void> { };

struct Fork : Command<void> {
  std::function<void()> proc;
};

struct Kill : Command<void> { };

void yield()
{
  OneShot::InvokeCmd(Yield{});
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

class Scheduler : public Handler<void, void, Yield, Fork, Kill> {
public:
  static void Start(std::function<void()> f)
  {
    Run(f);
    while (!queue.empty()) { // Round-robin scheduling
      auto resumption = std::move(queue.front());
      queue.pop_front();
      std::move(resumption).Resume();
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
    queue.push_back({std::bind(Run, f.proc)});
  }
  void CommandClause(Kill, Res) override { }
  void ReturnClause() override { }
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
    OneShot::Handle<HStateful<void, Pid>>(body, msgQueue);
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
    send<std::tuple<Pid, int>>(pongPid, {self(), i});
    std::cout << "received: " << receive<int>() << std::endl;
  }
  send<std::tuple<Pid, int>>(pongPid, {self(), 0});
}

int main()
{
  Scheduler::Start(std::bind(spawn, ping));
}
