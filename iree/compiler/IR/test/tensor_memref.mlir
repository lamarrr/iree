// RUN: iree-opt %s -canonicalize | FileCheck %s --dump-input=fail

// CHECK-LABEL: @memref_to_memref
// CHECK-SAME: [[ARG:%[a-zA-Z0-9]+]]
func @memref_to_memref(%arg0 : memref<i32>) -> memref<i32> {
  // CHECK-NEXT: return [[ARG]]
  %0 = iree.memref_to_tensor(%arg0 : memref<i32>) : tensor<i32>
  %1 = iree.tensor_to_memref(%0 : tensor<i32>) : memref<i32>
  return %1 : memref<i32>
}

// CHECK-LABEL: @tensor_to_tensor
// CHECK-SAME: [[ARG:%[a-zA-Z0-9]+]]
func @tensor_to_tensor(%arg0 : tensor<i32>) -> tensor<i32> {
  // CHECK-NEXT: return [[ARG]]
  %0 = iree.tensor_to_memref(%arg0 : tensor<i32>) : memref<i32>
  %1 = iree.memref_to_tensor(%0 : memref<i32>) : tensor<i32>
  return %1 : tensor<i32>
}
