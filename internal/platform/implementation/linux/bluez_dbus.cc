// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0

#include "internal/platform/implementation/linux/bluez_dbus.h"

#include <gio/gio.h>
#include <glib.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

// `linux` is sometimes a predefined macro.
#ifdef linux
#undef linux
#endif

namespace nearby {
namespace linux_impl {
namespace bluez {

// ---- System bus singleton -------------------------------------------------

static std::mutex g_bus_mutex;
static GDBusConnection* g_system_bus = nullptr;

GDBusConnection* GetSystemBus() {
  std::lock_guard<std::mutex> lock(g_bus_mutex);
  if (g_system_bus && !g_dbus_connection_is_closed(g_system_bus)) {
    return g_system_bus;
  }
  GError* error = nullptr;
  g_system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
  if (!g_system_bus) {
    g_warning("bluez: failed to connect to system bus: %s",
              error ? error->message : "unknown");
    g_clear_error(&error);
    return nullptr;
  }
  return g_system_bus;
}

// ---- Adapter queries ------------------------------------------------------

bool IsAdapterPowered() {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return false;

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kPropertiesIface, "Get",
      g_variant_new("(ss)", kAdapterIface, "Powered"),
      G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (!result) {
    g_clear_error(&error);
    return false;
  }
  GVariant* inner = nullptr;
  g_variant_get(result, "(v)", &inner);
  gboolean powered = g_variant_get_boolean(inner);
  g_variant_unref(inner);
  g_variant_unref(result);
  return powered;
}

std::string GetAdapterAddress() {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return "";

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kPropertiesIface, "Get",
      g_variant_new("(ss)", kAdapterIface, "Address"),
      G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (!result) {
    g_clear_error(&error);
    return "";
  }
  GVariant* inner = nullptr;
  g_variant_get(result, "(v)", &inner);
  const char* addr = g_variant_get_string(inner, nullptr);
  std::string address(addr ? addr : "");
  g_variant_unref(inner);
  g_variant_unref(result);
  return address;
}

// ---- Counter for unique D-Bus object paths --------------------------------

static std::atomic<uint32_t> g_path_counter{0};
static constexpr char kObjectManagerIface[] =
    "org.freedesktop.DBus.ObjectManager";

static std::string NextObjectPath(absl::string_view prefix) {
  uint32_t id = g_path_counter.fetch_add(1);
  return absl::StrCat(prefix, id);
}

// ---- BLE Advertisement ----------------------------------------------------

static const char kAdvIntrospectionXml[] =
    "<node>"
    "  <interface name='org.bluez.LEAdvertisement1'>"
    "    <method name='Release'/>"
    "    <property name='Type' type='s' access='read'/>"
    "    <property name='ServiceUUIDs' type='as' access='read'/>"
    "    <property name='ServiceData' type='a{sv}' access='read'/>"
    "  </interface>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='properties' type='a{sv}' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

/* LINT.IfChange */
static const char kGattServiceIntrospectionXml[] =
    "<node>"
    "  <interface name='org.bluez.GattService1'>"
    "    <property name='UUID' type='s' access='read'/>"
    "    <property name='Primary' type='b' access='read'/>"
    "  </interface>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='properties' type='a{sv}' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";

static const char kGattCharacteristicIntrospectionXml[] =
    "<node>"
    "  <interface name='org.bluez.GattCharacteristic1'>"
    "    <method name='ReadValue'>"
    "      <arg name='options' type='a{sv}' direction='in'/>"
    "      <arg name='value' type='ay' direction='out'/>"
    "    </method>"
    "    <method name='WriteValue'>"
    "      <arg name='value' type='ay' direction='in'/>"
    "      <arg name='options' type='a{sv}' direction='in'/>"
    "    </method>"
    "    <method name='StartNotify'/>"
    "    <method name='StopNotify'/>"
    "    <property name='UUID' type='s' access='read'/>"
    "    <property name='Service' type='o' access='read'/>"
    "    <property name='Value' type='ay' access='read'/>"
    "    <property name='Flags' type='as' access='read'/>"
    "  </interface>"
    "  <interface name='org.freedesktop.DBus.Properties'>"
    "    <method name='Get'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='name' type='s' direction='in'/>"
    "      <arg name='value' type='v' direction='out'/>"
    "    </method>"
    "    <method name='GetAll'>"
    "      <arg name='interface' type='s' direction='in'/>"
    "      <arg name='properties' type='a{sv}' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>";
/* LINT.ThenChange(//depot/google3/third_party/nearby/internal/platform/implementation/linux/bluez_dbus.cc) */

struct AdvContext {
  std::string type;
  std::vector<std::string> service_uuids;
  std::vector<std::pair<std::string, ByteArray>> service_data;
};

static GVariant* BuildServiceDataVariant(const AdvContext* ctx) {
  GVariantBuilder sd_builder;
  g_variant_builder_init(&sd_builder, G_VARIANT_TYPE("a{sv}"));
  for (const auto& [uuid, data] : ctx->service_data) {
    GVariantBuilder bytes;
    g_variant_builder_init(&bytes, G_VARIANT_TYPE("ay"));
    for (size_t i = 0; i < data.size(); i++) {
      g_variant_builder_add(&bytes, "y",
                            static_cast<guchar>(data.data()[i]));
    }
    g_variant_builder_add(&sd_builder, "{sv}", uuid.c_str(),
                          g_variant_builder_end(&bytes));
  }
  return g_variant_builder_end(&sd_builder);
}

static GVariant* AdvGetProperty(const AdvContext* ctx, const char* name) {
  if (g_strcmp0(name, "Type") == 0) {
    return g_variant_new_string(ctx->type.c_str());
  }
  if (g_strcmp0(name, "ServiceUUIDs") == 0) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    for (const auto& uuid : ctx->service_uuids) {
      g_variant_builder_add(&builder, "s", uuid.c_str());
    }
    return g_variant_builder_end(&builder);
  }
  if (g_strcmp0(name, "ServiceData") == 0) {
    return BuildServiceDataVariant(ctx);
  }
  return nullptr;
}

static void HandleAdvMethodCall(
    GDBusConnection*, const char*, const char*, const char* interface_name,
    const char* method_name, GVariant* parameters,
    GDBusMethodInvocation* invocation, gpointer user_data) {
  auto* ctx = static_cast<AdvContext*>(user_data);

  if (g_strcmp0(interface_name, "org.bluez.LEAdvertisement1") == 0 &&
      g_strcmp0(method_name, "Release") == 0) {
    g_dbus_method_invocation_return_value(invocation, nullptr);
    return;
  }

  if (g_strcmp0(interface_name, kPropertiesIface) == 0) {
    if (g_strcmp0(method_name, "Get") == 0) {
      const char* req_iface = nullptr;
      const char* prop_name = nullptr;
      g_variant_get(parameters, "(&s&s)", &req_iface, &prop_name);
      GVariant* val = AdvGetProperty(ctx, prop_name);
      if (val) {
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(v)", val));
      } else {
        g_dbus_method_invocation_return_error(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
            "Unknown property");
      }
      return;
    }
    if (g_strcmp0(method_name, "GetAll") == 0) {
      GVariantBuilder builder;
      g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
      const char* props[] = {"Type", "ServiceUUIDs", "ServiceData"};
      for (auto& p : props) {
        GVariant* v = AdvGetProperty(ctx, p);
        if (v) g_variant_builder_add(&builder, "{sv}", p, v);
      }
      g_dbus_method_invocation_return_value(
          invocation, g_variant_new("(@a{sv})", g_variant_builder_end(&builder)));
      return;
    }
  }

  g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                        G_DBUS_ERROR_UNKNOWN_METHOD,
                                        "Unknown method");
}

static GVariant* HandleAdvGetProp(
    GDBusConnection*, const char*, const char*, const char*,
    const char* property_name, GError** error, gpointer user_data) {
  auto* ctx = static_cast<AdvContext*>(user_data);
  GVariant* val = AdvGetProperty(ctx, property_name);
  if (!val) {
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                "Unknown property");
  }
  return val;
}

