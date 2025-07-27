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

#ifndef THIRD_PARTY_GEMMA_CPP_UTIL_ALLOCATOR_H_
#define THIRD_PARTY_GEMMA_CPP_UTIL_ALLOCATOR_H_

// Allocator with support for sharding tensors across NUMA nodes.

#include <stddef.h>
#include <stdint.h>

// IWYU pragma: begin_exports
#include <memory>  // std::unique_ptr

#include "util/basics.h"
#include "util/topology.h"
#include "hwy/base.h"
// IWYU pragma: end_exports

#include "hwy/aligned_allocator.h"

namespace gcpp {

// Points to an adapter lambda that calls `FreeAlignedBytes` or `munmap`. The
// `bytes` argument is required for the latter.
using FreeFunc = void (*)(void* mem, size_t bytes);

// Custom deleter for std::unique_ptr that calls `FreeFunc`. T is POD.
class DeleterFree {
 public:
  // `MatStorageT` requires this to be default-constructible.
  DeleterFree() : free_func_(nullptr), bytes_(0) {}
  DeleterFree(FreeFunc free_func, size_t bytes)
      : free_func_(free_func), bytes_(bytes) {}

  template <typename T>
  void operator()(T* p) const {
    free_func_(p, bytes_);
  }

 private:
  FreeFunc free_func_;
  size_t bytes_;
};

// Wrapper that also calls the destructor for non-POD T.
class DeleterDtor {
 public:
  DeleterDtor() {}
  DeleterDtor(size_t num, DeleterFree free) : num_(num), free_(free) {}

  template <typename T>
  void operator()(T* p) const {
    for (size_t i = 0; i < num_; ++i) {
      p[i].~T();
    }
    free_(p);
  }

 private:
  size_t num_;  // not the same as free_.bytes_ / sizeof(T)!
  DeleterFree free_;
};

// Unique (move-only) pointer to an aligned array of POD T.
template <typename T>
using AlignedPtr = std::unique_ptr<T[], DeleterFree>;
// Unique (move-only) pointer to an aligned array of non-POD T.
template <typename T>
using AlignedClassPtr = std::unique_ptr<T[], DeleterDtor>;

// Both allocation, binding, and row accessors depend on the sizes of memory
// pages and cache lines. To avoid having to pass `Allocator&` everywhere, we
// use `Monostate` (static members).
class Allocator {
 public:
  // Must be called at least once before any other function. Not thread-safe,
  // hence only call this from the main thread.
  // TODO: remove enable_bind once Gemma tensors support binding.
  static void Init(const BoundedTopology& topology, bool enable_bind = false);

  // Bytes per cache line, or a reasonable guess if unknown. Used to choose
  // ranges such that there will be no false sharing.
  static size_t LineBytes();
  // Bytes per full vector. Used to compute loop steps.
  static size_t VectorBytes();
  // Work granularity that avoids false sharing and partial vectors.
  static size_t StepBytes();  // = HWY_MAX(LineBytes(), VectorBytes())
  // Granularity like `StepBytes()`, but when NUMA may be involved.
  static size_t QuantumBytes();
  // Upper bound on `QuantumBytes()`, for stack allocations.
  static constexpr size_t MaxQuantumBytes() { return 4096; }
  static size_t QuantumSteps();  // = QuantumBytes() / StepBytes()

  // L1 and L2 are typically per core.
  static size_t L1Bytes();
  static size_t L2Bytes();
  // Clusters often share an L3. We return the total size per package.
  static size_t L3Bytes();

  // Returns pointer aligned to `QuantumBytes()`.
  template <typename T>
  static AlignedPtr<T> Alloc(size_t num) {
    constexpr size_t kSize = sizeof(T);
    constexpr bool kIsPow2 = (kSize & (kSize - 1)) == 0;
    constexpr size_t kBits = hwy::detail::ShiftCount(kSize);
    static_assert(!kIsPow2 || (1ull << kBits) == kSize, "ShiftCount has a bug");
    const size_t bytes = kIsPow2 ? num << kBits : num * kSize;
    // Fail if the `bytes = num * kSize` computation overflowed.
    const size_t check = kIsPow2 ? bytes >> kBits : bytes / kSize;
    if (check != num) return AlignedPtr<T>();

    PtrAndDeleter pd = AllocBytes(bytes);
    return AlignedPtr<T>(static_cast<T*>(pd.p), pd.deleter);
  }

