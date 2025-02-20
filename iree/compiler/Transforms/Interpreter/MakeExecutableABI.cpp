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

#include "iree/compiler/IR/Interpreter/HLOps.h"
#include "iree/compiler/IR/Ops.h"
#include "iree/compiler/Utils/OpUtils.h"
#include "mlir/Dialect/StandardOps/Ops.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BlockAndValueMapping.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/StandardTypes.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Support/LogicalResult.h"

namespace mlir {
namespace iree_compiler {

namespace {

// Replaces a load_input op with valid IR that loads the input value.
LogicalResult replaceLoadInputOp(IREE::LoadInputOp bindOp) {
  OpBuilder builder(bindOp);

  Value *newValue = nullptr;
  auto dstType = bindOp.getResult()->getType();
  if (dstType.isa<TensorType>()) {
    auto castOp = builder.create<IREE::MemRefToTensorOp>(bindOp.getLoc(),
                                                         dstType, bindOp.src());
    newValue = castOp.getResult();
  } else if (dstType.isIntOrIndexOrFloat()) {
    auto loadOp = builder.create<LoadOp>(bindOp.getLoc(), dstType, bindOp.src(),
                                         ArrayRef<Value *>{});
    newValue = loadOp.getResult();
  } else {
    return bindOp.emitError()
           << "Unsupported input destination type " << dstType;
  }

  bindOp.replaceAllUsesWith(newValue);
  bindOp.erase();

  return success();
}

// Replaces a store_output op with valid IR that stores the output value.
LogicalResult replaceStoreOutputOp(IREE::StoreOutputOp bindOp) {
  OpBuilder builder(bindOp);

  auto srcType = bindOp.src()->getType();
  if (srcType.isa<MemRefType>()) {
    // Already stored into the output.
  } else if (srcType.isa<TensorType>()) {
    auto castOp = builder.create<IREE::TensorToMemRefOp>(
        bindOp.getLoc(), bindOp.dst()->getType(), bindOp.src());

    // Insert a copy to our output parameter.
    auto dst = bindOp.dst()->getType().cast<ShapedType>();
    if (!dst.hasStaticShape()) {
      return bindOp.emitError()
             << "Dynamic output args are not yet implemented";
    }

    // TODO(b/134586626): decide if we want copy indices or byte offsets and
    // support 0-rank natively.
    int rank = dst.getRank() ? dst.getRank() : 1;
    auto zeroValues = std::vector<int32_t>(rank);
    auto shapeValues = std::vector<int32_t>(rank);
    if (dst.getRank() > 0) {
      for (int i = 0; i < dst.getRank(); ++i) {
        shapeValues[i] = static_cast<int32_t>(dst.getDimSize(i));
      }
    } else {
      shapeValues[0] = 1;
    }
    auto zeros = builder.create<IREE::ConstantOp>(
        bindOp.getLoc(),
        DenseIntElementsAttr::get<int32_t>(
            builder.getTensorType({rank}, builder.getIntegerType(32)),
            zeroValues));
    auto lengths = builder.create<IREE::ConstantOp>(
        bindOp.getLoc(),
        DenseIntElementsAttr::get<int32_t>(
            builder.getTensorType({rank}, builder.getIntegerType(32)),
            shapeValues));

    builder.create<IREEInterp::HL::CopyOp>(bindOp.getLoc(), castOp.getResult(),
                                           zeros, bindOp.dst(), zeros, lengths);
  } else if (srcType.isIntOrIndexOrFloat()) {
    builder.create<StoreOp>(bindOp.getLoc(), bindOp.src(), bindOp.dst(),
                            ArrayRef<Value *>{});
  } else {
    return bindOp.emitError() << "Unsupported output src type " << srcType;
  }

  bindOp.erase();

  return success();
}

// Strips iree.bind_* ops from |func|.
LogicalResult stripBindingOps(FuncOp func) {
  // Find iree.load_input ops to replace with memref_to_tensor if needed.
  SmallVector<IREE::LoadInputOp, 8> bindInputOps;
  func.walk([&](IREE::LoadInputOp bindOp) { bindInputOps.push_back(bindOp); });
  for (auto &bindOp : bindInputOps) {
    if (failed(replaceLoadInputOp(bindOp))) {
      return failure();
    }
  }

  // Find iree.store_output ops and replace with tensor_to_memref if needed.
  SmallVector<IREE::StoreOutputOp, 8> bindOutputOps;
  func.walk(
      [&](IREE::StoreOutputOp bindOp) { bindOutputOps.push_back(bindOp); });
  for (auto &bindOp : bindOutputOps) {
    if (failed(replaceStoreOutputOp(bindOp))) {
      return failure();
    }
  }

  return success();
}

}  // namespace

// Finds iree.executable.export functions and fixes up bindings.
// For the interpreter this really just means stripping the bind ops entirely.
class MakeExecutableABIPass : public ModulePass<MakeExecutableABIPass> {
 public:
  void runOnModule() override {
    auto module = getModule();
    for (auto func : module.getOps<FuncOp>()) {
      if (func.getAttr("iree.executable.export")) {
        if (failed(stripBindingOps(func))) {
          return signalPassFailure();
        }
      }
    }
  }
};

std::unique_ptr<OpPassBase<ModuleOp>> createMakeExecutableABIPass() {
  return std::make_unique<MakeExecutableABIPass>();
}

static PassRegistration<MakeExecutableABIPass> pass(
    "iree-make-executable-abi",
    "Makes functions match the IREE dispatch executable ABI.");

}  // namespace iree_compiler
}  // namespace mlir