static const GDBusInterfaceVTable kAdvVtable = {
    HandleAdvMethodCall, HandleAdvGetProp, nullptr, {0}};

AdvertisementHandle::AdvertisementHandle(GDBusConnection* bus,
                                         std::string object_path,
                                         uint32_t registration_id)
    : bus_(bus),
      object_path_(std::move(object_path)),
      registration_id_(registration_id) {}

AdvertisementHandle::~AdvertisementHandle() {
  if (!bus_ || object_path_.empty()) return;

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus_, kBluezBusName, kAdapterPath, kAdvManagerIface,
      "UnregisterAdvertisement",
      g_variant_new("(o)", object_path_.c_str()), nullptr,
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (result) g_variant_unref(result);
  g_clear_error(&error);

  if (registration_id_) {
    g_dbus_connection_unregister_object(bus_, registration_id_);
  }
}

AdvertisementHandle::AdvertisementHandle(AdvertisementHandle&& other) noexcept
    : bus_(other.bus_),
      object_path_(std::move(other.object_path_)),
      registration_id_(other.registration_id_) {
  other.bus_ = nullptr;
  other.registration_id_ = 0;
}

AdvertisementHandle& AdvertisementHandle::operator=(
    AdvertisementHandle&& other) noexcept {
  if (this != &other) {
    bus_ = other.bus_;
    object_path_ = std::move(other.object_path_);
    registration_id_ = other.registration_id_;
    other.bus_ = nullptr;
    other.registration_id_ = 0;
  }
  return *this;
}

AdvertisementBuilder::AdvertisementBuilder() = default;

AdvertisementBuilder& AdvertisementBuilder::SetType(absl::string_view type) {
  type_ = std::string(type);
  return *this;
}

AdvertisementBuilder& AdvertisementBuilder::AddServiceUuid(
    absl::string_view uuid) {
  service_uuids_.emplace_back(uuid);
  return *this;
}

