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

// Defines the ExecutableFormat 4cc type and a few well-known formats.
// Not all formats need to be defined here, however any format expected to be
// supported by debuggers/tooling will likely want to be here to ensure easier
// referencing.

#ifndef IREE_HAL_EXECUTABLE_FORMAT_H_
#define IREE_HAL_EXECUTABLE_FORMAT_H_

#include <cstdint>

namespace iree {
namespace hal {

// Executable format 4cc identifier.
using ExecutableFormat = uint32_t;

// Constructs an ExecutableFormat 4cc at compile-time.
constexpr ExecutableFormat MakeExecutableFormatID(char const four_cc[5]) {
  return (four_cc[0] << 24) | (four_cc[1] << 16) | (four_cc[2] << 8) |
         four_cc[3];
}


// Undefined (or unknown). The format may be derived from the executable
// contents (such as file magic bytes).
constexpr ExecutableFormat kExecutableFormatUnspecified =
    MakeExecutableFormatID("    ");

// MLIR text form.
constexpr ExecutableFormat kExecutableFormatMlir =
    MakeExecutableFormatID("MLIR");

// IREE v0 bytecode.
constexpr ExecutableFormat kExecutableFormatIreeBytecode =
    MakeExecutableFormatID("IREE");

// SPIR-V executable in FlatBuffer format using the
// iree/schemas/spirv_executable_def.fbs schema.
constexpr ExecutableFormat kExecutableFormatSpirV =
    MakeExecutableFormatID("SPVE");


}  // namespace hal
}  // namespace iree

#endif  // IREE_HAL_EXECUTABLE_FORMAT_H_
