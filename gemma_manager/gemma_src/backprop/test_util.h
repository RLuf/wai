// Copyright 2024 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_GEMMA_CPP_GEMMA_TEST_UTIL_H_
#define THIRD_PARTY_GEMMA_CPP_GEMMA_TEST_UTIL_H_

#include <stddef.h>

#include <cmath>
#include <complex>
#include <random>
#include <vector>

#include "gtest/gtest.h"
#include "compression/compress.h"
#include "gemma/configs.h"
#include "gemma/weights.h"
#include "hwy/contrib/thread_pool/thread_pool.h"

namespace gcpp {

template <typename T>
void RandInit(MatPtrT<T>& x, T stddev, std::mt19937& gen) {
  std::normal_distribution<T> dist(0.0, stddev);
  for (size_t i = 0; i < x.NumElements(); ++i) {
    x.At(i) = dist(gen);
  }
}

// TODO: make a member of Layer<T>.
template <typename T>
void RandInit(LayerWeightsPtrs<T>& w, T stddev, std::mt19937& gen) {
  RandInit(w.pre_attention_norm_scale, stddev, gen);
  RandInit(w.attn_vec_einsum_w, stddev, gen);
  RandInit(w.qkv_einsum_w, stddev, gen);
  RandInit(w.pre_ffw_norm_scale, stddev, gen);
  RandInit(w.gating_einsum_w, stddev, gen);
  RandInit(w.linear_w, stddev, gen);
}

template <typename T>
void RandInit(ModelWeightsPtrs<T>& w, T stddev, std::mt19937& gen) {
  const size_t kLayers = w.c_layers.size();
  RandInit(w.embedder_input_embedding, stddev, gen);
  RandInit(w.final_norm_scale, stddev, gen);
  for (size_t i = 0; i < kLayers; ++i) {
    RandInit(*w.GetLayer(i), stddev, gen);
  }
}

template <typename T, typename U>
void Complexify(const MatPtrT<T>& x, MatPtrT<std::complex<U>>& c_x) {
  for (size_t i = 0; i < x.NumElements(); ++i) {
    c_x.At(i) = std::complex<U>(x.At(i), 0.0);
  }
}

template <typename T, typename U>
void Complexify(const LayerWeightsPtrs<T>& w, LayerWeightsPtrs<U>& c_w) {
  Complexify(w.pre_attention_norm_scale, c_w.pre_attention_norm_scale);
  Complexify(w.attn_vec_einsum_w, c_w.attn_vec_einsum_w);
  Complexify(w.qkv_einsum_w, c_w.qkv_einsum_w);
  Complexify(w.pre_ffw_norm_scale, c_w.pre_ffw_norm_scale);
  Complexify(w.gating_einsum_w, c_w.gating_einsum_w);
  Complexify(w.linear_w, c_w.linear_w);
}

template <typename T, typename U>
void Complexify(const ModelWeightsPtrs<T>& w, ModelWeightsPtrs<U>& c_w) {
  const size_t kLayers = w.c_layers.size();
  Complexify(w.embedder_input_embedding, c_w.embedder_input_embedding);
  Complexify(w.final_norm_scale, c_w.final_norm_scale);
  for (size_t i = 0; i < kLayers; ++i) {
    Complexify(*w.GetLayer(i), *c_w.GetLayer(i));
  }
}

// Somewhat duplicates ModelWeightsStorage, but that has neither double nor
// complex types allowed and it would cause code bloat to add them there.
template <typename T>
class WeightsWrapper {
 public:
  explicit WeightsWrapper(const ModelConfig& config)
      : pool_(0), weights_(config) {
    weights_.Allocate(data_, pool_);
  }

  const ModelWeightsPtrs<T>& get() const { return weights_; }
  ModelWeightsPtrs<T>& get() { return weights_; }
  void ZeroInit() { weights_.ZeroInit(); }
  void CopyFrom(const WeightsWrapper<T>& other) {
    weights_.CopyFrom(other.weights_);
  }

