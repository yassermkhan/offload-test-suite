//===- api-query-mock-helpers.h - GPU Query Mock Helpers --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef OFFLOADTEST_API_QUERY_MOCK_HELPERS_H
#define OFFLOADTEST_API_QUERY_MOCK_HELPERS_H

#include "API/Device.h"

namespace offloadtest {

// Supplies report data without implementing GPU execution operations.
class QueryDevice {
  std::unique_ptr<Device> RealDevice;
  Capabilities MockCapabilities;

public:
  explicit QueryDevice(std::unique_ptr<Device> RealDevice);
  explicit QueryDevice(Capabilities MockCapabilities);

  llvm::StringRef getAPIName() const;
  llvm::StringRef getDescription() const;
  llvm::StringRef getDriverName() const;
  llvm::StringRef getDriverVersion() const;
  llvm::StringRef getGPUGeneration() const;
  uint16_t getFamilyPrefix() const;
  const Capabilities &getCapabilities();
  void printExtra(llvm::raw_ostream &OS);
};

llvm::Expected<llvm::SmallVector<std::unique_ptr<QueryDevice>>>
initializeQueryDevices(const DeviceConfig Config);

} // namespace offloadtest

#endif // OFFLOADTEST_API_QUERY_MOCK_HELPERS_H