  // Same as Alloc, but calls constructor(s) with `args`.
  template <typename T, class... Args>
  static AlignedClassPtr<T> AllocClasses(size_t num, Args&&... args) {
    constexpr size_t kSize = sizeof(T);
    constexpr bool kIsPow2 = (kSize & (kSize - 1)) == 0;
    constexpr size_t kBits = hwy::detail::ShiftCount(kSize);
    static_assert(!kIsPow2 || (1ull << kBits) == kSize, "ShiftCount has a bug");
    const size_t bytes = kIsPow2 ? num << kBits : num * kSize;
    // Fail if the `bytes = num * kSize` computation overflowed.
    const size_t check = kIsPow2 ? bytes >> kBits : bytes / kSize;
    if (check != num) return AlignedClassPtr<T>();

    PtrAndDeleter pd = AllocBytes(bytes);
    T* p = static_cast<T*>(pd.p);
    for (size_t i = 0; i < num; ++i) {
      new (p + i) T(std::forward<Args>(args)...);
    }
    return AlignedClassPtr<T>(p, DeleterDtor(num, pd.deleter));
  }

  // Returns whether `BindMemory` can/should be called, i.e. we have page-level
  // control over memory placement and multiple packages and NUMA nodes.
  static bool ShouldBind();

  // Attempts to move(!) `[p, p + bytes)` to the given NUMA node, which is
  // typically `BoundedTopology::GetCluster(package_idx, cluster_idx).node`.
  // Writes zeros to SOME of the memory. Only call if `ShouldBind()`.
  // `p` and `bytes` must be multiples of `QuantumBytes()`.
  static bool BindMemory(void* p, size_t bytes, size_t node);

 private:
  // Type-erased so this can be implemented in allocator.cc.
  struct PtrAndDeleter {
    void* p;
    DeleterFree deleter;
  };
  static PtrAndDeleter AllocBytes(size_t bytes);
};

// Value of `stride` to pass to `RowVectorBatch` to enable the "cyclic offsets"
// optimization. If `Allocator::ShouldBind()`, `Allocator::QuantumBytes()` is
// typically 4KiB. To avoid remote accesses, we would thus pad each row to that,
// which results in 4K aliasing and/or cache conflict misses. `RowPtr` is able
// to prevent that by pulling rows forward by a cyclic offset, which is still a
// multiple of the cache line size. This requires an additional
// `Allocator::QuantumBytes()` of padding after also rounding up to that.
template <typename T>
constexpr size_t StrideForCyclicOffsets(size_t cols) {
  const size_t quantum = Allocator::MaxQuantumBytes() / sizeof(T);
  return hwy::RoundUpTo(cols, quantum) + quantum;
}

// Owns dynamically-allocated aligned memory for a batch of row vectors.
// This can be seen as a (batch_size x cols) matrix. Unlike `RowPtr`, this owns
// the memory.
template <typename T>
class RowVectorBatch {
 public:
  // Default ctor for Activations ctor.
  RowVectorBatch() = default;
  // Main ctor, called from Activations::Allocate. If `stride` = 0, the default,
  // we default to tightly packed rows (`stride = cols`).
  // WARNING: not all call sites support `stride` != cols.
  // TODO: once they do, remove stride and behave like AllocateAlignedRows here.
  RowVectorBatch(Extents2D extents, size_t stride = 0) : extents_(extents) {
    if (stride == 0) {
      stride_ = extents_.cols;
    } else {
      HWY_ASSERT(stride >= extents_.cols);
      stride_ = stride;
    }
    // Allow binding the entire matrix.
    const size_t padded = hwy::RoundUpTo(extents_.rows * stride_,
                                         Allocator::QuantumBytes() / sizeof(T));
    mem_ = Allocator::Alloc<T>(padded);
  }

  // Move-only
  RowVectorBatch(RowVectorBatch&) noexcept = delete;
  RowVectorBatch& operator=(RowVectorBatch&) noexcept = delete;
  RowVectorBatch(RowVectorBatch&&) noexcept = default;
  RowVectorBatch& operator=(RowVectorBatch&&) noexcept = default;

  size_t BatchSize() const { return extents_.rows; }
  size_t Cols() const { return extents_.cols; }
  size_t Stride() const { return stride_; }
  Extents2D Extents() const { return extents_; }

  // Returns the given row vector of length `Cols()`.
  T* Batch(size_t batch_idx) {
    HWY_DASSERT(batch_idx < BatchSize());
    return mem_.get() + batch_idx * stride_;
  }
  const T* Batch(size_t batch_idx) const {
    HWY_DASSERT(batch_idx < BatchSize());
    return mem_.get() + batch_idx * stride_;
  }

