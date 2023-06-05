#include <boost/describe/class.hpp>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <random>
#include <ratio>
#include <string>

#include "../util/oop_lua.h"
#include "../util/util.h"

class Worker {
 private:
  std::mt19937 rng_;
  std::normal_distribution<double> ndist_{0.0, 10.0};
  static constexpr size_t LOOP_CNT = 500000;

  inline double sample() noexcept { return ndist_(rng_); }

 public:
  double a_, b_, c_;

  Worker() {
    auto rd = std::random_device{};
    rng_.seed(rd());
    a_ = rng_();
    b_ = rng_();
    c_ = rng_();
  }

  double f() noexcept {
    double res = 0.0;
    for (size_t i = 0; i < LOOP_CNT; ++i) {
      double t = sample();
      res += std::pow(t, 3);
    }
    return res;
  }

  double g() noexcept {
    double res = 0.0;
    for (size_t i = 0; i < LOOP_CNT; ++i) {
      double t = sample();
      res += std::sqrt(t);
    }
    return res;
  }

  double h() noexcept {
    double res = 0.0;
    for (size_t i = 0; i < LOOP_CNT; ++i) {
      double t = sample();
      res += t;
    }
    return res;
  }

  BOOST_DESCRIBE_CLASS(Worker, (), (a_, b_, c_, f, g, h), (), ());
};

double exec_cxx(Worker& worker) noexcept {
  double x = worker.a_ * worker.f();
  double y = worker.b_ * worker.g();
  double z = worker.c_ * worker.h();
  if (std::abs(x) < std::abs(y)) {
    return (std::pow(x, 2) + std::pow(y, 2)) * z;
  } else {
    return (std::pow(x, 2) - std::pow(y, 2)) * z;
  }
}

double exec_lua(Lua& lua, Worker& worker) noexcept {
  double ret;
  lua.call("exec_lua", ret, &worker);
  return ret;
}

double repeat_test(std::function<double(Worker&)> fn, size_t n) noexcept {
  auto start = std::chrono::steady_clock::now();
  double res = 0.0;
  for (size_t i = 0; i < n; ++i) {
    Worker worker;
    res += fn(worker);
  }
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> duration = end - start;
  return duration.count();
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cout << "Usage: <executable> <lua_file> <exec_time>" << std::endl;
    return -1;
  }
  const char* file_name = argv[1];
  size_t n = std::stoi(argv[2]);
  Lua lua({file_name});
  lua.register_type<Worker>();
  auto cxx_duration = repeat_test(exec_cxx, n);
  auto lua_duration = repeat_test(
      [&lua](Worker& worker) -> double { return exec_lua(lua, worker); }, n);
  double cxx_avg = cxx_duration / n, lua_avg = lua_duration / n;
  printf("C++: %0.3lf ms \n", cxx_avg);
  printf("Lua: %0.3lf ms \n", lua_avg);
  return 0;
}