 private:
  hwy::ThreadPool pool_;
  std::vector<MatStorage> data_;
  ModelWeightsPtrs<T> weights_;
};

template <typename T, typename U>
void TestNear(const MatPtrT<T>& actual, const MatPtrT<U>& expected,
              double max_abs_err, double max_rel_err, int line) {
  double sum0 = 0;
  double sum1 = 0;
  double sum01 = 0;
  for (size_t i = 0; i < actual.NumElements(); ++i) {
    sum0 += actual.At(i) * actual.At(i);
    sum1 += expected.At(i) * expected.At(i);
    sum01 += actual.At(i) * expected.At(i);
    ASSERT_NEAR(actual.At(i), expected.At(i),
                std::max(max_abs_err, std::abs(expected.At(i)) * max_rel_err))
        << "line: " << line << " dim=" << expected.NumElements() << " i=" << i;
  }
  if (sum0 > 1e-40) {
    double norm_dot = sum01 / std::sqrt(sum0) / std::sqrt(sum1);
    ASSERT_NEAR(norm_dot, 1.0, 1e-7)
        << "line: " << line << " sum0: " << sum0  << " sum1: " << sum1
        << " sum01: " << sum01;
  }
}

// Compute gradient with the finite difference method in the complex plane.
// If f : R->R is the tested function and F : C->C is its extension on the
// complex plane so that F is complex differentiable in x, then
//
//   F(x + ih) = F(x) + ih F'(x) + O(h^2) F''(x)
//
// which means that
//
//   F'(x) ~= Imag(F(x + ih)) / h
//
// This method is more numerically stable than the real-valued finite difference
// method since we don't need to subtract floating point numbers that are near
// to each other.
template <typename FUNC, typename T, typename U>
void TestGradient(const MatPtrT<T>& grad, MatPtrT<std::complex<U>>& x,
                  FUNC func, U step, T max_abs_err, T max_rel_err, int line) {
  MatStorageT<T> exp_grad("exp_grad", x.Rows(), x.Cols());
  const U inv_step = 1.0 / step;
  for (size_t i = 0; i < x.NumElements(); ++i) {
    const U x0 = std::real(x.At(i));
    const std::complex<U> x1 = std::complex<U>(x0, step);
    x.At(i) = x1;
    const std::complex<U> f1 = func();
    exp_grad.At(i) = std::imag(f1) * inv_step;
    x.At(i) = x0;
  }
  TestNear(grad, exp_grad, max_abs_err, max_rel_err, line);
}

template <typename FUNC>
void TestGradient(const MatPtrT<float>& grad, MatPtrT<std::complex<float>>& x,
                  FUNC func, float max_abs_err, float max_rel_error, int line) {
  TestGradient(grad, x, func, 1e-30f, max_abs_err, max_rel_error, line);
}

template <typename FUNC, typename T>
void TestGradient(const MatPtrT<T>& grad, MatPtrT<std::complex<double>>& x,
                  FUNC func, T max_abs_err, T max_rel_error, int line) {
  TestGradient(grad, x, func, 1e-50, max_abs_err, max_rel_error, line);
}

template <typename T, typename U, typename FUNC>
void TestGradient(const LayerWeightsPtrs<T>& grad,
                  LayerWeightsPtrs<U>& c_weights, FUNC func, T max_err) {
  TestGradient(grad.pre_attention_norm_scale,
               c_weights.pre_attention_norm_scale,
               func, max_err, max_err, __LINE__);
  TestGradient(grad.attn_vec_einsum_w, c_weights.attn_vec_einsum_w,
               func, max_err, max_err, __LINE__);
  TestGradient(grad.qkv_einsum_w, c_weights.qkv_einsum_w,
               func, max_err, max_err, __LINE__);
  TestGradient(grad.pre_ffw_norm_scale, c_weights.pre_ffw_norm_scale,
               func, max_err, max_err, __LINE__);
  TestGradient(grad.gating_einsum_w, c_weights.gating_einsum_w,
               func, max_err, max_err, __LINE__);
  TestGradient(grad.linear_w, c_weights.linear_w,
               func, max_err, max_err, __LINE__);
}

template <typename T, typename U, typename FUNC>
void TestGradient(const ModelWeightsPtrs<T>& grad,
                  ModelWeightsPtrs<U>& c_weights, FUNC func, T max_err) {
  TestGradient(grad.embedder_input_embedding,
                 c_weights.embedder_input_embedding,
                 func,  2 * max_err, max_err, __LINE__);
  TestGradient(grad.final_norm_scale, c_weights.final_norm_scale,
               func, max_err, max_err, __LINE__);
  for (size_t i = 0; i < grad.c_layers.size(); ++i) {
    TestGradient(*grad.GetLayer(i), *c_weights.GetLayer(i), func, max_err);
  }
}

}  // namespace gcpp

#endif  // THIRD_PARTY_GEMMA_CPP_GEMMA_TEST_UTIL_H_
