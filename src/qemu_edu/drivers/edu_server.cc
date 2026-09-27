// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// [START imports]
#include "edu_server.h"
// [END imports]

// [START namespace_start]
namespace qemu_edu {
// [END namespace_start]

// [START compute_factorial]
// Driver Service: Compute factorial on the edu device
void QemuEduServer::ComputeFactorial(ComputeFactorialRequestView request,
                                     ComputeFactorialCompleter::Sync& completer) {
  auto edu_device = device_.lock();
  if (!edu_device) {
    FDF_LOG(ERROR, "Unable to access device resources.");
    completer.ReplyError(ZX_ERR_BAD_STATE);
    return;
  }

  uint32_t input = request->input;

  edu_device->ComputeFactorial(
      input, [completer = completer.ToAsync()](zx::result<uint32_t> result_status) mutable {
        if (result_status.is_error()) {
          completer.ReplyError(result_status.error_value());
          return;
        }
        uint32_t factorial = result_status.value();
        completer.ReplySuccess(factorial);
      });
}
// [END compute_factorial]

// [START liveness_check]
// Driver Service: Complete a liveness check on the edu device
void QemuEduServer::LivenessCheck(LivenessCheckCompleter::Sync& completer) {
  auto edu_device = device_.lock();
  if (!edu_device) {
    FDF_LOG(ERROR, "Unable to access device resources.");
    completer.ReplyError(ZX_ERR_BAD_STATE);
    return;
  }

  constexpr uint32_t kChallenge = 0xdeadbeef;
  constexpr uint32_t kExpectedResponse = ~(kChallenge);

  auto status = edu_device->LivenessCheck(kChallenge);
  if (status.is_error()) {
    FDF_LOG(ERROR, "Unable to send liveness check request.");
    completer.ReplyError(status.error_value());
    return;
  }
  const bool alive = (status.value() == kExpectedResponse);

  FDF_SLOG(INFO, "Replying with", KV("result", alive));
  completer.ReplySuccess(alive);
}
// [END liveness_check]

void QemuEduServer::handle_unknown_method(
    fidl::UnknownMethodMetadata<examples_qemuedu::Device> metadata,
    fidl::UnknownMethodCompleter::Sync& completer) {}

// [START namespace_end]
}  // namespace qemu_edu
// [END namespace_end]