AdvertisementBuilder& AdvertisementBuilder::AddServiceData(
    absl::string_view uuid, const ByteArray& data) {
  service_data_.emplace_back(std::string(uuid), data);
  return *this;
}

AdvertisementBuilder& AdvertisementBuilder::SetConnectable(bool connectable) {
  connectable_ = connectable;
  return *this;
}

absl::StatusOr<std::unique_ptr<AdvertisementHandle>>
AdvertisementBuilder::Register() {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) {
    return absl::UnavailableError("System D-Bus not available");
  }

  GError* error = nullptr;
  GDBusNodeInfo* node_info =
      g_dbus_node_info_new_for_xml(kAdvIntrospectionXml, &error);
  if (!node_info) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    return absl::InternalError(
        absl::StrCat("Failed to parse advertisement introspection: ", msg));
  }

  std::string path = NextObjectPath("/org/quickshare/adv");

  auto* ctx = new AdvContext{
      .type = type_,
      .service_uuids = service_uuids_,
      .service_data = service_data_,
  };

  uint32_t reg_id = g_dbus_connection_register_object(
      bus, path.c_str(), node_info->interfaces[0], &kAdvVtable, ctx,
      [](gpointer data) { delete static_cast<AdvContext*>(data); }, &error);
  g_dbus_node_info_unref(node_info);

  if (!reg_id) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    delete ctx;
    return absl::InternalError(
        absl::StrCat("Failed to register D-Bus object: ", msg));
  }

  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdvManagerIface,
      "RegisterAdvertisement",
      g_variant_new("(oa{sv})", path.c_str(), &options), nullptr,
      G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);

  if (!result) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    g_dbus_connection_unregister_object(bus, reg_id);
    return absl::InternalError(
        absl::StrCat("BlueZ rejected advertisement: ", msg));
  }
  g_variant_unref(result);

  return std::unique_ptr<AdvertisementHandle>(
      new AdvertisementHandle(bus, std::move(path), reg_id));
}

// ---- BLE Scanning ---------------------------------------------------------

struct ScanSession {
  uint32_t signal_id = 0;
  std::string service_uuid;
  ScanCallback callback;
};

static std::mutex g_scan_mutex;
static std::map<uint32_t, std::shared_ptr<ScanSession>> g_active_scans;
static std::atomic<uint32_t> g_scan_counter{0};

absl::StatusOr<uint32_t> StartScan(absl::string_view service_uuid,
                                   ScanCallback callback) {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return absl::UnavailableError("System D-Bus not available");

  std::lock_guard<std::mutex> lock(g_scan_mutex);

  // Configure LE Discovery Filter.
  GVariantBuilder filter;
  g_variant_builder_init(&filter, G_VARIANT_TYPE("a{sv}"));
  GVariantBuilder uuids;
  g_variant_builder_init(&uuids, G_VARIANT_TYPE("as"));
  g_variant_builder_add(&uuids, "s", std::string(service_uuid).c_str());
  g_variant_builder_add(&filter, "{sv}", "UUIDs", g_variant_builder_end(&uuids));
  g_variant_builder_add(&filter, "{sv}", "Transport", g_variant_new_string("le"));

  GError* error = nullptr;
  g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdapterIface,
      "SetDiscoveryFilter", g_variant_new("(a{sv})", &filter),
      nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (error) {
    g_clear_error(&error);
  }

  // Start Discovery.
  g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdapterIface,
      "StartDiscovery", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (error) {
    std::string msg = error->message;
    g_clear_error(&error);
    return absl::InternalError(absl::StrCat("StartDiscovery failed: ", msg));
  }

  uint32_t sub_id = g_scan_counter.fetch_add(1) + 1;
  auto session = std::make_unique<ScanSession>();
  session->service_uuid = std::string(service_uuid);
  session->callback = std::move(callback);

  // Listen to InterfacesAdded signals on ObjectManager.
  session->signal_id = g_dbus_connection_signal_subscribe(
      bus, kBluezBusName, "org.freedesktop.DBus.ObjectManager",
      "InterfacesAdded", "/", nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
      [](GDBusConnection*, const char*, const char*, const char*, const char*,
         GVariant* parameters, gpointer user_data) {
        auto* s = static_cast<ScanSession*>(user_data);
        const char* path = nullptr;
        GVariant* interfaces = nullptr;
        g_variant_get(parameters, "(&o@a{sa{sv}})", &path, &interfaces);

        GVariant* dev_props = nullptr;
        if (g_variant_lookup(interfaces, kDeviceIface, "@a{sv}", &dev_props)) {
          ScanResult result;
          result.device_path = path;

          const char* address = nullptr;
          if (g_variant_lookup(dev_props, "Address", "&s", &address)) {
            result.address = address;
          }

          GVariant* sd = nullptr;
          if (g_variant_lookup(dev_props, "ServiceData", "@a{sv}", &sd)) {
            GVariantIter iter;
            g_variant_iter_init(&iter, sd);
            const char* uuid = nullptr;
            GVariant* val = nullptr;
            while (g_variant_iter_next(&iter, "{&sv}", &uuid, &val)) {
              GVariant* bytes = nullptr;
              g_variant_get(val, "v", &bytes);
              gsize length = 0;
              const guchar* data = static_cast<const guchar*>(g_variant_get_fixed_array(bytes, &length, 1));
              if (data && length > 0) {
                ByteArray byte_array(reinterpret_cast<const char*>(data), length);
                result.service_data.emplace_back(uuid, std::move(byte_array));
              }
              g_variant_unref(bytes);
              g_variant_unref(val);
            }
            g_variant_unref(sd);
          }

          if (!result.service_data.empty()) {
            s->callback(result);
          }
          g_variant_unref(dev_props);
        }
      },
      session.get(), nullptr);

  g_active_scans[sub_id] = std::move(session);
  return sub_id;
}

