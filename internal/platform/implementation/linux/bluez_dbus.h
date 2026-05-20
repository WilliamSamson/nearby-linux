// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLUEZ_DBUS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLUEZ_DBUS_H_

// Thin C++ wrapper around the BlueZ D-Bus APIs used by the Nearby Connections
// BLE medium on Linux. This keeps all sd-bus interaction (or GDBus, depending
// on available library) in one compilation unit so the medium classes can
// remain library-agnostic.
//
// We use GDBus (part of GLib/GIO) because:
//  - It's already a transitive dependency of the GTK UI.
//  - The mock_facade.c already demonstrates the pattern.
//  - It works in both synchronous and asynchronous modes.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

// Forward-declare GLib/GIO types to avoid leaking the GLib header into every
// consumer. The implementation (.cc) includes the real headers.
typedef struct _GDBusConnection GDBusConnection;

namespace nearby {
namespace linux_impl {
namespace bluez {

// ---- Constants ------------------------------------------------------------

inline constexpr char kBluezBusName[] = "org.bluez";
inline constexpr char kAdapterPath[] = "/org/bluez/hci0";
inline constexpr char kAdapterIface[] = "org.bluez.Adapter1";
inline constexpr char kAdvManagerIface[] = "org.bluez.LEAdvertisingManager1";
inline constexpr char kGattManagerIface[] = "org.bluez.GattManager1";
inline constexpr char kDeviceIface[] = "org.bluez.Device1";
inline constexpr char kGattServiceIface[] = "org.bluez.GattService1";
inline constexpr char kGattCharacteristicIface[] =
    "org.bluez.GattCharacteristic1";
inline constexpr char kGattDescriptorIface[] = "org.bluez.GattDescriptor1";
inline constexpr char kPropertiesIface[] = "org.freedesktop.DBus.Properties";

// ---- System bus singleton -------------------------------------------------

// Returns a shared reference to the system D-Bus connection. The connection
// is established lazily on first call and cached for the process lifetime.
// Returns nullptr on failure.
GDBusConnection* GetSystemBus();

// ---- Adapter queries ------------------------------------------------------

// Returns true if the BlueZ adapter at `kAdapterPath` exists and is powered.
bool IsAdapterPowered();

// Returns the Bluetooth MAC address of the default adapter, or empty string.
std::string GetAdapterAddress();

// ---- BLE Advertising ------------------------------------------------------

// Unique handle for a registered BLE advertisement. The advertisement is
// unregistered when the handle is destroyed (RAII).
class AdvertisementHandle {
 public:
  ~AdvertisementHandle();

  // Non-copyable, movable.
  AdvertisementHandle(const AdvertisementHandle&) = delete;
  AdvertisementHandle& operator=(const AdvertisementHandle&) = delete;
  AdvertisementHandle(AdvertisementHandle&& other) noexcept;
  AdvertisementHandle& operator=(AdvertisementHandle&& other) noexcept;

 private:
  friend class AdvertisementBuilder;
  AdvertisementHandle(GDBusConnection* bus, std::string object_path,
                      uint32_t registration_id);

  GDBusConnection* bus_ = nullptr;
  std::string object_path_;
  uint32_t registration_id_ = 0;
};

// Builder for creating and registering a BlueZ LE advertisement.
class AdvertisementBuilder {
 public:
  AdvertisementBuilder();

  // Sets the advertisement type ("broadcast" or "peripheral").
  AdvertisementBuilder& SetType(absl::string_view type);

  // Adds a service UUID to advertise.
  AdvertisementBuilder& AddServiceUuid(absl::string_view uuid);

  // Adds service data for a UUID.
  AdvertisementBuilder& AddServiceData(absl::string_view uuid,
                                       const ByteArray& data);

  // Sets whether the advertisement is connectable.
  AdvertisementBuilder& SetConnectable(bool connectable);

  // Registers the advertisement with BlueZ. Returns a handle that
  // auto-unregisters on destruction.
  absl::StatusOr<std::unique_ptr<AdvertisementHandle>> Register();

