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

#include "gemma/weights.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "compression/blob_store.h"
#include "compression/compress-inl.h"
#include "compression/compress.h"
#include "compression/io.h"  // Path
#include "compression/shared.h"
#include "gemma/common.h"
#include "gemma/configs.h"
#include "hwy/aligned_allocator.h"
#include "hwy/base.h"  // HWY_ABORT
#include "hwy/contrib/thread_pool/thread_pool.h"
#include "hwy/highway.h"
#include "hwy/profiler.h"
#include "hwy/stats.h"

namespace gcpp {

template <typename T>
struct TensorLoader {
  void operator()(ModelWeightsPtrs<T>& weights, ForEachType fet,
                  ReadFromBlobStore& loader) {
    weights.ForEachTensor(
        {&weights}, fet,
        [&loader](const char* name, hwy::Span<MatPtr*> tensors) {
          loader(name, tensors);
        });
  }
};

BlobError ModelWeightsStorage::Load(const Path& weights, Model model_type,
                                    Type weight_type, PromptWrapping wrapping,
                                    hwy::ThreadPool& pool,
                                    std::string* tokenizer_proto) {
  PROFILER_ZONE("Startup.LoadModelWeightsPtrs");
  if (!weights.Exists()) {
    HWY_ABORT("The model weights file '%s' does not exist.",
              weights.path.c_str());
  }
  ReadFromBlobStore loader(weights);
  ForEachType fet =
      loader.HaveToc() ? ForEachType::kLoadWithToc : ForEachType::kLoadNoToc;
  std::vector<float> scales;
  if (fet == ForEachType::kLoadWithToc) {
    BlobError err = loader.LoadConfig(config_);
    if (err != 0 || config_.model_dim == 0) {
      fprintf(stderr, "Failed to load model config: %d\n", err);
      return err;
    }
    if (tokenizer_proto != nullptr) {
      err = loader.LoadTokenizer(*tokenizer_proto);
      if (err != 0) {
        fprintf(stderr, "Failed to load tokenizer: %d\n", err);
        return err;
      }
    }
  } else {
    if (weight_type == Type::kUnknown || model_type == Model::UNKNOWN) {
      fprintf(stderr,
              "weight type (%d) and model type (%d) must be specified when "
              "no config is present in weights file\n",
              static_cast<int>(weight_type), static_cast<int>(model_type));
      return __LINE__;
    }
    // No Toc-> no config.
    config_ = ConfigFromModel(model_type);
    config_.weight = weight_type;
    config_.wrapping = wrapping;
    scales.resize(config_.num_tensor_scales + config_.vit_config.num_scales);
  }
  CreateForType(config_.weight, pool);
  CallForModelWeightT<TensorLoader>(fet, loader);
  if (!scales.empty()) {
    loader.LoadScales(scales.data(), scales.size());
  }
  BlobError err = loader.ReadAll(pool, model_storage_);
  if (err != 0) {
    fprintf(stderr, "Failed to load model weights: %d\n", err);
    return err;
  }
  if (!scales.empty()) {
    GetOrApplyScales(scales);
  }
  if (fet == ForEachType::kLoadNoToc) {
    PROFILER_ZONE("Startup.Reshape");
    AllocAndCopyWithTranspose(pool);
  }
  return 0;
}

template <typename T>
struct TensorSaver {
  // Adds all the tensors to the blob writer.
  void operator()(ModelWeightsPtrs<T>& weights, ForEachType fet,
                  WriteToBlobStore& writer) {
    weights.ForEachTensor(
        {&weights}, fet,
        [&writer](const char* name, hwy::Span<MatPtr*> tensors) {
          tensors[0]->CallUpcasted(writer, name);
        });
  }
};

BlobError ModelWeightsStorage::Save(const std::string& tokenizer,
                                    const Path& weights,
                                    hwy::ThreadPool& pool) {
  WriteToBlobStore writer(pool);
  ForEachType fet = ForEachType::kLoadWithToc;
  CallForModelWeightT<TensorSaver>(fet, writer);
  writer.AddTokenizer(tokenizer);
  int err = writer.WriteAll(weights, &config_);
  if (err != 0) {
    fprintf(stderr, "Failed to write model weights: %d\n", err);
    return err;
  }
  return 0;
}

void ModelWeightsStorage::Allocate(const ModelConfig& config, Type weight_type,
                                   hwy::ThreadPool& pool) {
  PROFILER_ZONE("Startup.AllocateModelWeightsPtrs");
  config_ = config;
  config_.weight = weight_type;
  CreateForType(weight_type, pool);
  if (float_weights_) float_weights_->Allocate(model_storage_, pool);
  if (bf16_weights_) bf16_weights_->Allocate(model_storage_, pool);
  if (sfp_weights_) sfp_weights_->Allocate(model_storage_, pool);
  if (nuq_weights_) nuq_weights_->Allocate(model_storage_, pool);
}

class WeightInitializer {
 public:
  WeightInitializer(std::mt19937& gen) : dist_(0.0f, 1.0f), gen_(gen) {}

  void operator()(const char* name, hwy::Span<MatPtr*> tensors) {
    float* data = tensors[0]->data<float>();
    for (size_t i = 0; i < tensors[0]->NumElements(); ++i) {
      data[i] = dist_(gen_);
    }
    tensors[0]->set_scale(1.0f);
  }