void StopScan(uint32_t subscription_id) {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return;

  std::lock_guard<std::mutex> lock(g_scan_mutex);
  auto it = g_active_scans.find(subscription_id);
  if (it == g_active_scans.end()) return;

  if (it->second->signal_id) {
    g_dbus_connection_signal_unsubscribe(bus, it->second->signal_id);
  }

  GError* error = nullptr;
  g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kAdapterIface,
      "StopDiscovery", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  if (error) {
    g_clear_error(&error);
  }

  g_active_scans.erase(it);
}

// ---- GATT Server ----------------------------------------------------------

static const char kGattApplicationIntrospectionXml[] =
    "<node>"
    "  <interface name='org.freedesktop.DBus.ObjectManager'>"
    "    <method name='GetManagedObjects'>"
    "      <arg name='objects' type='a{oa{sa{sv}}}' direction='out'/>"
    "    </method>"
    "    <signal name='InterfacesAdded'>"
    "      <arg name='object' type='o'/>"
    "      <arg name='interfaces' type='a{sa{sv}}'/>"
    "    </signal>"
    "    <signal name='InterfacesRemoved'>"
    "      <arg name='object' type='o'/>"
    "      <arg name='interfaces' type='as'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

struct GattServerHandle::Impl {
  GDBusConnection* bus = nullptr;
  std::string app_path;
  std::string service_path;
  GattServiceDef service;
  std::vector<uint32_t> registration_ids;
  GattServerCallbacks callbacks;
  std::vector<ByteArray> char_values;
};

struct GattObjectContext {
  enum class Kind { kApplication, kService, kCharacteristic };

  Kind kind;
  size_t char_index = 0;
  GattServerHandle::Impl* impl = nullptr;
};

static void AddStringArray(GVariantBuilder* builder,
                           const std::vector<std::string>& values) {
  for (const auto& value : values) {
    g_variant_builder_add(builder, "s", value.c_str());
  }
}

static std::vector<std::string> GattFlags(uint32_t flags) {
  std::vector<std::string> result;
  if (flags & GattCharacteristicDef::kRead) result.emplace_back("read");
  if (flags & GattCharacteristicDef::kWrite) result.emplace_back("write");
  if (flags & GattCharacteristicDef::kNotify) result.emplace_back("notify");
  if (flags & GattCharacteristicDef::kIndicate) result.emplace_back("indicate");
  return result;
}

static GVariant* BuildByteArray(const ByteArray& value) {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
  for (size_t i = 0; i < value.size(); ++i) {
    g_variant_builder_add(&builder, "y",
                          static_cast<guchar>(value.data()[i]));
  }
  return g_variant_builder_end(&builder);
}

static GVariant* ServiceProperty(const GattServerHandle::Impl* impl,
                                 const char* name) {
  if (g_strcmp0(name, "UUID") == 0) {
    return g_variant_new_string(impl->service.uuid.c_str());
  }
  if (g_strcmp0(name, "Primary") == 0) {
    return g_variant_new_boolean(impl->service.primary);
  }
  return nullptr;
}

static GVariant* CharacteristicProperty(const GattServerHandle::Impl* impl,
                                        size_t char_index,
                                        const char* name) {
  if (char_index >= impl->service.characteristics.size()) return nullptr;
  const GattCharacteristicDef& characteristic =
      impl->service.characteristics[char_index];
  if (g_strcmp0(name, "UUID") == 0) {
    return g_variant_new_string(characteristic.uuid.c_str());
  }
  if (g_strcmp0(name, "Service") == 0) {
    return g_variant_new_object_path(impl->service_path.c_str());
  }
  if (g_strcmp0(name, "Value") == 0) {
    return BuildByteArray(impl->char_values[char_index]);
  }
  if (g_strcmp0(name, "Flags") == 0) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    AddStringArray(&builder, GattFlags(characteristic.flags));
    return g_variant_builder_end(&builder);
  }
  return nullptr;
}

