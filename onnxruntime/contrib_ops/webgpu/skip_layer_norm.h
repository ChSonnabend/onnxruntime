// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "core/providers/webgpu/program.h"
#include "core/providers/webgpu/webgpu_kernel.h"

namespace onnxruntime {
namespace contrib {
namespace webgpu {

using namespace onnxruntime::webgpu;
using onnxruntime::webgpu::ComputeContext;


class SkipLayerNormProgram final : public Program<SkipLayerNormProgram> {
 public:
  SkipLayerNormProgram(bool hasBeta, bool hasBias, float epsilon, int hiddenSize, bool isFP16, bool simplified) : Program{"SkipLayerNorm"} {
    epsilon_ = epsilon;
    hasBeta_ = hasBeta;
    hasBias_ = hasBias;
    epsilon_ = epsilon;
    hiddenSize_ = hiddenSize;
    simplified_ = simplified;
    isFP16_ = isFP16;
  }

  Status GenerateShaderCode(ShaderHelper& sh) const override;

  WEBGPU_PROGRAM_DEFINE_UNIFORM_VARIABLES(
    {"components", ProgramUniformVariableDataType::Uint32},
    {"hidden_size", ProgramUniformVariableDataType::Uint32},
    {"epsilon", ProgramUniformVariableDataType::Float32});

 private:
  bool hasBeta_;
  bool hasBias_;
  float epsilon_;
  int hiddenSize_;
  bool isFP16_;
  bool simplified_;
};

template <bool simplified>
class SkipLayerNorm final : public WebGpuKernel {
 public:
  SkipLayerNorm(const OpKernelInfo& info) : WebGpuKernel(info) {
    info.GetAttrOrDefault<float>("epsilon", &epsilon_, 1e-05);
  }

  Status ComputeInternal(ComputeContext& context) const override;

 protected:
  std::string cache_hint;

 private:
  float epsilon_;
};

}  // namespace webgpu
}  // namespace contrib
}  // namespace onnxruntime
