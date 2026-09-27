// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// [START imports]
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <lib/component/incoming/cpp/protocol.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
// [END imports]

// [START fidl_imports]
#include <fidl/examples.qemuedu/cpp/wire.h>
// [END fidl_imports]

// [START cli_helpers]
int usage(const char* cmd) {
  fprintf(stderr,
          "\nInteract with the QEMU edu device:\n"
          "   %s live                       Performs a card liveness check\n"
          "   %s fact <n>                   Computes the factorial of n\n"
          "   %s help                       Print this message\n",
          cmd, cmd, cmd);
  return -1;
}

// Returns "true" if the argument matches the prefix.
// In this case, moves the argument past the prefix.
bool prefix_match(const char** arg, const char* prefix) {
  if (!strncmp(*arg, prefix, strlen(prefix))) {
    *arg += strlen(prefix);
    return true;
  }
  return false;
}

constexpr long kBadParse = -1;
long parse_positive_long(const char* number) {
  char* end;
  long result = strtol(number, &end, 10);
  if (end == number || *end != '\0' || result < 0) {
    return kBadParse;
  }
  return result;
}
// [END cli_helpers]

// [START service_client]
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

// Open a FIDL client connection to the examples.qemuedu.Device
zx::result<fidl::WireSyncClient<examples_qemuedu::Device>> ConnectToDevice() {
  zx::result<fidl::ClientEnd<examples_qemuedu::Device>> client_end = ConnectViaDevfs();
  if (client_end.is_error()) {
    fprintf(stderr, "Failed to connect to /dev/class/test: %s\n", client_end.status_string());
    return client_end.take_error();
  }
  return zx::ok(fidl::WireSyncClient(std::move(*client_end)));
}
// [END service_client]

// [START liveness_check]
// Run a liveness check on the QEMU edu device.
// Returns 0 on success.
int liveness_check() {
  auto client_result = ConnectToDevice();
  if (client_result.is_error()) {
    return -1;
  }
  auto& client = client_result.value();

  auto liveness_check_result = client->LivenessCheck();
  if (!liveness_check_result.ok()) {
    fprintf(stderr, "Error: failed to get liveness check result: %s\n",
            zx_status_get_string(liveness_check_result.status()));
    return -1;
  }

  if (liveness_check_result->value()->result) {
    printf("Liveness check passed!\n");
    return 0;
  } else {
    printf("Liveness check failed!\n");
    return -1;
  }
}
// [END liveness_check]

// [START compute_factorial]
// Compute the factorial of n using the QEMU edu device.
// Returns 0 on success.
int compute_factorial(long n) {
  auto client_result = ConnectToDevice();
  if (client_result.is_error()) {
    return -1;
  }
  auto& client = client_result.value();

  if (n >= std::numeric_limits<uint32_t>::max()) {
    fprintf(stderr, "N is too large\n");
    return -1;
  }

  uint32_t input = static_cast<uint32_t>(n);
  auto compute_factorial_result = client->ComputeFactorial(input);
  if (!compute_factorial_result.ok()) {
    fprintf(stderr, "Error: failed to call compute factorial result: %s\n",
            zx_status_get_string(compute_factorial_result.status()));
    return -1;
  }

  printf("Factorial(%u) = %u\n", input, compute_factorial_result->value()->output);

  return 0;
}
// [END compute_factorial]

// [START main]
int main(int argc, char* argv[]) {
  const char* cmd = basename(argv[0]);

  // If no arguments passed, bail out after dumping
  // usage information.
  if (argc < 2) {
    return usage(cmd);
  }

  const char* arg = argv[1];
  if (prefix_match(&arg, "live")) {
    return liveness_check();
  } else if (prefix_match(&arg, "fact")) {
    if (argc < 3) {
      fprintf(stderr, "Expecting 1 argument\n");
      return usage(cmd);
    }
    long n = parse_positive_long(argv[2]);
    return compute_factorial(n);
  }

  return usage(cmd);
}
// [END main]
