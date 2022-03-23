// C++ Effects library
// Maciej Pirog, Huawei Edinburgh Research Centre, maciej.pirog@huawei.com
// License: MIT

// Benchmark: comapre static and dynamic InvokeCmd

#include <iostream>
#include <optional>
#include <string>
#include <chrono>

#include "cpp-effects/cpp-effects.h"

using namespace CppEffects;

namespace DynamicGenerator {

// --------------
// Internal stuff
// --------------

template <typename T>
struct Yield : Command<void> {
  T value;
};

template <typename T>
void yield(int64_t label, T x)
{
  OneShot::InvokeCmd(label, Yield<T>{{}, x});
}

template <typename T>
struct GenState;

template <typename T>
using Result = std::optional<GenState<T>>;

template <typename T>
struct GenState {
  T value;
  Resumption<void, Result<T>> resumption;
};

template <typename T>
class GeneratorHandler : public Handler<Result<T>, void, Yield<T>> {
  Result<T> CommandClause(Yield<T> y, Resumption<void, Result<T>> r) final override
  {
    return GenState<T>{y.value, std::move(r)};
  }
  Result<T> ReturnClause() final override
  {
    return {};
  }
};

// ----------------------
// Programmer's interface
// ----------------------

// When a generator is created, its body is executed until the first
// Yield. It is only because in this example we want the programmer's
// interface to be as simple as possible, so that the "there is a
// value" and "you can resume me" states of a generator always hve the
// same value, available via operator bool.

template <typename T>
class Generator {
public:
  Generator(std::function<void(std::function<void(T)>)> f)
  {
    auto label = OneShot::FreshLabel();
    result = OneShot::Handle<GeneratorHandler<T>>(label, [f, label](){
      f([label](T x) { yield<T>(label, x); });
    });
  }
  Generator() { } // Create a dummy generator that generates nothing
  Generator(const Generator&) = delete;
  Generator(Generator&& other)
  {
    result = result.res;
    other.result = {};
  }
  Generator& operator=(const Generator&) = delete;
  Generator& operator=(Generator&& other)
  {
    if (this != &other) {
      if (result) { delete result.value().resumption; }
      result = other.result;
      other.result = {};
    }
    return *this;
  }
  T Value() const
  {
    if (!result) { throw std::out_of_range("Generator::Value"); }
    return result.value().value;
  }
  bool Next()
  {
    if (!result) { throw std::out_of_range("Generator::Value"); }
    result = std::move(result.value().resumption).Resume();
    return result.has_value();
  }
  operator bool() const
  {
    return result.has_value();
  }
private:
  Result<T> result = {};
};

}

namespace StaticGenerator {

// --------------
// Internal stuff
// --------------

template <typename T>
class GeneratorHandler;

template <typename T>
struct Yield : Command<void> {
  T value;
};

template <typename T>
void yield(int64_t label, T x)
{
  OneShot::StaticInvokeCmd<GeneratorHandler<T>>(label, Yield<T>{{}, x}); // <--- StaticInvokeCmd
}

template <typename T>
struct GenState;

template <typename T>
using Result = std::optional<GenState<T>>;

template <typename T>
struct GenState {
  T value;
  ResumptionData<void, Result<T>>* resumption;
};

template <typename T>
class GeneratorHandler : public Handler<Result<T>, void, Yield<T>> {
  Result<T> CommandClause(Yield<T> y, Resumption<void, Result<T>> r) final override
  {
    return GenState<T>{y.value, r.Release()};
  }
  Result<T> ReturnClause() final override
  {
    return {};
  }
};

// ----------------------
// Programmer's interface
// ----------------------

// When a generator is created, its body is executed until the first
// Yield. It is only because in this example we want the programmer's
// interface to be as simple as possible, so that the "there is a
// value" and "you can resume me" states of a generator always hve the
// same value, available via operator bool.

template <typename T>
class Generator {
public:
  Generator(std::function<void(std::function<void(T)>)> f)
  {
    auto label = OneShot::FreshLabel();
    result = OneShot::Handle<GeneratorHandler<T>>(label, [f, label](){
      f([label](T x) { yield<T>(label, x); });
    });
  }
  Generator() { } // Create a dummy generator that generates nothing
  Generator(const Generator&) = delete;
  Generator(Generator&& other)
  {
    result = result.res;
    other.result = {};
  }
  Generator& operator=(const Generator&) = delete;
  Generator& operator=(Generator&& other)
  {
    if (this != &other) {
      if (result) { delete result.value().resumption; }
      result = other.result;
      other.result = {};
    }
    return *this;
  }
  T Value() const
  {
    if (!result) { throw std::out_of_range("Generator::Value"); }
    return result.value().value;
  }
  bool Next()
  {
    if (!result) { throw std::out_of_range("Generator::Value"); }
    result = Resumption<void, Result<T>>(result.value().resumption).Resume();
    return result.has_value();
  }
  operator bool() const
  {
    return result.has_value();
  }
private:
  Result<T> result = {};
};

}