 private:
  std::string type_ = "peripheral";
  std::vector<std::string> service_uuids_;
  std::vector<std::pair<std::string, ByteArray>> service_data_;
  bool connectable_ = true;
};

// ---- BLE Scanning ---------------------------------------------------------

struct ScanResult {
  std::string device_path;     // D-Bus object path
  std::string address;         // Bluetooth address
  std::vector<std::pair<std::string, ByteArray>> service_data;
};

using ScanCallback =
    std::function<void(const ScanResult& result)>;

// Starts BLE scanning with a filter for the given service UUID.
// Returns a subscription ID that can be passed to StopScan().
absl::StatusOr<uint32_t> StartScan(absl::string_view service_uuid,
                                   ScanCallback callback);

// Stops a previously started scan.
void StopScan(uint32_t subscription_id);

// ---- GATT Server ----------------------------------------------------------

struct GattCharacteristicDef {
  std::string uuid;
  uint32_t flags;  // bitfield: kRead=1, kWrite=2, kNotify=4, kIndicate=8
  static constexpr uint32_t kRead = 1;
  static constexpr uint32_t kWrite = 2;
  static constexpr uint32_t kNotify = 4;
  static constexpr uint32_t kIndicate = 8;
};

struct GattServiceDef {
  std::string uuid;
  bool primary = true;
  std::vector<GattCharacteristicDef> characteristics;
};

// Callbacks for GATT server events.
struct GattServerCallbacks {
  // Called when a remote device reads a characteristic.
  std::function<ByteArray(const std::string& char_uuid, int offset)> on_read;
  // Called when a remote device writes to a characteristic.
  std::function<void(const std::string& char_uuid, const ByteArray& value)>
      on_write;
  // Called when a remote device subscribes to notifications.
  std::function<void(const std::string& char_uuid)> on_subscribe;
  // Called when a remote device unsubscribes.
  std::function<void(const std::string& char_uuid)> on_unsubscribe;
};

// Handle for a registered GATT application. The GATT services are
// unregistered when the handle is destroyed.
class GattServerHandle {
 public:
  struct Impl;

  ~GattServerHandle();

  GattServerHandle(const GattServerHandle&) = delete;
  GattServerHandle& operator=(const GattServerHandle&) = delete;
  GattServerHandle(GattServerHandle&&) noexcept;
  GattServerHandle& operator=(GattServerHandle&&) noexcept;
  GattServerHandle() = default;

  // Updates the value of a characteristic. If subscribers exist, a
  // notification/indication is sent.
  absl::Status UpdateCharacteristic(absl::string_view char_uuid,
                                    const ByteArray& value);

 private:
  friend absl::StatusOr<std::unique_ptr<GattServerHandle>> RegisterGattServer(
      const GattServiceDef& service, GattServerCallbacks callbacks);

  std::unique_ptr<Impl> impl_;
};

// Registers a GATT server application with BlueZ.
absl::StatusOr<std::unique_ptr<GattServerHandle>> RegisterGattServer(
    const GattServiceDef& service, GattServerCallbacks callbacks);

// ---- GATT Client ----------------------------------------------------------

// Simplified GATT client that connects to a remote device's GATT server.
class GattClientConnection {
 public:
  ~GattClientConnection();

  GattClientConnection(const GattClientConnection&) = delete;
  GattClientConnection& operator=(const GattClientConnection&) = delete;
  GattClientConnection() = default;

  // Discovers services and characteristics.
  absl::Status DiscoverServices(absl::string_view service_uuid);

  // Reads a characteristic value.
  absl::StatusOr<ByteArray> ReadCharacteristic(
      absl::string_view service_uuid, absl::string_view char_uuid);

  // Writes a characteristic value.
  absl::Status WriteCharacteristic(absl::string_view service_uuid,
                                   absl::string_view char_uuid,
                                   const ByteArray& value);

  // Subscribes to characteristic notifications.
  absl::Status StartNotify(
      absl::string_view service_uuid, absl::string_view char_uuid,
      std::function<void(const ByteArray& value)> callback);

  // Disconnects from the remote device.
  void Disconnect();

 private:
  friend absl::StatusOr<std::unique_ptr<GattClientConnection>>
  ConnectGattClient(absl::string_view device_path);

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Connects to a remote BLE device's GATT server.
absl::StatusOr<std::unique_ptr<GattClientConnection>> ConnectGattClient(
    absl::string_view device_path);

}  // namespace bluez
}  // namespace linux_impl
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_BLUEZ_DBUS_H_
