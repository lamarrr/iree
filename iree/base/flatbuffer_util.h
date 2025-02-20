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

#ifndef IREE_BASE_FLATBUFFER_UTIL_H_
#define IREE_BASE_FLATBUFFER_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "third_party/flatbuffers/include/flatbuffers/flatbuffers.h"
#include "iree/base/status.h"

namespace iree {

// Wraps a FlatBuffer String in an absl::string_view.
// Returns empty-string ("") for nullptr values.
inline absl::string_view WrapString(const ::flatbuffers::String* value) {
  return value ? absl::string_view{value->data(), value->size()} : "";
}

// Base type for FlatBufferFile<T>. See below.
class FlatBufferFileBase {
 public:

  using Identifier = absl::optional<const char*>;

  virtual ~FlatBufferFileBase();

 protected:
  template <typename T>
  friend class FlatBufferFile;

  using VerifierFn = bool (*)(const char* identifier,
                              ::flatbuffers::Verifier* verifier);

  FlatBufferFileBase() = default;

  const void* root_ptr() const { return root_ptr_; }

  // Redirections of template static methods on FlatBufferFile so we can put the
  // implementations in a shared compilation unit.
  // See FlatBufferFile<T> for doc comments.
  Status Create(const void* root_ptr, std::function<void()> deleter);
  Status CreateWithBackingBuffer(const void* root_ptr,
                                 ::flatbuffers::DetachedBuffer backing_buffer);
  Status Wrap(const void* root);
  Status FromBuffer(Identifier identifier,
                    absl::Span<const uint8_t> buffer_data,
                    std::function<void()> deleter, size_t root_type_size,
                    VerifierFn verifier_fn);
  Status WrapBuffer(Identifier identifier,
                    absl::Span<const uint8_t> buffer_data,
                    size_t root_type_size, VerifierFn verifier_fn);
  Status FromString(Identifier identifier, std::string buffer_data,
                    size_t root_type_size, VerifierFn verifier_fn);
  Status LoadFile(Identifier identifier, absl::string_view path,
                  size_t root_type_size, VerifierFn verifier_fn);

 private:
  const void* root_ptr_ = nullptr;
  std::function<void()> deleter_;
};

// Immutable root FlatBuffer type wrapper with support for loading and backing
// buffer management.
//
// Immutable and thread-safe.
template <typename T>
class FlatBufferFile final : public FlatBufferFileBase {
 public:
  // Creates a FlatBufferFile from an in-memory root pointer.
  // The provided |deleter| will be called when the FlatBufferFile is destructed
  // and can be used to deallocate/clean up resources.
  //
  // This assumes that the root pointer has already been verified as valid.
  // If verification is required instead use FromBuffer on the original buffer.
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> Create(
      const T* root, std::function<void()> deleter);

  // Creates a FlatBufferFile from an in-memory root pointer and the detached
  // backing buffer storing it.
  //
  // Example:
  //  FlatBufferBuilder fbb;
  //  MyTypeBuilder mtb(fbb);
  //  fbb.Finish(mtb.Finish());
  //  auto my_type = FlatBufferFile<MyType>::CreateWithBackingBuffer(
  //      fbb.Release());
  //  my_type->foo();
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> CreateWithBackingBuffer(
      ::flatbuffers::DetachedBuffer backing_buffer);

  // Wraps a caller-owned in-memory root pointer.
  // The provided |root| must remain valid for the lifetime of the returned
  // FlatBufferFile.
  //
  // This assumes that the root pointer has already been verified as valid.
  // If verification is required instead use FromBuffer on the original buffer.
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> Wrap(const T* root);

  // Creates a FlatBufferFile wrapping an external data buffer with a deleter
  // function that will be called when the FlatBufferFile is destructed.
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> FromBuffer(
      Identifier identifier, absl::Span<const uint8_t> buffer_data,
      std::function<void()> deleter);

  // Creates a FlatBufferFile from a serialized data buffer.
  // The FlatBufferFile takes ownership of the vector.
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> FromBuffer(
      Identifier identifier, std::vector<uint8_t> buffer_data);

  // Loads a FlatBufferFile from an external buffer owned by the caller.
  // The buffer must remain valid until the Pipeline is destroyed.
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> WrapBuffer(
      Identifier identifier, absl::Span<const uint8_t> buffer_data);

  // Loads a FlatBufferFile from a serialized string.
  // The FlatBufferFile takes ownership of the string.
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> FromString(
      Identifier identifier, std::string buffer_data);

  // Loads a FlatBufferFile from a serialized file on the file system.
  // This will attempt to mmap the file and is the preferred way of loading as
  // only those pages that contain requested tables will be read.
  static StatusOr<std::unique_ptr<FlatBufferFile<T>>> LoadFile(
      Identifier identifier, absl::string_view path);

  // Returns a vector of file references that share the same underlying data
  // buffer. The buffer will be kept alive until the last file is released.
  static StatusOr<std::vector<std::unique_ptr<FlatBufferFile<T>>>>
  CreateShareGroup(std::unique_ptr<FlatBufferFile<T>> file, int count);

  ~FlatBufferFile() override = default;

  // Typed root pointer of the file.
  const T* root() const { return reinterpret_cast<const T*>(root_ptr()); }

 private:
  FlatBufferFile() = default;