namespace OptDynamicGenerator {

// --------------
// Internal stuff
// --------------

template <typename T>
class GeneratorHandler;

template <typename T>
struct Yield : Command<void> {
  T value;
};

template <typename T>
void yield(int64_t label, T x)
{
  OneShot::InvokeCmd(label, Yield<T>{{}, x});
}

template <typename T>
struct GenState;

template <typename T>
using Result = std::optional<GenState<T>>;

template <typename T>
class Generator;

template <typename T>
struct GenState {
  T value;
  ResumptionData<void, void>* resumption;
};

template <typename T>
class GeneratorHandler : public Handler<void, void, Yield<T>> {
  void CommandClause(Yield<T> y, Resumption<void, void> r) final override
  {
    gen->result = GenState<T>{y.value, r.Release()};
  }
  void ReturnClause() final override
  {
  }
public:
  Generator<T>* gen;
  GeneratorHandler(Generator<T>* gen) : gen(gen) { }
};

// ----------------------
// Programmer's interface
// ----------------------

// When a generator is created, its body is executed until the first
// Yield. It is only because in this example we want the programmer's
// interface to be as simple as possible, so that the "there is a
// value" and "you can resume me" states of a generator always hve the
// same value, available via operator bool.

template <typename T>
class Generator {
public:
  Generator(std::function<void(std::function<void(T)>)> f)
  {
    auto label = OneShot::FreshLabel();
    OneShot::Handle<GeneratorHandler<T>>(label, [f, label](){
      f([label](T x) { yield<T>(label, x); });
    }, this);
  }
  Generator() { } // Create a dummy generator that generates nothing
  Generator(const Generator&) = delete;
  Generator(Generator&& other)
  {
    result = result.res;
    other.result = {};
  }
  Generator& operator=(const Generator&) = delete;
  Generator& operator=(Generator&& other)
  {
    if (this != &other) {
      if (result) { delete result.value().resumption; }
      result = other.result;
      other.result = {};
    }
    return *this;
  }
  T Value() const
  {
    if (!result) { throw std::out_of_range("Generator::Value"); }
    return result.value().value;
  }
  bool Next()
  {
    if (!result) { throw std::out_of_range("Generator::Value"); }
    Resumption<void,void>(result.value().resumption).Resume();
    return result.has_value();
  }
  operator bool() const
  {
    return result.has_value();
  }
  Result<T> result = {};
};

}

namespace OptStaticGenerator {


// --------------
// Internal stuff
// --------------

template <typename T>
class GeneratorHandler;

template <typename T>
struct Yield : Command<void> {
  T value;
};

template <typename T>
void yield(int64_t label, T x)
{
  OneShot::StaticInvokeCmd<GeneratorHandler<T>>(label, Yield<T>{{}, x});
}

template <typename T>
struct GenState;

template <typename T>
using Result = std::optional<GenState<T>>;

template <typename T>
class Generator;

template <typename T>
struct GenState {
  T value;
  Resumption<void, void> resumption;
};

template <typename T>
class GeneratorHandler : public Handler<void, void, Yield<T>> {
  void CommandClause(Yield<T> y, Resumption<void, void> r) final override
  {
    gen->result = GenState<T>{y.value, std::move(r)};
  }
  void ReturnClause() final override
  {
  }
public:
  Generator<T>* gen;
  GeneratorHandler(Generator<T>* gen) : gen(gen) { }
};

// ----------------------
// Programmer's interface
// ----------------------

// When a generator is created, its body is executed until the first
// Yield. It is only because in this example we want the programmer's
// interface to be as simple as possible, so that the "there is a
// value" and "you can resume me" states of a generator always hve the
// same value, available via operator bool.

template <typename T>
class Generator {
public:
  Generator(std::function<void(std::function<void(T)>)> f)
  {
    auto label = OneShot::FreshLabel();
    OneShot::Handle<GeneratorHandler<T>>(label, [f, label](){
      f([label](T x) { yield<T>(label, x); });
    }, this);
  }
  Generator() { } // Create a dummy generator that generates nothing
  Generator(const Generator&) = delete;
  Generator(Generator&& other)
  {
    if (this != &other) {
      result = result.res;
      other.result = {};
    }
  }
  Generator& operator=(const Generator&) = delete;
  Generator& operator=(Generator&& other)
  {
    if (this != &other) {
      //if (result) { delete result.value().resumption; }
      result = other.result;
      other.result = {};
    }
    return *this;
  }
  T Value() const
  {
    //if (!result) { throw std::out_of_range("Generator::Value"); }
    return (*result).value;
  }
  bool Next()
  {
    //if (!result) { throw std::out_of_range("Generator::Value"); }
    std::move((*result).resumption).Resume();
    return result.has_value();
  }
  operator bool() const
  {
    return result.has_value();
  }
  Result<T> result = {};
};

}


