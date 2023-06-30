#include "custom_op_library.h"

#define ORT_API_MANUAL_INIT
#include "onnxruntime_cxx_api.h"
#undef ORT_API_MANUAL_INIT

#include <vector>
#include <cmath>
#include <mutex>

#include "core/common/common.h"

#ifdef USE_CUDA
#include <cuda.h>
#include <cuda_runtime.h>
template <typename T1, typename T2, typename T3>
void cuda_add(int64_t, T3*, const T1*, const T2*, cudaStream_t compute_stream);
#endif

#ifdef USE_CUDA
#include "onnxruntime_cuda_context.h"
#endif
#include "onnxruntime_lite_custom_op.h"

static const char* c_OpDomain = "test.customop";

#ifdef USE_DML
void IdentityDML(const Ort::Custom::Tensor<float>&,
                 Ort::Custom::Tensor<float>&) {
}
#endif

#include <iostream>
#ifdef USE_CUDA
void KernelOne(Ort::Custom::OrtCudaContext* cuda_ctx,
               const Ort::Custom::Tensor<float>& X,
               const Ort::Custom::Tensor<float>& Y,
               Ort::Custom::Tensor<float>& Z) {
  auto input_shape = X.Shape();
  std::cout << "Fetch cuda stream from context" << std::endl;
  cudaStream_t cuda_stream = cuda_ctx->cuda_stream;
  auto z_raw = Z.Allocate(input_shape);
  cuda_add(Z.NumberOfElement(), z_raw, X.Data(), Y.Data(), cuda_stream);
}
#else
struct KernelOne {
  void Compute(OrtKernelContext* context) {
    // Setup inputs
    Ort::KernelContext ctx(context);
    auto input_X = ctx.GetInput(0);
    auto input_Y = ctx.GetInput(1);
    const float* X = input_X.GetTensorData<float>();
    const float* Y = input_Y.GetTensorData<float>();

    // Setup output
    auto dimensions = input_X.GetTensorTypeAndShapeInfo().GetShape();

    auto output = ctx.GetOutput(0, dimensions);
    float* out = output.GetTensorMutableData<float>();

    const size_t size = output.GetTensorTypeAndShapeInfo().GetElementCount();

    // Do computation
    for (size_t i = 0; i < size; i++) {
      out[i] = X[i] + Y[i];
    }
  }
};

// legacy custom op registration
struct CustomOpOne : Ort::CustomOpBase<CustomOpOne, KernelOne> {
  void* CreateKernel(const OrtApi& /* api */, const OrtKernelInfo* /* info */) const {
    return std::make_unique<KernelOne>().release();
  };

  const char* GetName() const { return "CustomOpOne"; };

#ifdef USE_CUDA
  const char* GetExecutionProviderType() const { return "CUDAExecutionProvider"; };
#endif

  size_t GetInputTypeCount() const { return 2; };
  ONNXTensorElementDataType GetInputType(size_t /*index*/) const { return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT; };

  size_t GetOutputTypeCount() const { return 1; };
  ONNXTensorElementDataType GetOutputType(size_t /*index*/) const { return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT; };
};
#endif

// lite custom op as a function
void KernelTwo(const Ort::Custom::Tensor<float>& X,
               Ort::Custom::Tensor<int32_t>& Y) {
  const auto& shape = X.Shape();
  auto X_raw = X.Data();
  auto Y_raw = Y.Allocate(shape);
  auto total = std::accumulate(shape.begin(), shape.end(), 1LL, std::multiplies<int64_t>());
  for (int64_t i = 0; i < total; i++) {
    Y_raw[i] = static_cast<int32_t>(round(X_raw[i]));
  }
}

template <typename T>
void MulTop(const Ort::Custom::Span<T>& in, Ort::Custom::Tensor<T>& out) {
  out.Allocate({1})[0] = in[0] * in[1];
}

void Fuse(
    OrtKernelContext*,
    const Ort::Custom::Span<float>& vector_1,
    const Ort::Custom::Span<float>& vector_2,
    int32_t alpha,
    Ort::Custom::Tensor<float>& vector_output) {
  auto len_output = std::min(vector_1.size(), vector_2.size());
  float* floats_out = static_cast<float*>(vector_output.Allocate({(int64_t)len_output}));
  for (size_t i = 0; i < len_output; ++i) {
    floats_out[i] = (vector_1[i] + vector_2[i]) * alpha;
  }
}

void Select(const Ort::Custom::Span<int32_t>& indices_in,
            Ort::Custom::Tensor<int32_t>& indices_out) {
  std::vector<int32_t> selected_indices;
  for (size_t i = 0; i < indices_in.size(); ++i) {
    if (indices_in[i] % 2 == 0) {
      selected_indices.push_back(indices_in[i]);
    }
  }

  int32_t* int_out = static_cast<int32_t*>(indices_out.Allocate({static_cast<int64_t>(selected_indices.size())}));
  for (size_t j = 0; j < selected_indices.size(); ++j) {
    int_out[j] = selected_indices[j];
  }
}

void Filter(const Ort::Custom::Tensor<float>& floats_in,
            Ort::Custom::Tensor<float>& floats_out) {
  const float* in = floats_in.Data();
  auto in_len = floats_in.NumberOfElement();

  std::vector<float> filter_floats;
  for (int64_t i = 0; i < in_len; ++i) {
    if (in[i] > 1.f) {
      filter_floats.push_back(in[i]);
    }
  }

  float* out = static_cast<float*>(floats_out.Allocate({static_cast<int64_t>(filter_floats.size())}));
  for (size_t j = 0; j < filter_floats.size(); ++j) {
    out[j] = filter_floats[j];
  }
}