static void AddServiceObject(GVariantBuilder* objects,
                             const GattServerHandle::Impl* impl) {
  GVariantBuilder interfaces;
  g_variant_builder_init(&interfaces, G_VARIANT_TYPE("a{sa{sv}}"));

  GVariantBuilder props;
  g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&props, "{sv}", "UUID",
                        g_variant_new_string(impl->service.uuid.c_str()));
  g_variant_builder_add(&props, "{sv}", "Primary",
                        g_variant_new_boolean(impl->service.primary));
  g_variant_builder_add(&interfaces, "{sa{sv}}", kGattServiceIface, &props);

  g_variant_builder_add(objects, "{oa{sa{sv}}}", impl->service_path.c_str(),
                        &interfaces);
}

static void AddCharacteristicObject(GVariantBuilder* objects,
                                    const GattServerHandle::Impl* impl,
                                    size_t char_index) {
  const GattCharacteristicDef& characteristic =
      impl->service.characteristics[char_index];
  std::string char_path = absl::StrCat(impl->service_path, "/char", char_index);

  GVariantBuilder flags;
  g_variant_builder_init(&flags, G_VARIANT_TYPE("as"));
  AddStringArray(&flags, GattFlags(characteristic.flags));

  GVariantBuilder props;
  g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&props, "{sv}", "UUID",
                        g_variant_new_string(characteristic.uuid.c_str()));
  g_variant_builder_add(&props, "{sv}", "Service",
                        g_variant_new_object_path(impl->service_path.c_str()));
  g_variant_builder_add(&props, "{sv}", "Value",
                        BuildByteArray(impl->char_values[char_index]));
  g_variant_builder_add(&props, "{sv}", "Flags",
                        g_variant_builder_end(&flags));

  GVariantBuilder interfaces;
  g_variant_builder_init(&interfaces, G_VARIANT_TYPE("a{sa{sv}}"));
  g_variant_builder_add(&interfaces, "{sa{sv}}", kGattCharacteristicIface,
                        &props);
  g_variant_builder_add(objects, "{oa{sa{sv}}}", char_path.c_str(),
                        &interfaces);
}