  // For MatMul or other operations that process the entire batch at once.
  // TODO: remove once we only use Mat.
  T* All() { return mem_.get(); }
  const T* Const() const { return mem_.get(); }
  size_t NumBytes() const { return BatchSize() * stride_ * sizeof(T); }

 private:
  AlignedPtr<T> mem_;
  Extents2D extents_;
  size_t stride_;
};

// Returns `num` rounded up to an odd number of cache lines. This is used to
// compute strides. An odd number of cache lines prevents 2K aliasing and is
// coprime with the cache associativity, which reduces conflict misses.
template <typename T>
static HWY_INLINE size_t RoundUpToOddLines(size_t num, size_t line_bytes) {
  HWY_DASSERT(line_bytes >= 32);
  HWY_DASSERT(line_bytes % sizeof(T) == 0);
  const size_t lines = hwy::DivCeil(num * sizeof(T), line_bytes);
  const size_t padded_num = (lines | 1) * line_bytes / sizeof(T);
  HWY_DASSERT(padded_num >= num);
  return padded_num;
}

template <typename T>
RowVectorBatch<T> AllocateAlignedRows(Extents2D extents) {
  return RowVectorBatch<T>(extents, StrideForCyclicOffsets<T>(extents.cols));
}

// Lightweight version of `MatPtr` used for the C argument of `MatMul`, because
// it is always float and does not support compressed T, but does support an
// arbitrary stride >= cols.
#pragma pack(push, 1)  // power of two size
template <typename T>
class RowPtr {
 public:
  RowPtr() = default;  // for `MMPtrs`.
  RowPtr(T* HWY_RESTRICT row0, size_t cols, size_t stride)
      : row0_(row0),
        stride_(stride),
        step_(static_cast<uint32_t>(Allocator::StepBytes())),
        cols_(static_cast<uint32_t>(cols)),
        row_mask_(Allocator::QuantumSteps() - 1) {
    HWY_DASSERT(stride >= cols);
    HWY_DASSERT(row_mask_ != ~size_t{0});
    if constexpr (HWY_IS_DEBUG_BUILD) {
      if (stride < StrideForCyclicOffsets<T>(cols)) {
        static bool once;
        if (!once) {
          once = true;
          HWY_WARN(
              "Check why RowPtr stride=%zu < StrideForCyclicOffsets(cols=%zu), "
              "T=%zu; this forces us to disable cyclic offsets.",
              stride, cols, sizeof(T));
        }
        row_mask_ = 0;
      }
    }
  }
  RowPtr(T* HWY_RESTRICT row0, size_t cols) : RowPtr(row0, cols, cols) {}

  T* HWY_RESTRICT Row(size_t r) const {
    // How much of the previous row's padding to consume.
    const size_t pad_bytes = (r & row_mask_) * step_;
    HWY_DASSERT(pad_bytes < Allocator::QuantumBytes());
    return row0_ + stride_ * r - pad_bytes;
  }
  size_t Cols() const { return cols_; }

  size_t Stride() const { return stride_; }
  void SetStride(size_t stride) {
    HWY_DASSERT(stride >= Cols());
    stride_ = stride;
    // The caller might not have padded enough, so disable the padding in Row().
    // Rows will now be exactly `stride` elements apart. This is used when
    // writing to the KV cache via MatMul.
    row_mask_ = 0;
  }

  // Returns 2D subrange whose top-left is `r, c` and width is `cols`.
  RowPtr<T> View(size_t r, size_t c, size_t cols) const {
    HWY_DASSERT(c < cols_);
    HWY_DASSERT(cols <= cols_ - c);
    return RowPtr<T>(Row(r) + c, cols, stride_);
  }

 private:
  T* HWY_RESTRICT row0_;
  size_t stride_;
  uint32_t step_;  // Copy from Allocator::LineBytes() to improve locality.
  uint32_t cols_;
  size_t row_mask_;
};
#pragma pack(pop)

using RowPtrBF = RowPtr<BF16>;
using RowPtrF = RowPtr<float>;
using RowPtrD = RowPtr<double>;

// For C argument to MatMul.
template <typename T>
RowPtr<T> RowPtrFromBatch(RowVectorBatch<T>& row_vectors) {
  return RowPtr<T>(row_vectors.All(), row_vectors.Cols(), row_vectors.Stride());
}

}  // namespace gcpp

#endif  // THIRD_PARTY_GEMMA_CPP_UTIL_ALLOCATOR_H_