 private:
  std::normal_distribution<float> dist_;
  std::mt19937& gen_;
};

void ModelWeightsStorage::RandInit(std::mt19937& gen) {
  HWY_ASSERT(float_weights_);
  WeightInitializer init(gen);
  ModelWeightsPtrs<float>::ForEachTensor({float_weights_.get()},
                                         ForEachType::kLoadNoToc, init);
}

void ModelWeightsStorage::ZeroInit() {
  if (float_weights_) float_weights_->ZeroInit();
  if (bf16_weights_) bf16_weights_->ZeroInit();
  if (sfp_weights_) sfp_weights_->ZeroInit();
  if (nuq_weights_) nuq_weights_->ZeroInit();
}

void ModelWeightsStorage::GetOrApplyScales(std::vector<float>& scales) {
  if (float_weights_) float_weights_->GetOrApplyScales(scales);
  if (bf16_weights_) bf16_weights_->GetOrApplyScales(scales);
  if (sfp_weights_) sfp_weights_->GetOrApplyScales(scales);
  if (nuq_weights_) nuq_weights_->GetOrApplyScales(scales);
}

void ModelWeightsStorage::AllocAndCopyWithTranspose(hwy::ThreadPool& pool) {
  if (float_weights_)
    float_weights_->AllocAndCopyWithTranspose(pool, model_storage_);
  if (bf16_weights_)
    bf16_weights_->AllocAndCopyWithTranspose(pool, model_storage_);
  if (sfp_weights_)
    sfp_weights_->AllocAndCopyWithTranspose(pool, model_storage_);
  if (nuq_weights_)
    nuq_weights_->AllocAndCopyWithTranspose(pool, model_storage_);
}

void ModelWeightsStorage::CopyWithTranspose(hwy::ThreadPool& pool) {
  if (float_weights_) float_weights_->CopyWithTranspose(pool);
  if (bf16_weights_) bf16_weights_->CopyWithTranspose(pool);
  if (sfp_weights_) sfp_weights_->CopyWithTranspose(pool);
  if (nuq_weights_) nuq_weights_->CopyWithTranspose(pool);
}

namespace {

void LogVec(const char* name, const float* data, size_t len) {
  hwy::Stats stats;
  for (size_t i = 0; i < len; ++i) {
    stats.Notify(data[i]);
  }
  printf("%-20s  %12zu   %13.10f   %8.5f   %13.10f\n",
         name, len, stats.Min(), stats.Mean(), stats.Max());
}

}  // namespace

void ModelWeightsStorage::LogWeightStats() {
  size_t total_weights = 0;
  // Only for float weights.
  ModelWeightsPtrs<float>::ForEachTensor(
      {float_weights_.get()}, ForEachType::kInitNoToc,
      [&total_weights](const char* name, hwy::Span<MatPtr*> tensors) {
        const MatPtr& tensor = *tensors[0];
        if (tensor.scale() != 1.0f) {
          printf("[scale=%f] ", tensor.scale());
        }
        LogVec(name, tensor.data<float>(), tensor.NumElements());
        total_weights += tensor.NumElements();
      });
  printf("%-20s  %12zu\n", "Total", total_weights);
}

void ModelWeightsStorage::CreateForType(Type weight_type,
                                        hwy::ThreadPool& pool) {
  switch (weight_type) {
    case Type::kF32:
      float_weights_ = std::make_unique<ModelWeightsPtrs<float>>(config_);
      break;
    case Type::kBF16:
      bf16_weights_ = std::make_unique<ModelWeightsPtrs<BF16>>(config_);
      break;
    case Type::kSFP:
      sfp_weights_ =
          std::make_unique<ModelWeightsPtrs<SfpStream>>(config_);
      break;
    case Type::kNUQ:
      nuq_weights_ =
          std::make_unique<ModelWeightsPtrs<NuqStream>>(config_);
      break;
    default:
      HWY_ABORT("Weight type %d unsupported.", static_cast<int>(weight_type));
  }
}

template <>
void LayerWeightsPtrs<NuqStream>::Reshape(MatStorage* storage) {
  if (attn_vec_einsum_w.data() == nullptr) return;

  const size_t model_dim = layer_config.model_dim;
  const size_t heads = layer_config.heads;
  const size_t qkv_dim = layer_config.qkv_dim;

  // Reshape [kHeads, kModelDim, kQKVDim] to [kModelDim, kHeads * kQKVDim].
  if (storage != nullptr) {
    storage->Allocate();
    att_weights.SetPtr(*storage);
  }

  const hwy::HWY_NAMESPACE::ScalableTag<float> df;

  hwy::AlignedFreeUniquePtr<float[]> attn_vec_einsum_w_tmp =
      hwy::AllocateAligned<float>(model_dim * heads * qkv_dim);
  hwy::AlignedFreeUniquePtr<float[]> att_weights_tmp =
      hwy::AllocateAligned<float>(model_dim * heads * qkv_dim);

  HWY_NAMESPACE::DecompressAndZeroPad(
      df, MakeSpan(attn_vec_einsum_w.data(), model_dim * heads * qkv_dim), 0,
      attn_vec_einsum_w_tmp.get(), model_dim * heads * qkv_dim);

  for (size_t m = 0; m < model_dim; ++m) {
    float* HWY_RESTRICT out_row = att_weights_tmp.get() + m * heads * qkv_dim;
    for (size_t h = 0; h < heads; ++h) {
      hwy::CopyBytes(
          attn_vec_einsum_w_tmp.get() + h * model_dim * qkv_dim + m * qkv_dim,
          out_row + h * qkv_dim, qkv_dim * sizeof(float));
    }
  }

  CompressWorkingSet work;
  hwy::ThreadPool pool(0);

  HWY_NAMESPACE::Compress(
      att_weights_tmp.get(), model_dim * heads * qkv_dim, work,
      MakeSpan(att_weights.data(), model_dim * heads * qkv_dim),
      /*packed_ofs=*/0, pool);

  att_weights.set_scale(attn_vec_einsum_w.scale());
}

}  // namespace gcpp
