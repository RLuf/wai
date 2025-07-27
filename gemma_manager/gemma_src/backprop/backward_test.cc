// Copyright 2023 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HWY_DISABLED_TARGETS
#define HWY_DISABLED_TARGETS HWY_SCALAR
#endif

#include <stddef.h>

#include <complex>
#include <cstdlib>  // std::abs
#include <random>
#include <vector>

#include "backprop/activations.h"
#include "backprop/backward_scalar.h"
#include "backprop/common_scalar.h"
#include "backprop/forward_scalar.h"
#include "backprop/prompt.h"
#include "backprop/sampler.h"
#include "backprop/test_util.h"
#include "gemma/configs.h"
#include "ops/ops.h"
#include "util/threading.h"
#include "hwy/base.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backprop/backward_test.cc"  //NOLINT
// clang-format on
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"
// After highway.h
#include "backprop/backward-inl.h"
#include "backprop/forward-inl.h"
#include "compression/compress.h"
#include "ops/ops-inl.h"
#include "util/allocator.h"

HWY_BEFORE_NAMESPACE();
namespace gcpp {
namespace HWY_NAMESPACE {

void TestMatMulVJP() {
  static const size_t kRows = 8;
  static const size_t kCols = 64;
  static const size_t kTokens = 5;
  const BoundedTopology topology(BoundedSlice(0, 1), BoundedSlice(0, 8));
  Allocator::Init(topology);
  gcpp::NestedPools pools(topology, 1, /*pin=*/Tristate::kFalse);
  std::mt19937 gen(42);
  MatStorageT<float> weights("weights", kRows, kCols);
  MatStorageT<float> x("x", kTokens, kCols);
  MatStorageT<float> dy("dy", kTokens, kRows);
  MatStorageT<float> grad("grad", kRows, kCols);
  MatStorageT<float> dx("dx", kTokens, kCols);
  MatStorageT<float> grad_scalar("grad_scalar", kRows, kCols);
  MatStorageT<float> dx_scalar("dx_scalar", kTokens, kCols);
  using TC = std::complex<double>;
  MatStorageT<TC> c_weights("c_weights", kRows, kCols);
  MatStorageT<TC> c_x("c_x", kTokens, kCols);
  MatStorageT<TC> c_y("c_y", kTokens, kRows);

  for (int iter = 0; iter < 10; ++iter) {
    RandInit(weights, 1.0f * (1 << iter), gen);
    RandInit(x, 1.0f * (1 << iter), gen);
    RandInit(dy, 1.0f, gen);
    Complexify(weights, c_weights);
    Complexify(x, c_x);
    auto func = [&]() {
      MatMulT(c_weights.data(), c_x.data(), c_y.data(), kRows, kCols, kTokens);
      return DotT(dy.data(), c_y.data(), kTokens * kRows);
    };

    grad.ZeroInit();
    MatMulVJP(weights.data(), x.data(), dy.data(), kCols, kRows, kTokens,
              grad.data(), dx.data(), pools.Pool());
    TestGradient(dx, c_x, func, 5e-5f, 5e-5f, __LINE__);
    TestGradient(grad, c_weights, func, 5e-5f, 5e-5f, __LINE__);

    grad_scalar.ZeroInit();
    MatMulVJPT(weights.data(), x.data(), dy.data(), grad_scalar.data(),
               dx_scalar.data(), kRows, kCols, kTokens);
    TestNear(dx, dx_scalar, 5e-5, 1e-4, __LINE__);
    TestNear(grad, grad_scalar, 5e-5, 5e-5, __LINE__);
  }
}

void TestMultiHeadMatMulVJP() {
  static const size_t kRows = 2;
  static const size_t kCols = 16;
  static const size_t kHeads = 4;
  static const size_t kTokens = 3;
  const BoundedTopology topology(BoundedSlice(0, 1), BoundedSlice(0, 8));
  Allocator::Init(topology);
  gcpp::NestedPools pools(topology, 1, /*pin=*/Tristate::kFalse);
  std::mt19937 gen(42);
  MatStorageT<float> weights("weights", kRows, kCols * kHeads);
  MatStorageT<float> x("x", kTokens, kCols * kHeads);
  MatStorageT<float> grad("grad", kRows, kCols * kHeads);
  MatStorageT<float> dx("dx", kTokens, kCols * kHeads);
  MatStorageT<float> dy("dy", kTokens, kRows);
  MatStorageT<float> grad_scalar("grad_scalar", kRows, kCols * kHeads);
  MatStorageT<float> dx_scalar("dx_scalar", kTokens, kCols * kHeads);
  using TC = std::complex<double>;
  MatStorageT<TC> c_weights("c_weights", kRows, kCols * kHeads);
  MatStorageT<TC> c_x("c_x", kTokens, kCols * kHeads);
  MatStorageT<TC> c_y("c_y", kTokens, kRows);

  for (int iter = 0; iter < 10; ++iter) {
    RandInit(weights, 1.0f * (1 << iter), gen);
    RandInit(x, 1.0f * (1 << iter), gen);
    RandInit(dy, 1.0f, gen);
    Complexify(weights, c_weights);
    Complexify(x, c_x);
    auto func = [&]() {
      MultiHeadMatMul(c_weights.data(), c_x.data(), c_y.data(), kHeads, kRows,
                      kCols, kTokens);
      return DotT(dy.data(), c_y.data(), kTokens * kRows);
    };

    grad.ZeroInit();
    MultiHeadMatMulVJP(weights.data(), x.data(), dy.data(), kHeads, kCols,
                       kRows, kTokens, grad.data(), dx.data(), pools.Pool());
    TestGradient(dx, c_x, func, 5e-5f, 5e-5f, __LINE__);
    TestGradient(grad, c_weights, func, 5e-5f, 5e-5f, __LINE__);

    grad_scalar.ZeroInit();
    MultiHeadMatMulVJPT(weights.data(), x.data(), dy.data(), grad_scalar.data(),
                        dx_scalar.data(), kHeads, kRows, kCols, kTokens);
    TestNear(dx, dx_scalar, 5e-5, 5e-5, __LINE__);
    TestNear(grad, grad_scalar, 5e-5, 5e-5, __LINE__);
  }
}

void TestRMSNormVJP() {
  static const size_t K = 2;
  static const size_t N = 64;
  const BoundedTopology topology(BoundedSlice(0, 1), BoundedSlice(0, 8));
  Allocator::Init(topology);
  gcpp::NestedPools pools(topology, 1, /*pin=*/Tristate::kFalse);
  std::mt19937 gen(42);
  MatStorageT<float> weights("weights", N, 1);
  MatStorageT<float> x("x", K, N);
  MatStorageT<float> grad("grad", N, 1);
  MatStorageT<float> dx("dx", K, N);
  MatStorageT<float> dy("dy", K, N);
  MatStorageT<float> grad_scalar("grad_scalar", N, 1);
  MatStorageT<float> dx_scalar("dx_scalar", K, N);
  using TC = std::complex<double>;
  MatStorageT<TC> c_weights("c_weights", N, 1);
  MatStorageT<TC> c_x("c_x", K, N);
  MatStorageT<TC> c_y("c_y", K, N);

  for (int iter = 0; iter < 10; ++iter) {
    RandInit(weights, 1.0f * (1 << iter), gen);
    RandInit(x, 1.0f * (1 << iter), gen);
    RandInit(dy, 1.0f, gen);
    Complexify(weights, c_weights);
    Complexify(x, c_x);
    auto func = [&]() {
      RMSNormT(c_weights.data(), c_x.data(), c_y.data(), N, K);
      return DotT(dy.data(), c_y.data(), K * N);
    };

    grad.ZeroInit();
    RMSNormVJP(weights.data(), x.data(), dy.data(), N, K, grad.data(),
               dx.data(), pools.Pool());
    TestGradient(dx, c_x, func, 5e-5f, 5e-5f, __LINE__);
    TestGradient(grad, c_weights, func, 5e-5f, 5e-5f, __LINE__);

    grad_scalar.ZeroInit();
    RMSNormVJPT(weights.data(), x.data(), dy.data(), grad_scalar.data(),
                dx_scalar.data(), N, K);
    TestNear(dx, dx_scalar, 0, 2e-5, __LINE__);
    TestNear(grad, grad_scalar, 0, 2e-5, __LINE__);
  }
}

static ModelConfig TestConfig() {
  ModelConfig config;
  config.scale_names = {"att_ein",      "qkv_ein",   "gr_lin_x_w", "gr_lin_y_w",
                        "gr_lin_out_w", "gr_gate_w", "gating_ein", "linear_w"};
  config.model_dim = 32;
  config.vocab_size = 16;
  config.seq_len = 24;
  LayerConfig layer_config;
  layer_config.model_dim = config.model_dim;
  layer_config.ff_hidden_dim = 64;
  layer_config.heads = 3;
  layer_config.kv_heads = 1;
  layer_config.qkv_dim = 16;
  config.layer_configs = {2, layer_config};
  config.num_tensor_scales = 4 * config.layer_configs.size();
  config.query_scale = QueryScaleType::SqrtKeySize;
  config.attention_window_sizes = FixedAttentionWindowSizes<2>(32);
  // This is required for optimize_test to pass.
  config.att_cap = 50.0f;
  config.final_cap = 30.0f;
  return config;
}

void TestEndToEnd() {
  std::mt19937 gen(42);
  const BoundedTopology topology(BoundedSlice(0, 1), BoundedSlice(0, 1));
  Allocator::Init(topology);
  gcpp::NestedPools pools(topology, 1, /*pin=*/Tristate::kFalse);
  ModelConfig config = TestConfig();
  WeightsWrapper<float> weights(config);
  WeightsWrapper<float> grad(config);
  ForwardPass<float> forward0(config);
  ForwardPass<float> forward1(config);
  ForwardPass<float> backward(config);
  using TC = std::complex<double>;
  WeightsWrapper<TC> c_weights(config);
  ForwardPass<TC> c_forward(config);

  ReverseSequenceSampler training_task({0, 0, 1, 1});
  std::vector<Prompt> batch = training_task.SampleBatch(3, gen);

  RowVectorBatch<float> inv_timescale = CreateInvTimescale(
      config.layer_configs[0].qkv_dim,
      config.layer_configs[0].post_qk == PostQKType::HalfRope);
  for (const Prompt& prompt : batch) {
    ReverseSequenceSampler::LogPrompt(prompt);
    RandInit(weights.get(), 1.0f, gen);

    float loss0 = CrossEntropyLossForwardPass(prompt, weights.get(), forward0);

    float loss1 = CrossEntropyLossForwardPass(
        prompt.tokens, prompt.context_size, weights.get(), forward1,
        inv_timescale, pools.Pool());

    EXPECT_NEAR(loss1, loss0, std::abs(loss0) * 2e-5);

    grad.ZeroInit();
    CrossEntropyLossBackwardPassInl(prompt, weights.get(), forward1, grad.get(),
                                    backward, inv_timescale, pools.Pool());

    Complexify(weights.get(), c_weights.get());
    auto func = [&]() {
      return CrossEntropyLossForwardPass(prompt, c_weights.get(), c_forward);
    };

    TestGradient(grad.get(), c_weights.get(), func, 2e-3f);
  }
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace gcpp
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace gcpp {
HWY_BEFORE_TEST(BackwardTest);
HWY_EXPORT_AND_TEST_P(BackwardTest, TestMatMulVJP);
HWY_EXPORT_AND_TEST_P(BackwardTest, TestMultiHeadMatMulVJP);
HWY_EXPORT_AND_TEST_P(BackwardTest, TestRMSNormVJP);
HWY_EXPORT_AND_TEST_P(BackwardTest, TestEndToEnd);
HWY_AFTER_TEST();

}  // namespace gcpp

#endif
