// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// [START imports]
#include "edu_device.h"

#include <lib/driver/logging/cpp/structured_logger.h>
// [END imports]

// [START namespace_start]
namespace edu_device {
// [END namespace_start]

// [START interrupt_mmio]
// Initialize PCI device hardware resources
zx::result<> QemuEduDevice::MapInterruptAndMmio() {
  // Retrieve the Base Address Register (BAR) for PCI Region 0
  auto bar = pci_->GetBar(0);
  if (!bar.ok()) {
    FDF_SLOG(ERROR, "failed to get bar", KV("status", bar.status()));
    return zx::error(bar.status());
  }
  if (bar->is_error()) {
    FDF_SLOG(ERROR, "failed to get bar", KV("status", bar->error_value()));
    return zx::error(bar->error_value());
  }

  // Create a Memory-Mapped I/O (MMIO) region over BAR0
  {
    auto& bar_result = bar->value()->result;
    if (!bar_result.result.is_vmo()) {
      FDF_SLOG(ERROR, "unexpected bar type");
      return zx::error(ZX_ERR_NO_RESOURCES);
    }
    zx::result<fdf::MmioBuffer> mmio = fdf::MmioBuffer::Create(
        0, bar_result.size, std::move(bar_result.result.vmo()), ZX_CACHE_POLICY_UNCACHED_DEVICE);
    if (mmio.is_error()) {
      FDF_SLOG(ERROR, "failed to map mmio", KV("status", mmio.status_value()));
      return mmio.take_error();
    }
    mmio_ = *std::move(mmio);
  }

  // Configure interrupt handling for the device using INTx
  auto result = pci_->SetInterruptMode(fuchsia_hardware_pci::wire::InterruptMode::kLegacy, 1);
  if (!result.ok()) {
    FDF_SLOG(ERROR, "failed configure interrupt mode", KV("status", result.status()));
    return zx::error(result.status());
  }
  if (result->is_error()) {
    FDF_SLOG(ERROR, "failed configure interrupt mode", KV("status", result->error_value()));
    return zx::error(result->error_value());
  }

  // Map the device's interrupt to a system IRQ
  auto interrupt = pci_->MapInterrupt(0);
  if (!interrupt.ok()) {
    FDF_SLOG(ERROR, "failed to map interrupt", KV("status", interrupt.status()));
    return zx::error(interrupt.status());
  }
  if (interrupt->is_error()) {
    FDF_SLOG(ERROR, "failed to map interrupt", KV("status", interrupt->error_value()));
    return zx::error(interrupt->error_value());
  }
  irq_ = std::move(interrupt->value()->interrupt);
  // Start listening for interrupts.
  irq_method_.set_object(irq_.get());
  irq_method_.Begin(dispatcher_);

  return zx::ok();
}
// [END interrupt_mmio]

// [START compute_factorial]
// Write data into the factorial register wait for an interrupt.
void QemuEduDevice::ComputeFactorial(uint32_t input,
                                     fit::callback<void(zx::result<uint32_t>)> callback) {
  if (pending_callback_.has_value()) {
    callback(zx::error(ZX_ERR_SHOULD_WAIT));
  }

  // Tell the device to raise an interrupt after computation.
  auto status = StatusRegister();
  status.set_irq_enable(true);
  status.WriteTo(&*mmio_);

  // Write the value into the factorial register to start computation.
  mmio_->Write32(input, kFactorialComputationOffset);

  // We will receive an interrupt when the computation completes.
  pending_callback_ = std::move(callback);
}

// Respond to INTx interrupts triggered by the device, and return the compute result.
void QemuEduDevice::HandleIrq(async_dispatcher_t* dispatcher, async::IrqBase* irq,
                              zx_status_t status, const zx_packet_interrupt_t* interrupt) {
  irq_.ack();
  if (!pending_callback_.has_value()) {
    FDF_LOG(ERROR, "Received unexpected interrupt!");
    return;
  }
  auto callback = std::move(*pending_callback_);
  pending_callback_ = std::nullopt;
  if (status != ZX_OK) {
    FDF_SLOG(ERROR, "Failed to wait for interrupt", KV("status", zx_status_get_string(status)));
    callback(zx::error(status));
    return;
  }

  // Acknowledge the interrupt with the edu device.
  auto int_status = mmio_->Read32(kInterruptStatusRegisterOffset);
  mmio_->Write32(int_status, kInterruptAcknowledgeRegisterOffset);

  // Deassert the legacy INTx interrupt on the PCI bus.
  auto irq_result = pci_->AckInterrupt();
  if (!irq_result.ok() || irq_result->is_error()) {
    FDF_SLOG(ERROR, "Failed to ack PCI interrupt",
             KV("status", irq_result.ok() ? irq_result->error_value() : irq_result.status()));
    callback(zx::error(ZX_ERR_IO));
    return;
  }

  // Reply with the result.
  uint32_t factorial = mmio_->Read32(kFactorialComputationOffset);
  FDF_SLOG(INFO, "Replying with", KV("factorial", factorial));
  callback(zx::ok(factorial));
}
// [END compute_factorial]

// [START liveness_check]
// Write a challenge value to the liveness check register and return the result.
zx::result<uint32_t> QemuEduDevice::LivenessCheck(uint32_t challenge) {
  // Write the challenge value to the liveness check register.
  mmio_->Write32(challenge, kLivenessCheckOffset);

  // Return the result.
  auto value = mmio_->Read32(kLivenessCheckOffset);
  return zx::ok(value);
}
// [END liveness_check]

// [START namespace_end]
}  // namespace edu_device
// [END namespace_end]