  // Conforms to VerifierFn.
  static bool VerifierFnT(const char* identifier,
                          ::flatbuffers::Verifier* verifier) {
    return verifier->VerifyBuffer<T>(identifier);
  }
};

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>> FlatBufferFile<T>::Create(
    const T* root, std::function<void()> deleter) {
  std::unique_ptr<FlatBufferFile<T>> flat_buffer_file{new FlatBufferFile<T>};
  auto* base_file = static_cast<FlatBufferFileBase*>(flat_buffer_file.get());
  RETURN_IF_ERROR(base_file->Create(root, std::move(deleter)));
  return std::move(flat_buffer_file);
}

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>>
FlatBufferFile<T>::CreateWithBackingBuffer(
    ::flatbuffers::DetachedBuffer backing_buffer) {
  std::unique_ptr<FlatBufferFile<T>> flat_buffer_file{new FlatBufferFile<T>};
  auto* base_file = static_cast<FlatBufferFileBase*>(flat_buffer_file.get());
  auto* root_ptr = ::flatbuffers::GetRoot<T>(backing_buffer.data());
  RETURN_IF_ERROR(
      base_file->CreateWithBackingBuffer(root_ptr, std::move(backing_buffer)));
  return std::move(flat_buffer_file);
}

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>> FlatBufferFile<T>::Wrap(
    const T* root) {
  std::unique_ptr<FlatBufferFile<T>> flat_buffer_file{new FlatBufferFile<T>};
  auto* base_file = static_cast<FlatBufferFileBase*>(flat_buffer_file.get());
  RETURN_IF_ERROR(base_file->Wrap(root));
  return std::move(flat_buffer_file);
}

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>> FlatBufferFile<T>::FromBuffer(
    Identifier identifier, absl::Span<const uint8_t> buffer_data,
    std::function<void()> deleter) {
  std::unique_ptr<FlatBufferFile<T>> flat_buffer_file{new FlatBufferFile<T>};
  auto* base_file = static_cast<FlatBufferFileBase*>(flat_buffer_file.get());
  RETURN_IF_ERROR(base_file->FromBuffer(
      identifier, buffer_data, std::move(deleter), sizeof(T), VerifierFnT));
  return std::move(flat_buffer_file);
}

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>> FlatBufferFile<T>::FromBuffer(
    Identifier identifier, std::vector<uint8_t> buffer_data) {
  auto* buffer_data_ptr = new decltype(buffer_data);
  (*buffer_data_ptr) = std::move(buffer_data);
  return FromBuffer(identifier, absl::MakeConstSpan(*buffer_data_ptr),
                    [buffer_data_ptr]() { delete buffer_data_ptr; });
}

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>> FlatBufferFile<T>::WrapBuffer(
    Identifier identifier, absl::Span<const uint8_t> buffer_data) {
  std::unique_ptr<FlatBufferFile<T>> flat_buffer_file{new FlatBufferFile<T>};
  auto* base_file = static_cast<FlatBufferFileBase*>(flat_buffer_file.get());
  RETURN_IF_ERROR(
      base_file->WrapBuffer(identifier, buffer_data, sizeof(T), VerifierFnT));
  return std::move(flat_buffer_file);
}

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>> FlatBufferFile<T>::FromString(
    Identifier identifier, std::string buffer_data) {
  std::unique_ptr<FlatBufferFile<T>> flat_buffer_file{new FlatBufferFile<T>};
  auto* base_file = static_cast<FlatBufferFileBase*>(flat_buffer_file.get());
  RETURN_IF_ERROR(base_file->FromString(identifier, std::move(buffer_data),
                                        sizeof(T), VerifierFnT));
  return std::move(flat_buffer_file);
}

// static
template <typename T>
StatusOr<std::unique_ptr<FlatBufferFile<T>>> FlatBufferFile<T>::LoadFile(
    Identifier identifier, absl::string_view path) {
  std::unique_ptr<FlatBufferFile<T>> flat_buffer_file{new FlatBufferFile<T>};
  auto* base_file = static_cast<FlatBufferFileBase*>(flat_buffer_file.get());
  RETURN_IF_ERROR(
      base_file->LoadFile(identifier, path, sizeof(T), VerifierFnT));
  return std::move(flat_buffer_file);
}

// static
template <typename T>
StatusOr<std::vector<std::unique_ptr<FlatBufferFile<T>>>>
FlatBufferFile<T>::CreateShareGroup(std::unique_ptr<FlatBufferFile<T>> file,
                                    int count) {
  // Create a shared_ptr wrapper for the base file that will be.
  std::shared_ptr<FlatBufferFile<T>> shared_file{file.release()};

  // Create N files. We wrap and keep the shared_ptr alive in the deleter
  // capture. By wrapping we avoid reverifying the entire buffer.
  std::vector<std::unique_ptr<FlatBufferFile<T>>> list;
  for (int i = 0; i < count; ++i) {
    ASSIGN_OR_RETURN(auto new_file, FlatBufferFile<T>::Create(
                                        shared_file->root(), [shared_file]() {
                                          // Each new file keeps a reference to
                                          // the shared file to keep it alive.
                                          (void)shared_file;
                                        }));
    list.push_back(std::move(new_file));
  }
  return std::move(list);
}

}  // namespace iree

#endif  // IREE_BASE_FLATBUFFER_UTIL_H_
