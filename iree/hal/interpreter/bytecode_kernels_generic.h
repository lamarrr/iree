// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IREE_HAL_INTERPRETER_BYTECODE_KERNELS_GENERIC_H_
#define IREE_HAL_INTERPRETER_BYTECODE_KERNELS_GENERIC_H_

#include "absl/container/flat_hash_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "iree/base/status.h"

namespace iree {
namespace hal {
namespace kernels {

template <typename T>
Status CompareEQ::Execute(absl::Span<const T> lhs_buffer,
                          absl::Span<const T> rhs_buffer,
                          absl::Span<uint8_t> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] == rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status CompareNE::Execute(absl::Span<const T> lhs_buffer,
                          absl::Span<const T> rhs_buffer,
                          absl::Span<uint8_t> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] != rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status CompareLT::Execute(absl::Span<const T> lhs_buffer,
                          absl::Span<const T> rhs_buffer,
                          absl::Span<uint8_t> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] < rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status CompareLE::Execute(absl::Span<const T> lhs_buffer,
                          absl::Span<const T> rhs_buffer,
                          absl::Span<uint8_t> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] <= rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status CompareGT::Execute(absl::Span<const T> lhs_buffer,
                          absl::Span<const T> rhs_buffer,
                          absl::Span<uint8_t> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] > rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status CompareGE::Execute(absl::Span<const T> lhs_buffer,
                          absl::Span<const T> rhs_buffer,
                          absl::Span<uint8_t> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] >= rhs_buffer[i];
  }
  return OkStatus();
}

namespace impl {
inline absl::InlinedVector<size_t, 6> ComputeCopyStrides(const Shape& shape,
                                                         size_t element_size) {
  absl::InlinedVector<size_t, 6> strides(shape.empty() ? 1 : shape.size());
  strides.back() = element_size;
  for (int i = shape.size() - 2; i >= 0; --i) {
    strides[i] = strides[i + 1] * shape[i + 1];
  }
  return strides;
}

inline void CopyRegion(absl::Span<const uint8_t> src_buffer,
                       absl::Span<const size_t> src_strides,
                       absl::Span<const int32_t> src_indices,
                       absl::Span<uint8_t> dst_buffer,
                       absl::Span<const size_t> dst_strides,
                       absl::Span<const int32_t> dst_indices,
                       absl::Span<const int32_t> lengths) {
  if (lengths.size() > 1) {
    for (int i = 0; i < lengths[0]; ++i) {
      size_t src_offset = src_strides[0] * (src_indices[0] + i);
      size_t dst_offset = dst_strides[0] * (dst_indices[0] + i);
      CopyRegion(src_buffer.subspan(src_offset), src_strides.subspan(1),
                 src_indices.subspan(1), dst_buffer.subspan(dst_offset),
                 dst_strides.subspan(1), dst_indices.subspan(1),
                 lengths.subspan(1));
    }
  } else {
    DCHECK_EQ(dst_strides.size(), 1);
    DCHECK_EQ(src_strides.size(), 1);
    DCHECK_EQ(src_indices.size(), 1);
    DCHECK_EQ(dst_indices.size(), 1);
    DCHECK_EQ(lengths.size(), 1);
    auto src_offset = src_indices[0] * src_strides[0];
    auto dst_offset = dst_indices[0] * dst_strides[0];
    auto length = dst_strides[0] * lengths[0];
    std::memcpy(dst_buffer.data() + dst_offset, src_buffer.data() + src_offset,
                length);
  }
}
}  // namespace impl

// TODO(benvanik): replace with a real implementation once copy is defined.
template <int element_size>
Status Copy::Execute(absl::Span<const uint8_t> src_buffer,
                     const Shape& src_shape,
                     absl::Span<const int32_t> src_indices,
                     absl::Span<uint8_t> dst_buffer, const Shape& dst_shape,
                     absl::Span<const int32_t> dst_indices,
                     absl::Span<const int32_t> lengths) {
  // TODO(gcmn) Maybe we can fast-path earlier if we detect contiguous memory
  // across multiple rows.
  auto src_strides = impl::ComputeCopyStrides(src_shape, element_size);
  auto dst_strides = impl::ComputeCopyStrides(dst_shape, element_size);
  impl::CopyRegion(src_buffer, src_strides, src_indices, dst_buffer,
                   dst_strides, dst_indices, lengths);
  return OkStatus();
}

template <typename T>
Status Select::Execute(absl::Span<const uint8_t> cond_buffer,
                       absl::Span<const T> lhs_buffer,
                       absl::Span<const T> rhs_buffer,
                       absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = cond_buffer[i] ? lhs_buffer[i] : rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status Transpose::Execute(absl::Span<const T> src_buffer,
                          absl::Span<T> dst_buffer, const Shape& src_shape,
                          absl::Span<const int32_t> perm) {
  // This implementation is .... not fast.
  int rank = src_shape.size();
  absl::InlinedVector<int, 8> src_strides(rank);
  absl::InlinedVector<int, 8> dst_strides(rank);
  size_t src_stride = 1;
  size_t dst_stride = 1;
  for (int dim_i = rank - 1; dim_i >= 0; --dim_i) {
    src_strides[dim_i] = src_stride;
    dst_strides[dim_i] = dst_stride;
    src_stride *= src_shape[dim_i];
    dst_stride *= src_shape[perm[dim_i]];
  }
  for (size_t dst_i = 0; dst_i < dst_buffer.size(); ++dst_i) {
    size_t src_i = 0;
    size_t t = dst_i;
    for (int dim_i = 0; dim_i < rank; ++dim_i) {
      size_t ratio = t / dst_strides[dim_i];
      t -= ratio * dst_strides[dim_i];
      src_i += ratio * src_strides[perm[dim_i]];
    }
    dst_buffer[dst_i] = src_buffer[src_i];
  }
  return OkStatus();
}

namespace impl {
inline void IncrementShapeIndex(absl::Span<int32_t> indices,
                                const Shape& shape) {
  for (int i = indices.size() - 1; i >= 0; --i) {
    if (++indices[i] < shape[i]) return;
    indices[i] = 0;
  }
}

inline bool IsPadding(absl::Span<const int32_t> indices, const Shape& shape,
                      absl::Span<const int32_t> edge_padding_low,
                      absl::Span<const int32_t> edge_padding_high,
                      absl::Span<const int32_t> interior_padding) {
  for (int i = 0; i < indices.size(); ++i) {
    auto index = indices[i];
    if (index < edge_padding_low[i] ||
        index >= shape[i] - edge_padding_high[i] ||
        (index - edge_padding_low[i]) % (interior_padding[i] + 1) != 0) {
      return true;
    }
  }

  return false;
}
}  // namespace impl

template <typename T>
Status Pad::Execute(absl::Span<const T> src_buffer,
                    absl::Span<const T> padding_value_buffer,
                    absl::Span<T> dst_buffer, const Shape& src_shape,
                    const Shape& dst_shape,
                    absl::Span<const int32_t> edge_padding_low,
                    absl::Span<const int32_t> edge_padding_high,
                    absl::Span<const int32_t> interior_padding) {
  // This implementation is not at all fast, as it iterates every index in the
  // destination buffer individually. Potential improvements:
  // 1. Fill the dst buffer with padded value initially. Only need to iterate
  //    through source buffer and can exit early.
  // 2. Use striding to advance through larger swaths of the buffer with a
  //    memcpy from src and filling (or skipping) padded incides. Especially
  //    useful when e.g. entire rows are padded.

  // TODO(b/140836672) support negative padding

  if (padding_value_buffer.size() != 1) {
    return InvalidArgumentErrorBuilder(IREE_LOC)
           << "Padding value buffer is larger than one element.";
  }
  auto padding_value = padding_value_buffer.front();

  absl::InlinedVector<int, 8> dst_indices(src_shape.size(), 0);

  const T* src_ptr = src_buffer.begin();
  T* dst_ptr = dst_buffer.begin();
  while (dst_ptr != dst_buffer.end()) {
    if (impl::IsPadding(dst_indices, dst_shape, edge_padding_low,
                        edge_padding_high, interior_padding)) {
      *dst_ptr++ = padding_value;
    } else {
      DCHECK(src_ptr != src_buffer.end());
      *dst_ptr++ = *src_ptr++;
    }
    impl::IncrementShapeIndex(absl::MakeSpan(dst_indices), dst_shape);
  }

  return OkStatus();
}

template <typename T>
Status Reverse::Execute(absl::Span<const T> src_buffer,
                        absl::Span<T> dst_buffer, const Shape& src_shape,
                        absl::Span<const int32_t> dimensions) {
  // This implementation is not fast either
  int rank = src_shape.size();
  absl::InlinedVector<int, 8> strides(rank);
  size_t stride = 1;
  for (int dim_i = rank - 1; dim_i >= 0; --dim_i) {
    strides[dim_i] = stride;
    stride *= src_shape[dim_i];
  }
  absl::flat_hash_set<int32_t> dims_set(dimensions.begin(), dimensions.end());
  for (size_t dst_i = 0; dst_i < dst_buffer.size(); ++dst_i) {
    size_t src_i = 0;
    size_t t = dst_i;
    for (int dim_i = 0; dim_i < rank; ++dim_i) {
      size_t ratio = t / strides[dim_i];
      t -= ratio * strides[dim_i];
      bool do_reverse = dims_set.contains(dim_i);
      src_i += (do_reverse ? (src_shape[dim_i] - 1 - ratio) : ratio) *
               strides[dim_i];
    }
    dst_buffer[dst_i] = src_buffer[src_i];
  }
  return OkStatus();
}

template <typename T>
Status Broadcast::Execute(absl::Span<const T> src_buffer,
                          absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = src_buffer[0];
  }
  return OkStatus();
}

template <typename T>
Status Tile::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer,
                     const Shape& src_shape, const Shape& dst_shape) {
  // This implementation is .... not fast.
  int rank = dst_shape.size();
  absl::InlinedVector<int, 8> src_strides(rank);
  absl::InlinedVector<int, 8> dst_strides(rank);
  size_t src_stride = 1;
  size_t dst_stride = 1;
  for (int dim_i = rank - 1; dim_i >= 0; --dim_i) {
    src_strides[dim_i] = src_stride;
    dst_strides[dim_i] = dst_stride;
    src_stride *= src_shape[dim_i];
    dst_stride *= dst_shape[dim_i];
  }
  for (size_t dst_i = 0; dst_i < dst_buffer.size(); ++dst_i) {
    size_t src_i = 0;
    size_t t = dst_i;
    for (int dim_i = 0; dim_i < rank; ++dim_i) {
      src_i += t / dst_strides[dim_i] % src_shape[dim_i] * src_strides[dim_i];
      t %= dst_strides[dim_i];
    }
    dst_buffer[dst_i] = src_buffer[src_i];
  }
  return OkStatus();
}

