// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_EDU_SERVER_H_
#define FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_EDU_SERVER_H_

// [START imports]
#include <fidl/examples.qemuedu/cpp/wire.h>
#include <lib/driver/logging/cpp/structured_logger.h>

#include "edu_device.h"
// [END imports]

// [START namespace_start]
namespace qemu_edu {
// [END namespace_start]

// [START fidl_server]
// FIDL server implementation for the `examples.qemuedu/Device` protocol.
class QemuEduServer : public fidl::WireServer<examples_qemuedu::Device> {
 public:
  explicit QemuEduServer(std::weak_ptr<edu_device::QemuEduDevice> device)
      : device_(std::move(device)) {}

  static fidl::ServerBindingRef<examples_qemuedu::Device> BindDeviceClient(
      async_dispatcher_t* dispatcher, std::weak_ptr<edu_device::QemuEduDevice> device,
      fidl::ServerEnd<examples_qemuedu::Device> request) {
    // Bind each connection request to a unique FIDL server instance
    auto server_impl = std::make_unique<QemuEduServer>(device);
    return fidl::BindServer(dispatcher, std::move(request), std::move(server_impl),
                            std::mem_fn(&QemuEduServer::OnUnbound));
  }

  // This method is called when a server connection is torn down.
  void OnUnbound(fidl::UnbindInfo info, fidl::ServerEnd<examples_qemuedu::Device> server_end) {
    if (info.is_peer_closed()) {
      FDF_LOG(DEBUG, "Client disconnected");
    } else if (!info.is_user_initiated()) {
      FDF_LOG(ERROR, "Client connection unbound: %s", info.status_string());
    }
  }

  // fidl::WireServer<examples_qemuedu::Device>

  void ComputeFactorial(ComputeFactorialRequestView request,
                        ComputeFactorialCompleter::Sync& completer) override;
  void LivenessCheck(LivenessCheckCompleter::Sync& completer) override;
  void handle_unknown_method(fidl::UnknownMethodMetadata<examples_qemuedu::Device> metadata,
                             fidl::UnknownMethodCompleter::Sync& completer) override;

 private:
  std::weak_ptr<edu_device::QemuEduDevice> device_;
};
// [END fidl_server]

// [START namespace_end]
}  // namespace qemu_edu
// [END namespace_end]

#endif  // FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_EDU_SERVER_H_
