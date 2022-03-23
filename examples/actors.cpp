// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Example: Erlang-inspired message-passing actors with local round-robin scheduler

// Todo: This example still requires some thought on memory management

#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <tuple>
#include <queue>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

// ----------------------
// Programmer's interface
// ----------------------

// The user defines an actor as a class derived from Actor<T>, where T
// is the type of messages that the actor accepts. Each actor's life
// purpose is to execute the virtual function body. When it's
// finished, the actor dies.
//
// An actor is an instance of this class. It should also be added to
// the scheduler. One can start a scheduler like this:
//
// Scheduler scheduler;
// scheduler.Run(p);
//
// where p is a pointer to the first actor to be executed by the
// scheduler. New actors can be added to the scheduler by calling
// Spawn from inside actors' bodies.

class Scheduler;

class ActorBase {
  friend class Scheduler;
  template <typename> friend class Actor;
protected:
  virtual void body() = 0;
private:
  Scheduler* scheduler = nullptr;
  bool awaitingMsg = false;
};

template <typename T>
class Actor : public ActorBase {
public:
  void Send(T msg);  // Send a message to this actor
protected:
  std::queue<T> msgQueue;

  // Functions to be used inside of body:
  T Receive();  // Read message from inbox (or suspend if there isn't any)
  void Yield();  // Give up control
  void Spawn(ActorBase* actor);  // Add an actor to the scheduler
};

struct ActorInfo;

class Scheduler {
  friend class ActorHandler;
  template <typename> friend class Actor;
public:
  void Run(ActorBase* actor);
  Scheduler() : label(OneShot::FreshLabel()) { }
private:
  int64_t label;
  ActorBase* currentActor;
  std::vector<ActorInfo> active;
  void wake();
};

// --------------
// Internal stuff
// --------------

using Res = Resumption<void, void>;

struct ActorInfo {
  ActorBase* actorData;
  Res actorResumption;
};

struct CmdYield : Command<void> { };

struct CmdFork : Command<void> {
  ActorBase* actor;
};

class ActorHandler : public Handler<void, void, CmdYield, CmdFork> {
public:
  ActorHandler(Scheduler* scheduler) : scheduler(scheduler) { }
private:
  Scheduler* scheduler;
  void CommandClause(CmdYield, Res r) override
  {
    scheduler->active.push_back({scheduler->currentActor, std::move(r)});
    scheduler->wake();
  }
  void CommandClause(CmdFork f, Res r) override
  {
    scheduler->active.push_back({
      f.actor,
      {[f, this](){ scheduler->Run(f.actor); }}});
    std::move(r).Resume();
  }
  void ReturnClause() override
  {
    if (scheduler->active.empty()) { return; }
    scheduler->wake();
  }
};

void Scheduler::wake()
{
  for (auto it = active.begin(); it != active.end(); ++it) {
    if (it->actorData->awaitingMsg) { continue; }
    auto resumption = std::move(it->actorResumption);
    currentActor = it->actorData;
    active.erase(it);
    std::move(resumption).TailResume();
    return;
  }
  std::cerr << "deadlock detected" << std::endl;
  exit(-1);
}

void Scheduler::Run(ActorBase* f)
{
  f->scheduler = this;
  currentActor = f;
  OneShot::Handle<ActorHandler>(this->label, [f](){ f->body(); }, this);
}

template <typename T>
T Actor<T>::Receive()
{
  if (msgQueue.empty()) {
    awaitingMsg = true;
    OneShot::InvokeCmd(scheduler->label, CmdYield{});
  }
  // A waiting actor can be resumed only after a message is put in its
  // inbox, so if the control reaches this point, it means the actor's
  // got mail.
  T t = msgQueue.front();
  msgQueue.pop();
  return t;
}

template <typename T>
void Actor<T>::Send(T msg)
{
  awaitingMsg = false;
  msgQueue.push(msg);
  OneShot::InvokeCmd(scheduler->label, CmdYield{});
}

template <typename T>
void Actor<T>::Yield()
{
  OneShot::InvokeCmd(scheduler->label, CmdYield{});
}

template <typename T>
void Actor<T>::Spawn(ActorBase* actor)
{
  actor->scheduler = scheduler;
  OneShot::InvokeCmd(scheduler->label, CmdFork{{}, actor});
}

// ----------------
// Concrete example
// ----------------

class Echo : public Actor<std::tuple<Actor<int>*, int>> {
  void body() override
  {
    while (true) {
      auto [sender, msg] = Receive();
      if (msg == -1) {
        std::cout << "no more echo" << std::endl;
        return;
      }
      sender->Send(msg);
    }
  }
};

class Starter : public Actor<int> {
  void body()
  {
    Echo echo;     // Create an actor
    Spawn(&echo);  // Add it to the scheduler
    
    for (int i = 0; i < 10; i++) {
      std::cout << "sent: " << i;
      echo.Send({this, i});
      std::cout << ", received: " << Receive() << std::endl;
    }
    echo.Send({this, -1});
  }
};

int main()
{
  Scheduler scheduler;
  Starter starter;
  scheduler.Run(&starter);

  // Output:
  // sent: 0, received: 0
  // sent: 1, received: 1
  // sent: 2, received: 2
  // sent: 3, received: 3
  // sent: 4, received: 4
  // sent: 5, received: 5
  // sent: 6, received: 6
  // sent: 7, received: 7
  // sent: 8, received: 8
  // sent: 9, received: 9
  // no more echo
}
