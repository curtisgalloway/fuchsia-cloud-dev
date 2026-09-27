// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_QEMU_EDU_H_
#define FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_QEMU_EDU_H_

// [START imports]
#include <fidl/examples.qemuedu/cpp/wire.h>
#include <lib/driver/component/cpp/driver_base.h>
#include <lib/driver/devfs/cpp/connector.h>
// [END imports]

// [START hw_imports]
#include "edu_device.h"
// [END hw_imports]

// [START namespace_start]
namespace qemu_edu {
// [END namespace_start]

// [START class_header]
class QemuEduDriver : public fdf::DriverBase2 {
  // [END class_header]
  // [START public_main]
 public:
  QemuEduDriver() : fdf::DriverBase2("qemu-edu") {}

  virtual ~QemuEduDriver() = default;

  // Start hook called by the driver factory.
  zx::result<> Start(fdf::DriverContext context) override;
  // [END public_main]

  // [START private_main]
 private:
  void Serve(fidl::ServerEnd<examples_qemuedu::Device> request);

  fidl::WireSyncClient<fuchsia_driver_framework::Node> node_;
  fidl::WireSyncClient<fuchsia_driver_framework::NodeController> controller_;
  // [END private_main]

  // [START fields_hw]
  std::shared_ptr<edu_device::QemuEduDevice> device_;
  // [END fields_hw]

  // [START class_footer]
};
// [END class_footer]

// [START namespace_end]
}  // namespace qemu_edu
// [END namespace_end]

#endif  // FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_QEMU_EDU_H_
