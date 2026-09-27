// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// [START imports]
#include <lib/component/incoming/cpp/protocol.h>
#include <unistd.h>

#include <filesystem>

#include <gtest/gtest.h>

#include "fidl/examples.qemuedu/cpp/wire.h"
// [END imports]

// [START main_body]
namespace {

// The driver's examples.qemuedu.Service is not routed beyond the driver
// collection on prebuilt product images, so connect through the devfs node the
// driver publishes instead. Waits up to 10 s for the node to appear.
zx::result<fidl::ClientEnd<examples_qemuedu::Device>> ConnectViaDevfs() {
  constexpr char kClassDir[] = "/dev/class/test";
  for (int attempt = 0; attempt < 100; attempt++) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(kClassDir, ec)) {
      return component::Connect<examples_qemuedu::Device>(entry.path().c_str());
    }
    usleep(100'000);
  }
  return zx::error(ZX_ERR_NOT_FOUND);
}

class QemuEduSystemTest : public testing::Test {
 public:
  void SetUp() override {
    zx::result<fidl::ClientEnd<examples_qemuedu::Device>> client_end = ConnectViaDevfs();
    ASSERT_EQ(client_end.status_value(), ZX_OK);
    device_ = fidl::WireSyncClient(std::move(*client_end));
  }

  fidl::WireSyncClient<examples_qemuedu::Device>& device() { return device_; }

 private:
  fidl::WireSyncClient<examples_qemuedu::Device> device_;
};

TEST_F(QemuEduSystemTest, LivenessCheck) {
  fidl::WireResult result = device()->LivenessCheck();
  ASSERT_EQ(result.status(), ZX_OK);
  ASSERT_TRUE(result->value()->result);
}

TEST_F(QemuEduSystemTest, ComputeFactorial) {
  std::array<uint32_t, 11> kExpected = {
      1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800,
  };
  for (uint32_t i = 0; i < kExpected.size(); i++) {
    fidl::WireResult result = device()->ComputeFactorial(i);
    ASSERT_EQ(result.status(), ZX_OK);
    EXPECT_EQ(result->value()->output, kExpected[i]);
  }
}

}  // namespace
// [END main_body]