void Box(const Ort::Custom::Tensor<float>* float_in_1,
         const Ort::Custom::Tensor<float>* float_in_2,
         std::optional<const Ort::Custom::Tensor<float>*> float_in_3,
         Ort::Custom::Tensor<float>* float_out_1,
         std::optional<Ort::Custom::Tensor<float>*> float_out_2) {
  auto raw_in_1 = float_in_1->Data();
  auto raw_in_2 = float_in_2->Data();

  auto l_in_1 = float_in_1->Shape()[0];
  auto l_in_2 = float_in_2->Shape()[0];
  auto l_out_1 = l_in_1 + l_in_2;

  auto raw_out_1 = float_out_1->Allocate({l_out_1});

  for (int64_t i = 0; i < l_out_1; ++i) {
    raw_out_1[i] = i < l_in_1 ? raw_in_1[i] : raw_in_2[i - l_in_1];
  }

  if (float_in_3.has_value() && float_out_2.has_value()) {
    auto raw_in_3 = float_in_3.value()->Data();
    auto l_in_3 = float_in_3.value()->Shape()[0];
    auto l_out_2 = l_in_2 + l_in_3;
    auto raw_out_2 = float_out_2.value()->Allocate({l_out_2});
    for (int64_t i = 0; i < l_out_2; ++i) {
      raw_out_2[i] = i < l_in_2 ? raw_in_2[i] : raw_in_3[i - l_in_2];
    }
  }
}

static void AddOrtCustomOpDomainToContainer(Ort::CustomOpDomain&& domain) {
  static std::vector<Ort::CustomOpDomain> ort_custom_op_domain_container;
  static std::mutex ort_custom_op_domain_mutex;
  std::lock_guard<std::mutex> lock(ort_custom_op_domain_mutex);
  ort_custom_op_domain_container.push_back(std::move(domain));
}

OrtStatus* ORT_API_CALL RegisterCustomOps(OrtSessionOptions* options, const OrtApiBase* api) {
  Ort::Global<void>::api_ = api->GetApi(ORT_API_VERSION);

  using LiteOp = Ort::Custom::OrtLiteCustomOp;

#ifdef USE_CUDA
  static const std::unique_ptr<LiteOp> c_CustomOpOne{Ort::Custom::CreateLiteCustomOp("CustomOpOne", "CUDAExecutionProvider", KernelOne)};
#else
  static const CustomOpOne c_CustomOpOne;
#endif  // !1

  static const std::unique_ptr<LiteOp> c_CustomOpTwo{Ort::Custom::CreateLiteCustomOp("CustomOpTwo", "CPUExecutionProvider", KernelTwo)};
  static const std::unique_ptr<LiteOp> c_MulTopOpFloat{Ort::Custom::CreateLiteCustomOp("MulTop", "CPUExecutionProvider", MulTop<float>)};
  static const std::unique_ptr<LiteOp> c_MulTopOpInt32{Ort::Custom::CreateLiteCustomOp("MulTop", "CPUExecutionProvider", MulTop<int32_t>)};
  static const std::unique_ptr<LiteOp> fus_op_ptr{Ort::Custom::CreateLiteCustomOp("Fuse", "CPUExecutionProvider", Fuse)};
  static const std::unique_ptr<LiteOp> sel_op_ptr{Ort::Custom::CreateLiteCustomOp("Select", "CPUExecutionProvider", Select)};
  static const std::unique_ptr<LiteOp> fil_op_ptr{Ort::Custom::CreateLiteCustomOp("Filter", "CPUExecutionProvider", Filter)};
  static const std::unique_ptr<LiteOp> box_op_ptr{Ort::Custom::CreateLiteCustomOp("Box", "CPUExecutionProvider", Box)};

#ifdef USE_DML
  static const std::unique_ptr<LiteOp> identity_dml_op_ptr{Ort::Custom::CreateLiteCustomOp("IdentityDML", "DmlExecutionProvider", IdentityDML)};
#endif

  OrtStatus* result = nullptr;

  ORT_TRY {
    Ort::CustomOpDomain domain{c_OpDomain};
#ifdef USE_CUDA
    domain.Add(c_CustomOpOne.get());
#else
    domain.Add(&c_CustomOpOne);
#endif
    domain.Add(c_CustomOpTwo.get());
#ifdef USE_DML
    domain.Add(identity_dml_op_ptr.get());
#endif

    Ort::CustomOpDomain domain_v2{"v2"};
    domain_v2.Add(c_MulTopOpFloat.get());
    domain_v2.Add(c_MulTopOpInt32.get());
    domain_v2.Add(fus_op_ptr.get());
    domain_v2.Add(sel_op_ptr.get());
    domain_v2.Add(fil_op_ptr.get());
    domain_v2.Add(box_op_ptr.get());

    Ort::UnownedSessionOptions session_options(options);
    session_options.Add(domain);
    session_options.Add(domain_v2);
    AddOrtCustomOpDomainToContainer(std::move(domain));
    AddOrtCustomOpDomainToContainer(std::move(domain_v2));
  }
  ORT_CATCH(const std::exception& e) {
    ORT_HANDLE_EXCEPTION([&]() {
      Ort::Status status{e};
      result = status.release();
    });
  }
  return result;
}

OrtStatus* ORT_API_CALL RegisterCustomOpsAltName(OrtSessionOptions* options, const OrtApiBase* api) {
  return RegisterCustomOps(options, api);
}
