#pragma once

#include <iostream>
#include <string>

class Data {
 private:
  double a_ = 1, b_ = 2, c_ = 3, d_ = 4;

 public:
  Data() = default;

  double get_a() const { return a_; };
  void set_a(double value) { a_ = value; }

  double get_b() const { return b_; };
  void set_b(double vblue) { b_ = vblue; }

  double get_c() const { return c_; };
  void set_c(double vclue) { c_ = vclue; }

  double get_d() const { return d_; };
  void set_d(double vdlue) { d_ = vdlue; }
};

class DataDict {
 private:
  Data& data_;

 public:
  DataDict(Data& data) : data_(data) {}

  void set_field(const std::string& field, double value) {
    if (field == "a") {
      data_.set_a(value);
    } else if (field == "b") {
      data_.set_b(value);
    } else if (field == "c") {
      data_.set_c(value);
    } else if (field == "d") {
      data_.set_d(value);
    } else {
      // report error
    }
  }

  double field(const std::string& field) const {
    if (field == "a") {
      return data_.get_a();
    } else if (field == "b") {
      return data_.get_b();
    } else if (field == "c") {
      return data_.get_c();
    } else if (field == "d") {
      return data_.get_d();
    } else {
      // report error
      return 0.0;
    }
  }
};
