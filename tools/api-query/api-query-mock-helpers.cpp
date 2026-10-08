//===- api-query-mock-helpers.cpp - GPU Query Mock Helpers ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "api-query-mock-helpers.h"
#include "DXFeatures.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"

#include <cassert>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

using namespace llvm;
using namespace offloadtest;

static cl::opt<std::string> MockDisableFeatures(
    "mock-disable-features",
    cl::desc("Report a mock DirectX GPU instead of querying hardware, disabling "
             "the listed capabilities (an empty list uses the baseline)"),
    cl::value_desc("capability,..."), cl::ValueRequired);

namespace {
struct MockCapability {
  std::string Value;
  std::string UnsupportedValue;
};

// Convert a feature value to the text used in the report.
template <typename T> std::string getMockValue(T Value) {
  if constexpr (std::is_same_v<T, bool>) {
    return CapabilityValueBool::valueToString(Value);
  } else if constexpr (std::is_enum_v<T>) {
    return CapabilityPrinter<T>::toString(Value);
  } else {
    static_assert(std::is_same_v<T, uint32_t>);
    return CapabilityValueUnsigned::valueToString(Value);
  }
}

// These values are only printed, never used to run GPU operations.
class MockCapabilityValue final : public CapabilityValueBase {
  std::string Value;

public:
  // Keep a copy of the value's text after the temporary defaults map is gone.
  explicit MockCapabilityValue(StringRef Value) : Value(Value.str()) {}

  std::string toString() const override { return Value; }
};
} // namespace

// Load defaults from DXFeatures.def and disable only the requested capabilities.
static Expected<Capabilities> createMockCapabilities(StringRef DisabledFeatures) {
  StringMap<MockCapability> CapabilitiesByName;
#define D3D_FEATURE_BOOL(Name) \
  CapabilitiesByName[#Name] = {getMockValue(true), getMockValue(false)};
#define D3D_FEATURE_ENUM(Type, Name) CapabilitiesByName.try_emplace(#Name);
#define D3D_FEATURE_UINT(Name) CapabilitiesByName.try_emplace(#Name);
#define D3D_MOCK_DEFAULT(Name, Default) \
  CapabilitiesByName[#Name].Value = getMockValue(Default);
#define D3D_MOCK_UNSUPPORTED(Name, Unsupported) \
  CapabilitiesByName[#Name].UnsupportedValue = getMockValue(Unsupported);
#include "DXFeatures.def"

  for (const auto &C : CapabilitiesByName) {
    if (C.second.Value.empty()) {
      return createStringError(inconvertibleErrorCode(),
                               "no mock default for DirectX capability '%s'",
                               C.getKey().str().c_str());
    }
  }

  if (!DisabledFeatures.empty()) {
    SmallVector<StringRef> Names;
    DisabledFeatures.split(Names, ',');
    for (StringRef Name : Names) {
      Name = Name.trim();
      auto It = CapabilitiesByName.find(Name);
      if (It == CapabilitiesByName.end()) {
        return createStringError(inconvertibleErrorCode(),
                                 "unknown DirectX capability '%s'",
                                 Name.str().c_str());
      }
      StringRef Unsupported = It->second.UnsupportedValue;
      if (Unsupported.empty()) {
        return createStringError(inconvertibleErrorCode(),
                                 "capability '%s' has no unsupported value",
                                 Name.str().c_str());
      }
      It->second.Value = It->second.UnsupportedValue;
    }
  }

  Capabilities Result;
  for (const auto &C : CapabilitiesByName) {
    auto Data = std::make_shared<MockCapabilityValue>(C.second.Value);
    Result.insert({C.getKey(), Capability(C.getKey(), std::move(Data))});
  }
  return Result;
}

// Own a real device and forward report queries to it.
QueryDevice::QueryDevice(std::unique_ptr<Device> RealDevice)
    : RealDevice(std::move(RealDevice)) {
  assert(this->RealDevice && "Normal mode requires a real device");
}

// Store mock capabilities without creating or querying a hardware device.
QueryDevice::QueryDevice(Capabilities MockCapabilities)
    : MockCapabilities(std::move(MockCapabilities)) {}

StringRef QueryDevice::getAPIName() const {
  if (!RealDevice) {
    return "DirectX";
  }
  return RealDevice->getAPIName();
}

StringRef QueryDevice::getDescription() const {
  if (!RealDevice) {
    return "Mock GPU";
  }
  return RealDevice->getDescription();
}

StringRef QueryDevice::getDriverName() const {
  if (!RealDevice) {
    return "DirectX";
  }
  return RealDevice->getDriverName();
}

StringRef QueryDevice::getDriverVersion() const {
  if (!RealDevice) {
    return "0.0";
  }
  return RealDevice->getDriverVersion();
}

StringRef QueryDevice::getGPUGeneration() const {
  if (!RealDevice) {
    return "Unknown";
  }
  return RealDevice->getGPUGeneration();
}

uint16_t QueryDevice::getFamilyPrefix() const {
  if (!RealDevice) {
    return 0;
  }
  return RealDevice->getFamilyPrefix();
}

const Capabilities &QueryDevice::getCapabilities() {
  if (!RealDevice) {
    return MockCapabilities;
  }
  return RealDevice->getCapabilities();
}

void QueryDevice::printExtra(raw_ostream &OS) {
  if (!RealDevice) {
    return;
  }
  RealDevice->printExtra(OS);
}

// Select one mock device when requested, otherwise enumerate real devices.
Expected<SmallVector<std::unique_ptr<QueryDevice>>>
offloadtest::initializeQueryDevices(const DeviceConfig Config) {
  SmallVector<std::unique_ptr<QueryDevice>> Result;
  if (MockDisableFeatures.getNumOccurrences()) {
    auto CapabilitiesOrErr =
        createMockCapabilities(MockDisableFeatures.getValue());
    if (!CapabilitiesOrErr) {
      return CapabilitiesOrErr.takeError();
    }
    Result.push_back(
        std::make_unique<QueryDevice>(std::move(*CapabilitiesOrErr)));
    return Result;
  }

  auto DevicesOrErr = initializeDevices(Config);
  if (!DevicesOrErr) {
    return DevicesOrErr.takeError();
  }
  for (auto &D : *DevicesOrErr) {
    Result.push_back(std::make_unique<QueryDevice>(std::move(D)));
  }
  return Result;
}
