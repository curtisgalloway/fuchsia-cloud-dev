// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// [START imports]
#include "qemu_edu.h"

#include <lib/driver/component/cpp/driver_export2.h>
// [END imports]

// [START fidl_imports]
#include "edu_server.h"
// [END fidl_imports]

// [START namespace_start]
namespace qemu_edu {
// [END namespace_start]

// [START start_method_start]
// Initialize this driver instance
zx::result<> QemuEduDriver::Start(fdf::DriverContext context) {
  // [END start_method_start]
  // [START connect_device]
  // Connect to the parent device node.
  zx::result connect_result = context.incoming().Connect<fuchsia_hardware_pci::Service::Device>("pci");
  if (connect_result.is_error()) {
    FDF_SLOG(ERROR, "Failed to open pci service.", KV("status", connect_result.status_string()));
    return connect_result.take_error();
  }
  // [END connect_device]

  // [START hw_resources]
  // Map hardware resources from the PCI device
  device_ =
      std::make_shared<edu_device::QemuEduDevice>(dispatcher(), std::move(connect_result.value()));
  auto pci_status = device_->MapInterruptAndMmio();
  if (pci_status.is_error()) {
    return pci_status.take_error();
  }
  // [END hw_resources]

  // [START device_registers]
  // Report the version information from the edu device.
  auto version_reg = device_->IdentificationRegister();
  FDF_SLOG(INFO, "edu device version", KV("major", version_reg.major_version()),
           KV("minor", version_reg.minor_version()));
  // [END device_registers]

  // [START serve_outgoing]
  // Serve the examples.qemuedu/Service capability.
  examples_qemuedu::Service::InstanceHandler handler({
      .device = fit::bind_member<&QemuEduDriver::Serve>(this),
  });

  auto add_result = outgoing()->AddService<examples_qemuedu::Service>(std::move(handler));
  if (add_result.is_error()) {
    FDF_SLOG(ERROR, "Failed to add Device service", KV("status", add_result.status_string()));
    return add_result.take_error();
  }
  // [END serve_outgoing]

  // [START start_method_end]
  return zx::ok();
}
// [END start_method_end]

void QemuEduDriver::Serve(fidl::ServerEnd<examples_qemuedu::Device> request) {
  QemuEduServer::BindDeviceClient(dispatcher(), device_, std::move(request));
}

// [START namespace_end]
}  // namespace qemu_edu
// [END namespace_end]

// [START driver_hook]
// Register driver hooks with the framework
FUCHSIA_DRIVER_EXPORT2(qemu_edu::QemuEduDriver);
// [END driver_hook]