namespace OptOptStaticGenerator {


// --------------
// Internal stuff
// --------------

template <typename T>
class GeneratorHandler;

template <typename T>
struct CmdYield : Command<void> {
  const T value;
};

template <typename T>
struct Yield {
  const int64_t label;
  void operator()(T x) const
  {
    OneShot::StaticInvokeCmd<GeneratorHandler<T>>(label, CmdYield<T>{{}, x});
  }
};

template <typename T>
struct GenState;

template <typename T>
using Result = std::optional<GenState<T>>;

template <typename T>
class Generator;

template <typename T>
struct GenState {
  T value;
  Resumption<void, void> resumption;
};

template <typename T>
class GeneratorHandler : public Handler<void, void, CmdYield<T>> {
  void CommandClause(CmdYield<T> y, Resumption<void, void> r) final override
  {
    gen->result = GenState<T>{y.value, std::move(r)};
  }
  void ReturnClause() final override
  {
  }
public:
  Generator<T>* gen;
  GeneratorHandler(Generator<T>* gen) : gen(gen) { }
};

// ----------------------
// Programmer's interface
// ----------------------

// When a generator is created, its body is executed until the first
// Yield. It is only because in this example we want the programmer's
// interface to be as simple as possible, so that the "there is a
// value" and "you can resume me" states of a generator always hve the
// same value, available via operator bool.

template <typename T>
class Generator {
public:
  Generator(std::function<void(Yield<T>)> f)
  {
    auto label = OneShot::FreshLabel();
    OneShot::Handle<GeneratorHandler<T>>(label, [f, label](){ f(Yield<T>{label}); }, this);
  }
  Generator() { } // Create a dummy generator that generates nothing
  Generator(const Generator&) = delete;
  Generator(Generator&& other)
  {
    if (this != &other) {
      result = result.res;
      other.result = {};
    }
  }
  Generator& operator=(const Generator&) = delete;
  Generator& operator=(Generator&& other)
  {
    if (this != &other) {
      //if (result) { delete result.value().resumption; }
      result = other.result;
      other.result = {};
    }
    return *this;
  }
  T Value() const
  {
    //if (!result) { throw std::out_of_range("Generator::Value"); }
    return (*result).value;
  }
  bool Next()
  {
    //if (!result) { throw std::out_of_range("Generator::Value"); }
    std::move((*result).resumption).Resume();
    return result.has_value();
  }
  operator bool() const
  {
    return result.has_value();
  }
  Result<T> result = {};
};

}


// ------------------
// Particular example
// ------------------

int main()
{

  std::cout << "--- generators: static vs dynamic invoke ---" << std::endl;

  volatile int64_t SUM = 0;
  const int MAX = 1000000;

  {

  std::cout << "loop:          " << std::flush;

  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i <= MAX; i++) {
    SUM = SUM + i;
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / MAX) << "ns per iteration)" <<std::endl;

  }

  SUM = 0;

  {

  std::cout << "naive-dynamic: " << std::flush;

  DynamicGenerator::Generator<int> dnaturals([](auto yield) {
    int i = 0;
    while (true) { yield(i++); }
  });

  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i <= MAX; i++) {
    SUM = SUM + dnaturals.Value();
    dnaturals.Next();
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / MAX) << "ns per iteration)" <<std::endl;

  }

  SUM = 0;

  {

  std::cout << "naive-static:  " << std::flush;

  StaticGenerator::Generator<int> snaturals([](auto yield) {
    int i = 0;
    while (true) { yield(i++); }
  });

  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i <= MAX; i++) {
    SUM = SUM + snaturals.Value();
    snaturals.Next();
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / MAX) << "ns per iteration)" <<std::endl;

  }

  SUM = 0;

  {

  std::cout << "opt-dynamic:   " << std::flush;

  OptDynamicGenerator::Generator<int> snaturals([](auto yield) {
    int i = 0;
    while (true) { yield(i++); }
  });

  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i <= MAX; i++) {
    SUM = SUM + snaturals.Value();
    snaturals.Next();
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / MAX) << "ns per iteration)" <<std::endl;

  }

  SUM = 0;

  {

  std::cout << "opt-static:    " << std::flush;

  OptStaticGenerator::Generator<int> snaturals([](auto yield) {
    int i = 0;
    while (true) { yield(i++); }
  });

  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i <= MAX; i++) {
    SUM = SUM + snaturals.Value();
    snaturals.Next();
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / MAX) << "ns per iteration)" <<std::endl;

  }

  SUM = 0;

  {

  std::cout << "optopt-static: " << std::flush;

  OptOptStaticGenerator::Generator<int> snaturals([](auto yield) {
    int i = 0;
    while (true) { yield(i++); }
  });

  auto begin = std::chrono::high_resolution_clock::now();

  for (int i = 0; i <= MAX; i++) {
    SUM = SUM + snaturals.Value();
    snaturals.Next();
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << " \t(" << (int)(std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() / MAX) << "ns per iteration)" <<std::endl;

  }
}
