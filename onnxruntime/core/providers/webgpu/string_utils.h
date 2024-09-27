// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <absl/strings/internal/ostringstream.h>

namespace onnxruntime {
namespace webgpu {

constexpr const size_t kStringInitialSizeSetByOffsetImpl = 128;
constexpr const size_t kStringInitialSizeGetByOffsetImpl = 128;
constexpr const size_t kStringInitialSizeShaderSourceCode = 2048;
#ifndef NDEBUG
constexpr const size_t kStringInitialSizeCacheKey = 512;
#else
constexpr const size_t kStringInitialSizeCacheKey = 256;
#endif

using OStringStream = absl::strings_internal::OStringStream;

namespace detail {
inline void OStringStreamAppendImpl(std::ostream& /*ss*/) noexcept {
}

template <typename T>
inline void OStringStreamAppendImpl(std::ostream& ss, const T& t) noexcept {
  ss << t;
}

template <typename T, typename... Args>
inline void OStringStreamAppendImpl(std::ostream& ss, const T& t, const Args&... args) noexcept {
  OStringStreamAppendImpl(ss, t);
  OStringStreamAppendImpl(ss, args...);
}
}  // namespace detail

}  // namespace webgpu
}  // namespace onnxruntime