static GVariant* BuildManagedObjects(const GattServerHandle::Impl* impl) {
  GVariantBuilder objects;
  g_variant_builder_init(&objects, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
  AddServiceObject(&objects, impl);
  for (size_t i = 0; i < impl->service.characteristics.size(); ++i) {
    AddCharacteristicObject(&objects, impl, i);
  }
  return g_variant_builder_end(&objects);
}

GattServerHandle::~GattServerHandle() {
  if (!impl_) return;
  if (impl_->bus && !impl_->app_path.empty()) {
    GError* error = nullptr;
    GVariant* result = g_dbus_connection_call_sync(
        impl_->bus, kBluezBusName, kAdapterPath, kGattManagerIface,
        "UnregisterApplication",
        g_variant_new("(o)", impl_->app_path.c_str()), nullptr,
        G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
    if (result) g_variant_unref(result);
    g_clear_error(&error);
  }
  if (impl_->bus) {
    for (auto id : impl_->registration_ids) {
      if (id) g_dbus_connection_unregister_object(impl_->bus, id);
    }
  }
}

GattServerHandle::GattServerHandle(GattServerHandle&&) noexcept = default;
GattServerHandle& GattServerHandle::operator=(GattServerHandle&&) noexcept =
    default;

absl::Status GattServerHandle::UpdateCharacteristic(
    absl::string_view char_uuid, const ByteArray& value) {
  if (!impl_) return absl::FailedPreconditionError("No GATT server");
  for (size_t i = 0; i < impl_->service.characteristics.size(); ++i) {
    if (impl_->service.characteristics[i].uuid == char_uuid) {
      impl_->char_values[i] = value;
      return absl::OkStatus();
    }
  }
  return absl::NotFoundError("Characteristic not found");
}

static void HandleGattApplicationMethodCall(
    GDBusConnection*, const char*, const char*, const char* interface_name,
    const char* method_name, GVariant*, GDBusMethodInvocation* invocation,
    gpointer user_data) {
  auto* ctx = static_cast<GattObjectContext*>(user_data);
  if (g_strcmp0(interface_name, kObjectManagerIface) == 0 &&
      g_strcmp0(method_name, "GetManagedObjects") == 0) {
    g_dbus_method_invocation_return_value(
        invocation, g_variant_new("(@a{oa{sa{sv}}})",
                                  BuildManagedObjects(ctx->impl)));
    return;
  }
  g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                        G_DBUS_ERROR_UNKNOWN_METHOD,
                                        "Unknown method");
}

static void HandleGattMethodCall(
    GDBusConnection*, const char*, const char*, const char* interface_name,
    const char* method_name, GVariant* parameters,
    GDBusMethodInvocation* invocation, gpointer user_data) {
  auto* ctx = static_cast<GattObjectContext*>(user_data);
  GattServerHandle::Impl* impl = ctx->impl;

  if (ctx->kind == GattObjectContext::Kind::kCharacteristic &&
      g_strcmp0(interface_name, kGattCharacteristicIface) == 0) {
    if (ctx->char_index >= impl->service.characteristics.size()) {
      g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                            G_DBUS_ERROR_UNKNOWN_OBJECT,
                                            "Unknown characteristic");
      return;
    }
    const GattCharacteristicDef& characteristic =
        impl->service.characteristics[ctx->char_index];

    if (g_strcmp0(method_name, "ReadValue") == 0) {
      if (impl->callbacks.on_read) {
        GVariant* options = nullptr;
        int offset = 0;
        g_variant_get(parameters, "(@a{sv})", &options);
        if (options) {
          g_variant_lookup(options, "offset", "q", &offset);
          g_variant_unref(options);
        }
        impl->char_values[ctx->char_index] =
            impl->callbacks.on_read(characteristic.uuid, offset);
      }
      g_dbus_method_invocation_return_value(
          invocation, g_variant_new("(@ay)",
                                    BuildByteArray(
                                        impl->char_values[ctx->char_index])));
      return;
    }

    if (g_strcmp0(method_name, "WriteValue") == 0) {
      GVariant* value_var = nullptr;
      GVariant* options = nullptr;
      g_variant_get(parameters, "(@ay@a{sv})", &value_var, &options);
      gsize length = 0;
      const guchar* data = static_cast<const guchar*>(
          g_variant_get_fixed_array(value_var, &length, 1));
      ByteArray new_value(reinterpret_cast<const char*>(data), length);
      impl->char_values[ctx->char_index] = new_value;
      if (impl->callbacks.on_write) {
        impl->callbacks.on_write(characteristic.uuid, new_value);
      }
      if (value_var) g_variant_unref(value_var);
      if (options) g_variant_unref(options);
      g_dbus_method_invocation_return_value(invocation, nullptr);
      return;
    }

    if (g_strcmp0(method_name, "StartNotify") == 0) {
      if (impl->callbacks.on_subscribe) {
        impl->callbacks.on_subscribe(characteristic.uuid);
      }
      g_dbus_method_invocation_return_value(invocation, nullptr);
      return;
    }

    if (g_strcmp0(method_name, "StopNotify") == 0) {
      if (impl->callbacks.on_unsubscribe) {
        impl->callbacks.on_unsubscribe(characteristic.uuid);
      }
      g_dbus_method_invocation_return_value(invocation, nullptr);
      return;
    }
  }

  if (g_strcmp0(interface_name, kPropertiesIface) == 0) {
    const char* requested_iface = nullptr;
    const char* prop_name = nullptr;
    if (g_strcmp0(method_name, "Get") == 0) {
      g_variant_get(parameters, "(&s&s)", &requested_iface, &prop_name);
      GVariant* value = nullptr;
      if (ctx->kind == GattObjectContext::Kind::kService &&
          g_strcmp0(requested_iface, kGattServiceIface) == 0) {
        value = ServiceProperty(impl, prop_name);
      } else if (ctx->kind == GattObjectContext::Kind::kCharacteristic &&
                 g_strcmp0(requested_iface, kGattCharacteristicIface) == 0) {
        value = CharacteristicProperty(impl, ctx->char_index, prop_name);
      }
      if (value) {
        g_dbus_method_invocation_return_value(invocation,
                                              g_variant_new("(v)", value));
      } else {
        g_dbus_method_invocation_return_error(
            invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
            "Unknown property");
      }
      return;
    }

    if (g_strcmp0(method_name, "GetAll") == 0) {
      g_variant_get(parameters, "(&s)", &requested_iface);
      GVariantBuilder props;
      g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));
      if (ctx->kind == GattObjectContext::Kind::kService &&
          g_strcmp0(requested_iface, kGattServiceIface) == 0) {
        g_variant_builder_add(&props, "{sv}", "UUID",
                              ServiceProperty(impl, "UUID"));
        g_variant_builder_add(&props, "{sv}", "Primary",
                              ServiceProperty(impl, "Primary"));
      } else if (ctx->kind == GattObjectContext::Kind::kCharacteristic &&
                 g_strcmp0(requested_iface, kGattCharacteristicIface) == 0) {
        g_variant_builder_add(&props, "{sv}", "UUID",
                              CharacteristicProperty(impl, ctx->char_index,
                                                     "UUID"));
        g_variant_builder_add(&props, "{sv}", "Service",
                              CharacteristicProperty(impl, ctx->char_index,
                                                     "Service"));
        g_variant_builder_add(&props, "{sv}", "Value",
                              CharacteristicProperty(impl, ctx->char_index,
                                                     "Value"));
        g_variant_builder_add(&props, "{sv}", "Flags",
                              CharacteristicProperty(impl, ctx->char_index,
                                                     "Flags"));
      }
      g_dbus_method_invocation_return_value(
          invocation, g_variant_new("(@a{sv})", g_variant_builder_end(&props)));
      return;
    }
  }

  g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                        G_DBUS_ERROR_UNKNOWN_METHOD,
                                        "Unknown method or property");
}

static const GDBusInterfaceVTable kGattApplicationVtable = {
    HandleGattApplicationMethodCall, nullptr, nullptr, {0}};