template <typename T>
Status Not::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = ~src_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status And::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] & rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status Or::Execute(absl::Span<const T> lhs_buffer,
                   absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] | rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status Xor::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] ^ rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status ShiftLeft::Execute(absl::Span<const T> lhs_buffer,
                          absl::Span<const T> rhs_buffer,
                          absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] << rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status ShiftRight::Execute(absl::Span<const T> lhs_buffer,
                           absl::Span<const T> rhs_buffer,
                           absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] >> rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status Add::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] + rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status Sub::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] - rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status Abs::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::abs(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Mul::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] * rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status Div::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = lhs_buffer[i] / rhs_buffer[i];
  }
  return OkStatus();
}

template <typename T>
Status MulAdd::Execute(absl::Span<const T> a_buffer,
                       absl::Span<const T> b_buffer,
                       absl::Span<const T> c_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = a_buffer[i] + (b_buffer[i] * c_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Exp::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::exp(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Rsqrt::Execute(absl::Span<const T> src_buffer,
                      absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = 1.0 / std::sqrt(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Log::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::log(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Cos::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::cos(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Sin::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::sin(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Tanh::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::tanh(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Atan2::Execute(absl::Span<const T> lhs_buffer,
                      absl::Span<const T> rhs_buffer,
                      absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::atan2(lhs_buffer[i], rhs_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Min::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::min(lhs_buffer[i], rhs_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Max::Execute(absl::Span<const T> lhs_buffer,
                    absl::Span<const T> rhs_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::max(lhs_buffer[i], rhs_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Clamp::Execute(absl::Span<const T> src_buffer,
                      absl::Span<const T> min_buffer,
                      absl::Span<const T> max_buffer,
                      absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    T src = src_buffer[i];
    T min = min_buffer[i];
    T max = max_buffer[i];
    dst_buffer[i] = src <= min ? min : src >= max ? max : src;
  }
  return OkStatus();
}

template <typename T>
Status Floor::Execute(absl::Span<const T> src_buffer,
                      absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::floor(src_buffer[i]);
  }
  return OkStatus();
}

template <typename T>
Status Ceil::Execute(absl::Span<const T> src_buffer, absl::Span<T> dst_buffer) {
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = std::ceil(src_buffer[i]);
  }
  return OkStatus();
}

template <typename SRC, typename DST>
Status Convert::Execute(absl::Span<const SRC> src_buffer,
                        absl::Span<DST> dst_buffer) {
  DCHECK_EQ(src_buffer.size(), dst_buffer.size());
  for (size_t i = 0; i < dst_buffer.size(); ++i) {
    dst_buffer[i] = static_cast<DST>(src_buffer[i]);
  }
  return OkStatus();
}

namespace impl {

struct SumKernel {
  template <typename T>
  inline void operator()(T* value0, const T value1) {
    *value0 += value1;
  }
};

struct MinKernel {
  template <typename T>
  inline void operator()(T* value0, const T value1) {
    *value0 = std::min(*value0, value1);
  }
};

struct MaxKernel {
  template <typename T>
  inline void operator()(T* value0, const T value1) {
    *value0 = std::max(*value0, value1);
  }
};

template <typename T, typename KernelImpl>
inline void ReduceDimension(absl::Span<const T> src_buffer,
                            absl::Span<T> dst_buffer, const Shape& src_shape,
                            absl::Span<const int32_t> reduce_dims,
                            absl::Span<const int> dst_strides, int dim,
                            absl::Span<int> src_indices, size_t flat_src_i,
                            size_t src_stride) {
  if (dim < 0) {
    // Base case of the recursion - figure out which elements should be acted
    // upon and apply the reduction kernel to them.

    // Derive destination indices from source indices.
    // For example,
    //     reduce_dims: [1, 2]
    //     src_indices: [2, 1, 3, 0]
    //                      ^  ^
    //                      |  |
    //                      |----- remove these dimensions
    //     dst_indices: [2, 0]
    //
    // TODO(scotttodd): Clean this up somehow, share across recursion levels?
    size_t dst_size = src_shape.size() - reduce_dims.size();
    absl::InlinedVector<int, 8> dst_indices;
    for (size_t i = 0; i < src_indices.size(); ++i) {
      if (std::find(std::begin(reduce_dims), std::end(reduce_dims), i) ==
          std::end(reduce_dims)) {
        dst_indices.push_back(src_indices[i]);
      }
    }
    // Compute the flattened index into dst_buffer at [dst_indices].
    size_t dst_i = 0;
    for (size_t i = 0; i < dst_indices.size(); ++i) {
      dst_i += dst_indices[i] * dst_strides[dst_size - 1 - i];
    }

    // Flattened src and dst indices have been computed, invoke the kernel.
    KernelImpl()(&dst_buffer[dst_i], src_buffer[flat_src_i]);
    return;
  }

  // Iterate through the current dimension in the source shape, recursing
  // down one dimension at a time.
  //
  // This touches each element in the source buffer once, tracking complete
  // dimensions within the shaped source buffer and using them to compute
  // the corresponding indices (shaped and flattened) within the destination
  // buffer. Each element in the destination buffer will be touched multiple
  // times.
  //
  // Note that cache coherency isn't considered here, and some computations
  // are redundant, so this could be optimized substantially.
  for (size_t dim_i = 0; dim_i < src_shape[dim]; ++dim_i) {
    src_indices[dim] = dim_i;

    // Recurse down to the next dimension (e.g. 2 -> 1 -> 0 -> base case)
    //   * Add the current stride to flat_src_i
    //   * Multiply src_stride by this dimension's shape
    ReduceDimension<T, KernelImpl>(src_buffer, dst_buffer, src_shape,
                                   reduce_dims, dst_strides, dim - 1,
                                   src_indices, flat_src_i + dim_i * src_stride,
                                   src_stride * src_shape[dim]);
  }
}

template <typename T, typename KernelImpl>
Status GenericReduce(absl::Span<const T> src_buffer,
                     absl::Span<const T> init_buffer, absl::Span<T> dst_buffer,
                     int32_t dimension, const Shape& src_shape,
                     const Shape& dst_shape) {
  // Initialize using init_buffer, which is expected to be a scalar.
  std::fill_n(dst_buffer.data(), dst_buffer.size(), init_buffer[0]);

  // Precompute destination strides.
  int dst_rank = dst_shape.size();
  absl::InlinedVector<int, 8> dst_strides;
  size_t dst_stride = 1;
  for (int dim_i = dst_rank - 1; dim_i >= 0; --dim_i) {
    dst_strides.push_back(dst_stride);
    dst_stride *= dst_shape[dim_i];
  }

  // Call the helper (recursive) function, starting with:
  //   * source index [0, 0, ..., 0]
  //   * the innermost dimension (last in the shape)
  //   * flat_src_i of 0 (corresponds to [0, 0, ..., 0] above)
  //   * source stride 1
  absl::InlinedVector<int, 8> src_indices(src_shape.size(), 0);
  ReduceDimension<T, KernelImpl>(src_buffer, dst_buffer, src_shape, {dimension},
                                 absl::MakeSpan(dst_strides),
                                 src_shape.size() - 1,
                                 absl::MakeSpan(src_indices), 0, 1);

  return OkStatus();
}

}  // namespace impl

template <typename T>
Status ReduceSum::Execute(absl::Span<const T> src_buffer,
                          absl::Span<const T> init_buffer,
                          absl::Span<T> dst_buffer, int32_t dimension,
                          const Shape& src_shape, const Shape& dst_shape) {
  return impl::GenericReduce<T, impl::SumKernel>(
      src_buffer, init_buffer, dst_buffer, dimension, src_shape, dst_shape);
}

template <typename T>
Status ReduceMin::Execute(absl::Span<const T> src_buffer,
                          absl::Span<const T> init_buffer,
                          absl::Span<T> dst_buffer, int32_t dimension,
                          const Shape& src_shape, const Shape& dst_shape) {
  return impl::GenericReduce<T, impl::MinKernel>(
      src_buffer, init_buffer, dst_buffer, dimension, src_shape, dst_shape);
}

template <typename T>
Status ReduceMax::Execute(absl::Span<const T> src_buffer,
                          absl::Span<const T> init_buffer,
                          absl::Span<T> dst_buffer, int32_t dimension,
                          const Shape& src_shape, const Shape& dst_shape) {
  return impl::GenericReduce<T, impl::MaxKernel>(
      src_buffer, init_buffer, dst_buffer, dimension, src_shape, dst_shape);
}

}  // namespace kernels
}  // namespace hal
}  // namespace iree

#endif  // IREE_HAL_INTERPRETER_BYTECODE_KERNELS_GENERIC_H_
