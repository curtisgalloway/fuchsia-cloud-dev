// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_EDU_DEVICE_H_
#define FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_EDU_DEVICE_H_

// [START imports]
#include <hwreg/bitfields.h>
// [END imports]

// [START hw_imports]
#include <fidl/fuchsia.hardware.pci/cpp/wire.h>
#include <lib/async/cpp/irq.h>
#include <lib/driver/mmio/cpp/mmio-buffer.h>
#include <lib/zx/interrupt.h>
// [END hw_imports]

// [START namespace_start]
namespace edu_device {
// [END namespace_start]

// [START register_definitions]
// Register offset addresses for edu device MMIO area
constexpr uint32_t kIdentificationOffset = 0x00;
constexpr uint32_t kLivenessCheckOffset = 0x04;
constexpr uint32_t kFactorialComputationOffset = 0x08;
constexpr uint32_t kStatusRegisterOffset = 0x20;
constexpr uint32_t kInterruptStatusRegisterOffset = 0x24;
constexpr uint32_t kInterruptRaiseRegisterOffset = 0x60;
constexpr uint32_t kInterruptAcknowledgeRegisterOffset = 0x64;
constexpr uint32_t kDmaSourceAddressOffset = 0x80;
constexpr uint32_t kDmaDestinationAddressOffset = 0x80;
constexpr uint32_t kDmaTransferCountOffset = 0x90;
constexpr uint32_t kDmaCommandRegisterOffset = 0x98;

class Identification : public hwreg::RegisterBase<Identification, uint32_t> {
 public:
  DEF_FIELD(31, 24, major_version);
  DEF_FIELD(23, 16, minor_version);
  DEF_FIELD(15, 0, edu);

  static auto Get() { return hwreg::RegisterAddr<Identification>(kIdentificationOffset); }
};

class Status : public hwreg::RegisterBase<Status, uint32_t> {
 public:
  DEF_BIT(0, busy);
  DEF_BIT(7, irq_enable);

  static auto Get() { return hwreg::RegisterAddr<Status>(kStatusRegisterOffset); }
};
// [END register_definitions]

// [START class_header]
// Interacts with the device hardware using a fuchsia.hardware.pci client.
class QemuEduDevice {
  // [END class_header]
  // [START public_main]
 public:
  explicit QemuEduDevice(async_dispatcher_t* dispatcher,
                         fidl::ClientEnd<fuchsia_hardware_pci::Device> pci)
      : dispatcher_(dispatcher), pci_(std::move(pci)) {}

  zx::result<> MapInterruptAndMmio();
  // [END public_main]

  // [START public_registers]
  void ComputeFactorial(uint32_t input, fit::callback<void(zx::result<uint32_t>)> callback);
  zx::result<uint32_t> LivenessCheck(uint32_t challenge);

  Identification IdentificationRegister() { return Identification::Get().ReadFrom(&*mmio_); }
  Status StatusRegister() { return Status::Get().ReadFrom(&*mmio_); }
  // [END public_registers]

  // [START private_main]
 private:
  void HandleIrq(async_dispatcher_t* dispatcher, async::IrqBase* irq, zx_status_t status,
                 const zx_packet_interrupt_t* interrupt);

  async_dispatcher_t* const dispatcher_;

  fidl::WireSyncClient<fuchsia_hardware_pci::Device> pci_;
  std::optional<fdf::MmioBuffer> mmio_;
  zx::interrupt irq_;
  async::IrqMethod<QemuEduDevice, &QemuEduDevice::HandleIrq> irq_method_{this};
  std::optional<fit::callback<void(zx::result<uint32_t>)>> pending_callback_;
  // [END private_main]

  // [START class_footer]
};
// [END class_footer]

// [START namespace_end]
}  // namespace edu_device
// [END namespace_end]

#endif  // FUCHSIA_SDK_EXAMPLES_QEMU_EDU_DRIVERS_EDU_DEVICE_H_