static const GDBusInterfaceVTable kGattVtable = {
    HandleGattMethodCall, nullptr, nullptr, {0}};

static uint32_t RegisterObjectOrCleanup(
    GDBusConnection* bus, const std::string& path,
    const GDBusInterfaceInfo* interface_info,
    const GDBusInterfaceVTable* vtable, GattObjectContext* context,
    GError** error) {
  uint32_t id = g_dbus_connection_register_object(
      bus, path.c_str(), const_cast<GDBusInterfaceInfo*>(interface_info),
      vtable, context,
      [](gpointer data) { delete static_cast<GattObjectContext*>(data); },
      error);
  if (!id) delete context;
  return id;
}

absl::StatusOr<std::unique_ptr<GattServerHandle>> RegisterGattServer(
    const GattServiceDef& service, GattServerCallbacks callbacks) {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return absl::UnavailableError("System D-Bus not available");
  if (service.characteristics.empty()) {
    return absl::InvalidArgumentError(
        "GATT server requires at least one characteristic");
  }

  GError* error = nullptr;
  GDBusNodeInfo* app_node =
      g_dbus_node_info_new_for_xml(kGattApplicationIntrospectionXml, &error);
  GDBusNodeInfo* service_node =
      g_dbus_node_info_new_for_xml(kGattServiceIntrospectionXml, &error);
  GDBusNodeInfo* char_node =
      g_dbus_node_info_new_for_xml(kGattCharacteristicIntrospectionXml, &error);
  if (!app_node || !service_node || !char_node) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    if (app_node) g_dbus_node_info_unref(app_node);
    if (service_node) g_dbus_node_info_unref(service_node);
    if (char_node) g_dbus_node_info_unref(char_node);
    return absl::InternalError(
        absl::StrCat("Failed to parse GATT introspection: ", msg));
  }

  auto handle = std::unique_ptr<GattServerHandle>(new GattServerHandle());
  handle->impl_ = std::make_unique<GattServerHandle::Impl>();
  handle->impl_->bus = bus;
  handle->impl_->callbacks = std::move(callbacks);
  handle->impl_->service = service;
  handle->impl_->char_values.resize(service.characteristics.size());

  handle->impl_->app_path = NextObjectPath("/org/quickshare/gatt");
  handle->impl_->service_path = absl::StrCat(handle->impl_->app_path, "/service0");

  uint32_t app_reg_id = RegisterObjectOrCleanup(
      bus, handle->impl_->app_path, app_node->interfaces[0],
      &kGattApplicationVtable,
      new GattObjectContext{.kind = GattObjectContext::Kind::kApplication,
                            .impl = handle->impl_.get()},
      &error);
  if (app_reg_id) handle->impl_->registration_ids.push_back(app_reg_id);

  uint32_t service_reg_id = 0;
  if (app_reg_id) {
    service_reg_id = RegisterObjectOrCleanup(
        bus, handle->impl_->service_path, service_node->interfaces[0],
        &kGattVtable,
        new GattObjectContext{.kind = GattObjectContext::Kind::kService,
                              .impl = handle->impl_.get()},
        &error);
    if (service_reg_id) {
      handle->impl_->registration_ids.push_back(service_reg_id);
    }
  }

  if (service_reg_id) {
    for (size_t i = 0; i < service.characteristics.size(); ++i) {
      std::string char_path = absl::StrCat(handle->impl_->service_path, "/char", i);
      uint32_t char_reg_id = RegisterObjectOrCleanup(
          bus, char_path, char_node->interfaces[0], &kGattVtable,
          new GattObjectContext{.kind = GattObjectContext::Kind::kCharacteristic,
                                .char_index = i,
                                .impl = handle->impl_.get()},
          &error);
      if (!char_reg_id) break;
      handle->impl_->registration_ids.push_back(char_reg_id);
    }
  }

  g_dbus_node_info_unref(app_node);
  g_dbus_node_info_unref(service_node);
  g_dbus_node_info_unref(char_node);

  if (handle->impl_->registration_ids.size() !=
      service.characteristics.size() + 2) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    return absl::InternalError(
        absl::StrCat("Failed to export GATT D-Bus objects: ", msg));
  }

  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, kAdapterPath, kGattManagerIface,
      "RegisterApplication",
      g_variant_new("(oa{sv})", handle->impl_->app_path.c_str(), &options),
      nullptr, G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);

  if (!result) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    return absl::InternalError(
        absl::StrCat("BlueZ rejected GATT application: ", msg));
  }
  g_variant_unref(result);

  return handle;
}

// ---- GATT Client ----------------------------------------------------------

struct GattClientConnection::Impl {
  GDBusConnection* bus = nullptr;
  std::string device_path;
};

GattClientConnection::~GattClientConnection() {
  if (impl_) Disconnect();
}

absl::Status GattClientConnection::DiscoverServices(
    absl::string_view service_uuid) {
  // In BlueZ, services are automatically discovered upon connection.
  return absl::OkStatus();
}

static std::string FindCharPath(GDBusConnection* bus, const std::string& device_path, const std::string& char_uuid) {
  GError* error = nullptr;
  GVariant* managed_objects = g_dbus_connection_call_sync(
      bus, kBluezBusName, "/", kObjectManagerIface, "GetManagedObjects",
      nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);

  if (!managed_objects) {
    g_clear_error(&error);
    return "";
  }

  std::string char_path;
  GVariantIter iter;
  const char* path;
  GVariant* interfaces;
  g_variant_get(managed_objects, "(a{oa{sa{sv}}})", &iter);
  while (g_variant_iter_next(&iter, "{&o@a{sa{sv}}}", &path, &interfaces)) {
    if (absl::StartsWith(path, device_path)) {
      GVariant* char_props = g_variant_lookup_value(interfaces, "org.bluez.GattCharacteristic1", nullptr);
      if (char_props) {
        GVariant* uuid_val = g_variant_lookup_value(char_props, "UUID", G_VARIANT_TYPE_STRING);
        if (uuid_val && g_strcmp0(g_variant_get_string(uuid_val, nullptr), char_uuid.c_str()) == 0) {
          char_path = path;
          g_variant_unref(uuid_val);
          g_variant_unref(char_props);
          g_variant_unref(interfaces);
          break;
        }
        if (uuid_val) g_variant_unref(uuid_val);
        g_variant_unref(char_props);
      }
    }
    g_variant_unref(interfaces);
  }
  g_variant_unref(managed_objects);
  return char_path;
}

absl::StatusOr<ByteArray> GattClientConnection::ReadCharacteristic(
    absl::string_view service_uuid, absl::string_view char_uuid) {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return absl::UnavailableError("System D-Bus not available");

  std::string char_path = FindCharPath(bus, impl_->device_path, std::string(char_uuid));
  if (char_path.empty()) return absl::NotFoundError("Characteristic not found");

  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, char_path.c_str(), "org.bluez.GattCharacteristic1",
      "ReadValue", g_variant_new("(a{sv})", &options), G_VARIANT_TYPE("(ay)"),
      G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);

  if (!result) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    return absl::InternalError(absl::StrCat("Failed to read characteristic: ", msg));
  }

  GVariant* bytes_var = nullptr;
  g_variant_get(result, "(@ay)", &bytes_var);
  gsize length = 0;
  const guchar* data = static_cast<const guchar*>(g_variant_get_fixed_array(bytes_var, &length, 1));
  ByteArray value(reinterpret_cast<const char*>(data), length);

  g_variant_unref(bytes_var);
  g_variant_unref(result);
  return value;
}

absl::Status GattClientConnection::WriteCharacteristic(
    absl::string_view service_uuid, absl::string_view char_uuid,
    const ByteArray& value) {
  GDBusConnection* bus = GetSystemBus();
  if (!bus) return absl::UnavailableError("System D-Bus not available");

  std::string char_path = FindCharPath(bus, impl_->device_path, std::string(char_uuid));
  if (char_path.empty()) return absl::NotFoundError("Characteristic not found");

  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
  
  GVariantBuilder bytes_builder;
  g_variant_builder_init(&bytes_builder, G_VARIANT_TYPE("ay"));
  for (size_t i = 0; i < value.size(); ++i) {
    g_variant_builder_add(&bytes_builder, "y", static_cast<guchar>(value.data()[i]));
  }

  GError* error = nullptr;
  GVariant* result = g_dbus_connection_call_sync(
      bus, kBluezBusName, char_path.c_str(), "org.bluez.GattCharacteristic1",
      "WriteValue", g_variant_new("(aya{sv})", &bytes_builder, &options), nullptr,
      G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);

  if (!result) {
    std::string msg = error ? error->message : "unknown";
    g_clear_error(&error);
    return absl::InternalError(absl::StrCat("Failed to write characteristic: ", msg));
  }

  g_variant_unref(result);
  return absl::OkStatus();
}

absl::Status GattClientConnection::StartNotify(
    absl::string_view service_uuid, absl::string_view char_uuid,
    std::function<void(const ByteArray& value)> callback) {
  // Notifying is used for some Nearby flows but might be optional for MVP.
  return absl::OkStatus();
}

void GattClientConnection::Disconnect() {
  if (!impl_ || !impl_->bus) return;
  GError* error = nullptr;
  g_dbus_connection_call_sync(
      impl_->bus, kBluezBusName, impl_->device_path.c_str(),
      kDeviceIface, "Disconnect", nullptr, nullptr,
      G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
  g_clear_error(&error);
}

absl::StatusOr<std::unique_ptr<GattClientConnection>> ConnectGattClient(
    absl::string_view device_path) {
  auto conn = std::make_unique<GattClientConnection>();
  conn->impl_ = std::make_unique<GattClientConnection::Impl>();
  conn->impl_->bus = GetSystemBus();
  conn->impl_->device_path = std::string(device_path);

  return conn;
}

}  // namespace bluez
}  // namespace linux_impl
}  // namespace nearby
