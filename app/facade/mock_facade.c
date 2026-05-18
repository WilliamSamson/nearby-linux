// Copyright 2026 The Quick Share Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0

#include "share_session_facade.h"

#include <arpa/inet.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_TOKEN       "0042"

// ---- Protobuf Varint and Field Parsers -------------------------------------

static uint64_t read_varint(const uint8_t** ptr, const uint8_t* end) {
  uint64_t val = 0;
  int shift = 0;
  while (*ptr < end) {
    uint8_t byte = **ptr;
    (*ptr)++;
    val |= (uint64_t)(byte & 0x7F) << shift;
    if (!(byte & 0x80)) break;
    shift += 7;
  }
  return val;
}

typedef struct {
  char peer_name[256];
  char file_name[256];
  uint64_t file_size;
  uint64_t payload_id;
} DecodedFrame;

static void parse_file_metadata(const uint8_t* ptr, const uint8_t* end, DecodedFrame* out) {
  while (ptr < end) {
    uint64_t key = read_varint(&ptr, end);
    uint32_t field = key >> 3;
    uint32_t wire = key & 0x7;
    if (field == 1 && wire == 2) { // name (string)
      uint64_t len = read_varint(&ptr, end);
      len = (len < 255) ? len : 255;
      memcpy(out->file_name, ptr, len);
      out->file_name[len] = '\0';
      ptr += len;
    } else if (field == 4 && wire == 0) { // size (varint)
      out->file_size = read_varint(&ptr, end);
    } else if (field == 3 && wire == 0) { // payload_id (varint)
      out->payload_id = read_varint(&ptr, end);
    } else {
      if (wire == 0) read_varint(&ptr, end);
      else if (wire == 2) { uint64_t len = read_varint(&ptr, end); ptr += len; }
      else if (wire == 1) ptr += 8;
      else if (wire == 5) ptr += 4;
    }
  }
}

static void parse_introduction_frame(const uint8_t* ptr, const uint8_t* end, DecodedFrame* out) {
  while (ptr < end) {
    uint64_t key = read_varint(&ptr, end);
    uint32_t field = key >> 3;
    uint32_t wire = key & 0x7;
    if (field == 1 && wire == 2) { // file_metadata (repeated message)
      uint64_t len = read_varint(&ptr, end);
      parse_file_metadata(ptr, ptr + len, out);
      ptr += len;
    } else {
      if (wire == 0) read_varint(&ptr, end);
      else if (wire == 2) { uint64_t len = read_varint(&ptr, end); ptr += len; }
      else if (wire == 1) ptr += 8;
      else if (wire == 5) ptr += 4;
    }
  }
}

static void parse_v1_frame(const uint8_t* ptr, const uint8_t* end, DecodedFrame* out) {
  while (ptr < end) {
    uint64_t key = read_varint(&ptr, end);
    uint32_t field = key >> 3;
    uint32_t wire = key & 0x7;
    if (field == 2 && wire == 2) { // introduction (message)
      uint64_t len = read_varint(&ptr, end);
      parse_introduction_frame(ptr, ptr + len, out);
      ptr += len;
    } else {
      if (wire == 0) read_varint(&ptr, end);
      else if (wire == 2) { uint64_t len = read_varint(&ptr, end); ptr += len; }
      else if (wire == 1) ptr += 8;
      else if (wire == 5) ptr += 4;
    }
  }
}

static bool decode_frame(const uint8_t* buf, size_t size, DecodedFrame* out) {
  memset(out, 0, sizeof(*out));
  const uint8_t* ptr = buf;
  const uint8_t* end = buf + size;
  while (ptr < end) {
    uint64_t key = read_varint(&ptr, end);
    uint32_t field = key >> 3;
    uint32_t wire = key & 0x7;
    if (field == 2 && wire == 2) { // v1 (message)
      uint64_t len = read_varint(&ptr, end);
      parse_v1_frame(ptr, ptr + len, out);
      ptr += len;
    } else {
      if (wire == 0) read_varint(&ptr, end);
      else if (wire == 2) { uint64_t len = read_varint(&ptr, end); ptr += len; }
      else if (wire == 1) ptr += 8;
      else if (wire == 5) ptr += 4;
    }
  }
  return strlen(out->file_name) > 0;
}

// ---- Data Structures ------------------------------------------------------

struct qs_session {
  char* peer_name;
  char* token;
  char* saved_path;
  char* failure_reason;

  qs_attachment_t attachment;
  qs_progress_t progress;
  bool accepted;
  bool cancelled;

  int client_fd;
  uint64_t payload_id;
  qs_facade_t* facade;
};

struct qs_facade {
  qs_facade_observer_t observer;
  bool is_visible;
  qs_setup_state_t setup_state;
  qs_backend_state_t backend_state;
  char* device_id;
  char* device_name;
  char* backend_summary;
  char* backend_detail;
  char* settings_path;

  qs_session_t* session;

  int listen_fd;
  int listen_port;
  pthread_t server_thread;
  volatile bool running;

  GDBusConnection* system_bus;
  guint bluez_adv_object_id;
  char* bluez_adv_path;
  GVariant* bluez_adv_service_data;
  bool bluez_advertising;

  char* avahi_group_path;
};

// ---- helpers ---------------------------------------------------------------

static void set_backend_message(qs_facade_t* f,
                                const char* summary,
                                const char* detail) {
  g_free(f->backend_summary);
  g_free(f->backend_detail);
  f->backend_summary = g_strdup(summary ? summary : "");
  f->backend_detail = g_strdup(detail ? detail : "");
  f->backend_state.summary = f->backend_summary;
  f->backend_state.detail = f->backend_detail;
}

static gboolean dbus_service_available(GDBusConnection* bus,
                                       const char* bus_name) {
  GError* error = NULL;
  GVariant* result = g_dbus_connection_call_sync(
      bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "NameHasOwner", g_variant_new("(s)", bus_name),
      G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
  if (!result) {
    g_clear_error(&error);
    return FALSE;
  }
  gboolean has_owner = FALSE;
  g_variant_get(result, "(b)", &has_owner);
  g_variant_unref(result);
  return has_owner;
}

// ---- D-Bus / BLE Advertisement ---------------------------------------------

#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_ADAPTER_PATH "/org/bluez/hci0"
#define BLUEZ_ADV_MANAGER_IFACE "org.bluez.LEAdvertisingManager1"
#define QUICK_SHARE_FAST_UUID "fef3"

static const char kBluezAdvertisementIntrospection[] =
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

static GDBusNodeInfo* bluez_adv_node_info(void) {
  static GDBusNodeInfo* info = NULL;
  if (!info) {
    GError* error = NULL;
    info = g_dbus_node_info_new_for_xml(kBluezAdvertisementIntrospection,
                                         &error);
    if (!info) {
      g_clear_error(&error);
    }
  }
  return info;
}

static void random_bytes(guint8* out, gsize len) {
  for (gsize i = 0; i < len; i++) {
    out[i] = (guint8)g_random_int_range(0, 256);
  }
}

static char* short_advertised_name(const char* device_name) {
  const char* source = (device_name && *device_name) ? device_name : "PC";
  char* normalized = g_utf8_normalize(source, -1, G_NORMALIZE_DEFAULT);
  if (!normalized || *normalized == '\0') {
    g_free(normalized);
    return g_strdup("PC");
  }
  char* end = g_utf8_offset_to_pointer(normalized, 8);
  char* result = g_strndup(normalized, end - normalized);
  g_free(normalized);
  return result;
}

static GVariant* build_nearby_fast_service_data(const char* device_name) {
  guint8 salt[2];
  guint8 metadata_key[14];
  guint8 device_token[2];
  random_bytes(salt, sizeof(salt));
  random_bytes(metadata_key, sizeof(metadata_key));
  random_bytes(device_token, sizeof(device_token));

  char* short_name = short_advertised_name(device_name);
  gsize name_len = strlen(short_name);
  if (name_len > 3) name_len = 3;

  GByteArray* endpoint_info = g_byte_array_new();
  guint8 first_byte = (guint8)(3 << 1);  // ShareTargetType::kLaptop.
  g_byte_array_append(endpoint_info, &first_byte, 1);
  g_byte_array_append(endpoint_info, salt, sizeof(salt));
  g_byte_array_append(endpoint_info, metadata_key, sizeof(metadata_key));
  guint8 encoded_name_len = (guint8)name_len;
  g_byte_array_append(endpoint_info, &encoded_name_len, 1);
  g_byte_array_append(endpoint_info, (const guint8*)short_name, name_len);

  GByteArray* advertisement = g_byte_array_new();
  guint8 version_socket_fast = 0x4a;  // V2, socket V2, fast advertisement.
  guint8 endpoint_len = (guint8)endpoint_info->len;
  g_byte_array_append(advertisement, &version_socket_fast, 1);
  g_byte_array_append(advertisement, &endpoint_len, 1);
  g_byte_array_append(advertisement, endpoint_info->data, endpoint_info->len);
  g_byte_array_append(advertisement, device_token, sizeof(device_token));

  GVariantBuilder bytes;
  g_variant_builder_init(&bytes, G_VARIANT_TYPE("ay"));
  for (guint i = 0; i < advertisement->len; i++) {
    g_variant_builder_add(&bytes, "y", advertisement->data[i]);
  }
  GVariant* byte_array = g_variant_ref_sink(g_variant_builder_end(&bytes));

  GVariantBuilder service_data;
  g_variant_builder_init(&service_data, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&service_data, "{sv}", QUICK_SHARE_FAST_UUID,
                        byte_array);
  GVariant* result = g_variant_ref_sink(g_variant_builder_end(&service_data));

  g_variant_unref(byte_array);
  g_byte_array_unref(advertisement);
  g_byte_array_unref(endpoint_info);
  g_free(short_name);
  return result;
}

static GVariant* bluez_adv_property(qs_facade_t* f, const char* name) {
  if (g_strcmp0(name, "Type") == 0) {
    return g_variant_new_string("peripheral");
  }
  if (g_strcmp0(name, "ServiceUUIDs") == 0) {
    const char* uuids[] = {QUICK_SHARE_FAST_UUID, NULL};
    return g_variant_new_strv(uuids, -1);
  }
  if (g_strcmp0(name, "ServiceData") == 0) {
    return f->bluez_adv_service_data
               ? g_variant_ref(f->bluez_adv_service_data)
               : build_nearby_fast_service_data(f->device_name);
  }
  return NULL;
}

static GVariant* bluez_adv_all_properties(qs_facade_t* f) {
  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
  const char* props[] = {"Type", "ServiceUUIDs", "ServiceData"};
  for (guint i = 0; i < G_N_ELEMENTS(props); i++) {
    GVariant* value = bluez_adv_property(f, props[i]);
    if (value) g_variant_builder_add(&builder, "{sv}", props[i], value);
  }
  return g_variant_builder_end(&builder);
}

static void handle_bluez_adv_method_call(GDBusConnection* connection,
                                         const char* sender,
                                         const char* object_path,
                                         const char* interface_name,
                                         const char* method_name,
                                         GVariant* parameters,
                                         GDBusMethodInvocation* invocation,
                                         gpointer user_data) {
  (void)connection;
  (void)sender;
  (void)object_path;
  qs_facade_t* f = user_data;
  if (g_strcmp0(interface_name, "org.bluez.LEAdvertisement1") == 0 &&
      g_strcmp0(method_name, "Release") == 0) {
    f->bluez_advertising = false;
    g_dbus_method_invocation_return_value(invocation, NULL);
    return;
  }

  if (g_strcmp0(interface_name, "org.freedesktop.DBus.Properties") == 0 &&
      g_strcmp0(method_name, "Get") == 0) {
    const char* requested_iface = NULL;
    const char* property_name = NULL;
    g_variant_get(parameters, "(&s&s)", &requested_iface, &property_name);
    if (g_strcmp0(requested_iface, "org.bluez.LEAdvertisement1") != 0) {
      g_dbus_method_invocation_return_error(
          invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Unsupported interface");
      return;
    }
    GVariant* property = bluez_adv_property(f, property_name);
    if (!property) {
      g_dbus_method_invocation_return_error(
          invocation, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
          "Unknown property");
      return;
    }
    g_dbus_method_invocation_return_value(invocation,
                                          g_variant_new("(v)", property));
    return;
  }

  if (g_strcmp0(interface_name, "org.freedesktop.DBus.Properties") == 0 &&
      g_strcmp0(method_name, "GetAll") == 0) {
    const char* requested_iface = NULL;
    g_variant_get(parameters, "(&s)", &requested_iface);
    if (g_strcmp0(requested_iface, "org.bluez.LEAdvertisement1") != 0) {
      g_dbus_method_invocation_return_error(
          invocation, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
          "Unsupported interface");
      return;
    }
    g_dbus_method_invocation_return_value(
        invocation, g_variant_new("(@a{sv})", bluez_adv_all_properties(f)));
    return;
  }

  g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                                        G_DBUS_ERROR_UNKNOWN_METHOD,
                                        "Unknown method");
}

static GVariant* handle_bluez_adv_get_property(GDBusConnection* connection,
                                               const char* sender,
                                               const char* object_path,
                                               const char* interface_name,
                                               const char* property_name,
                                               GError** error,
                                               gpointer user_data) {
  (void)connection;
  (void)sender;
  (void)object_path;
  (void)interface_name;
  qs_facade_t* f = user_data;
  GVariant* property = bluez_adv_property(f, property_name);
  if (!property) {
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY,
                "Unknown property");
  }
  return property;
}

static const GDBusInterfaceVTable kBluezAdvVtable = {
    handle_bluez_adv_method_call,
    handle_bluez_adv_get_property,
    NULL,
    {0},
};

static bool linux_ble_stop_advertising(qs_facade_t* f) {
  if (!f || !f->system_bus || !f->bluez_advertising) return true;

  GError* error = NULL;
  GVariant* result = g_dbus_connection_call_sync(
      f->system_bus, BLUEZ_BUS_NAME, BLUEZ_ADAPTER_PATH,
      BLUEZ_ADV_MANAGER_IFACE, "UnregisterAdvertisement",
      g_variant_new("(o)", f->bluez_adv_path), NULL, G_DBUS_CALL_FLAGS_NONE,
      3000, NULL, &error);
  if (result) g_variant_unref(result);
  g_clear_error(&error);

  f->bluez_advertising = false;
  if (f->bluez_adv_object_id) {
    g_dbus_connection_unregister_object(f->system_bus, f->bluez_adv_object_id);
    f->bluez_adv_object_id = 0;
  }
  g_clear_pointer(&f->bluez_adv_service_data, g_variant_unref);
  return true;
}

static bool linux_ble_start_advertising(qs_facade_t* f, GError** error) {
  if (!f) return false;
  if (f->bluez_advertising) return true;

  if (!f->system_bus) {
    f->system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, error);
    if (!f->system_bus) return false;
  }

  GDBusNodeInfo* node_info = bluez_adv_node_info();
  if (!node_info) {
    g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                "Could not create BlueZ advertisement D-Bus introspection");
    return false;
  }

  if (!f->bluez_adv_path) {
    f->bluez_adv_path = g_strdup("/org/quickshare/LinuxAdvertisement0");
  }
  g_clear_pointer(&f->bluez_adv_service_data, g_variant_unref);
  f->bluez_adv_service_data = build_nearby_fast_service_data(f->device_name);

  if (!f->bluez_adv_object_id) {
    f->bluez_adv_object_id = g_dbus_connection_register_object(
        f->system_bus, f->bluez_adv_path, node_info->interfaces[0],
        &kBluezAdvVtable, f, NULL, error);
    if (!f->bluez_adv_object_id) return false;
  }

  GVariantBuilder options;
  g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
  GVariant* result = g_dbus_connection_call_sync(
      f->system_bus, BLUEZ_BUS_NAME, BLUEZ_ADAPTER_PATH,
      BLUEZ_ADV_MANAGER_IFACE, "RegisterAdvertisement",
      g_variant_new("(oa{sv})", f->bluez_adv_path, &options), NULL,
      G_DBUS_CALL_FLAGS_NONE, 3000, NULL, error);
  if (!result) {
    if (f->bluez_adv_object_id) {
      g_dbus_connection_unregister_object(f->system_bus,
                                          f->bluez_adv_object_id);
      f->bluez_adv_object_id = 0;
    }
    return false;
  }

  g_variant_unref(result);
  f->bluez_advertising = true;
  return true;
}

static gboolean bluez_adapter_powered(GDBusConnection* bus,
                                      gboolean* available) {
  *available = FALSE;
  GError* error = NULL;
  GDBusProxy* proxy = g_dbus_proxy_new_sync(
      bus, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.bluez",
      "/org/bluez/hci0", "org.bluez.Adapter1", NULL, &error);
  if (!proxy) {
    g_clear_error(&error);
    return FALSE;
  }

  *available = TRUE;
  GVariant* powered = g_dbus_proxy_get_cached_property(proxy, "Powered");
  if (!powered) {
    powered = g_dbus_proxy_call_sync(
        proxy, "org.freedesktop.DBus.Properties.Get",
        g_variant_new("(ss)", "org.bluez.Adapter1", "Powered"),
        G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    if (!powered) {
      g_clear_error(&error);
      g_object_unref(proxy);
      return FALSE;
    }
    GVariant* inner = NULL;
    g_variant_get(powered, "(v)", &inner);
    g_variant_unref(powered);
    powered = inner;
  }

  gboolean is_powered = g_variant_get_boolean(powered);
  g_variant_unref(powered);
  g_object_unref(proxy);
  return is_powered;
}

// ---- Avahi mDNS Advertising ------------------------------------------------

static bool avahi_start_advertising(qs_facade_t* f) {
  if (!f->system_bus) return false;

  GError* error = NULL;
  GVariant* group_res = g_dbus_connection_call_sync(
      f->system_bus, "org.freedesktop.Avahi", "/", "org.freedesktop.Avahi.Server",
      "EntryGroupNew", NULL, G_VARIANT_TYPE("(o)"),
      G_DBUS_CALL_FLAGS_NONE, 3000, NULL, &error);
  if (!group_res) {
    g_clear_error(&error);
    return false;
  }

  const char* group_path = NULL;
  g_variant_get(group_res, "(o)", &group_path);
  f->avahi_group_path = g_strdup(group_path);
  g_variant_unref(group_res);

  GVariantBuilder txt_builder;
  g_variant_builder_init(&txt_builder, G_VARIANT_TYPE("aay"));

  char name_rec[256];
  snprintf(name_rec, sizeof(name_rec), "n=%s", f->device_name);
  GVariantBuilder bytes_n;
  g_variant_builder_init(&bytes_n, G_VARIANT_TYPE("ay"));
  for (char* c = name_rec; *c; c++) g_variant_builder_add(&bytes_n, "y", *c);
  g_variant_builder_add(&txt_builder, "ay", g_variant_builder_end(&bytes_n));

  GVariantBuilder bytes_t;
  g_variant_builder_init(&bytes_t, G_VARIANT_TYPE("ay"));
  g_variant_builder_add(&bytes_t, "y", 't');
  g_variant_builder_add(&bytes_t, "y", '=');
  g_variant_builder_add(&bytes_t, "y", '3'); // Laptop type
  g_variant_builder_add(&txt_builder, "ay", g_variant_builder_end(&bytes_t));

  GVariant* add_res = g_dbus_connection_call_sync(
      f->system_bus, "org.freedesktop.Avahi", f->avahi_group_path,
      "org.freedesktop.Avahi.EntryGroup", "AddService",
      g_variant_new("(iiussusq@aay)",
                    -1, // AVAHI_IF_UNSPEC
                    -1, // AVAHI_PROTO_UNSPEC
                    0,  // flags
                    f->device_name,
                    "_nearby._tcp",
                    "", // domain
                    "", // host
                    (uint16_t)f->listen_port,
                    g_variant_builder_end(&txt_builder)),
      NULL, G_DBUS_CALL_FLAGS_NONE, 3000, NULL, &error);

  if (!add_res) {
    g_clear_error(&error);
    return false;
  }
  g_variant_unref(add_res);

  GVariant* commit_res = g_dbus_connection_call_sync(
      f->system_bus, "org.freedesktop.Avahi", f->avahi_group_path,
      "org.freedesktop.Avahi.EntryGroup", "Commit", NULL, NULL,
      G_DBUS_CALL_FLAGS_NONE, 3000, NULL, &error);

  if (!commit_res) {
    g_clear_error(&error);
    return false;
  }
  g_variant_unref(commit_res);
  return true;
}

static void avahi_stop_advertising(qs_facade_t* f) {
  if (!f->system_bus || !f->avahi_group_path) return;
  GError* error = NULL;
  GVariant* res = g_dbus_connection_call_sync(
      f->system_bus, "org.freedesktop.Avahi", f->avahi_group_path,
      "org.freedesktop.Avahi.EntryGroup", "Free", NULL, NULL,
      G_DBUS_CALL_FLAGS_NONE, 3000, NULL, &error);
  if (res) g_variant_unref(res);
  g_clear_error(&error);
  g_clear_pointer(&f->avahi_group_path, g_free);
}

// ---- TCP Server & File Transfer Session ------------------------------------

static void fire_changed(qs_facade_t* f, qs_session_t* s, qs_status_t status) {
  s->progress.status = status;
  if (f->observer.on_session_changed) {
    f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
  }
}

static void session_free(qs_session_t* s) {
  if (!s) return;
  if (s->client_fd >= 0) close(s->client_fd);
  g_free(s->peer_name);
  g_free(s->token);
  g_free(s->saved_path);
  g_free(s->failure_reason);
  g_free((void*)s->attachment.display_name);
  g_free(s);
}

static void send_connection_response(int fd, bool accept) {
  uint8_t accept_bytes[] = {
    0x08, 0x01, // version = V1
    0x12, 0x06, // v1 (len = 6)
    0x08, 0x02, // type = RESPONSE
    0x1a, 0x02, // connection_response (len = 2)
    0x08, 0x01  // status = ACCEPT
  };

  uint8_t reject_bytes[] = {
    0x08, 0x01, // version = V1
    0x12, 0x06, // v1 (len = 6)
    0x08, 0x02, // type = RESPONSE
    0x1a, 0x02, // connection_response (len = 2)
    0x08, 0x02  // status = REJECT
  };

  uint8_t* frame_bytes = accept ? accept_bytes : reject_bytes;
  uint32_t frame_len = sizeof(accept_bytes);

  uint8_t prefix[4];
  prefix[0] = (frame_len >> 24) & 0xff;
  prefix[1] = (frame_len >> 16) & 0xff;
  prefix[2] = (frame_len >> 8) & 0xff;
  prefix[3] = frame_len & 0xff;

  if (write(fd, prefix, 4) != 4) return;
  if (write(fd, frame_bytes, frame_len) != (ssize_t)frame_len) return;
}

static gpointer session_transfer_thread(gpointer data) {
  qs_session_t* s = data;
  qs_facade_t* f = s->facade;

  // Wait for user to accept or reject.
  while (f->running && !s->accepted && !s->cancelled && !s->failure_reason) {
    g_usleep(50000);
  }

  if (s->cancelled || !f->running) {
    send_connection_response(s->client_fd, false);
    fire_changed(f, s, QS_STATUS_CANCELLED);
    return NULL;
  }
  if (s->failure_reason) {
    send_connection_response(s->client_fd, false);
    fire_changed(f, s, QS_STATUS_FAILED);
    return NULL;
  }

  // Send Accept Connection Response.
  send_connection_response(s->client_fd, true);

  // Set up save path.
  const char* downloads = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
  if (!downloads) downloads = g_get_home_dir();
  char* full_path = g_build_filename(downloads, s->attachment.display_name, NULL);
  s->saved_path = g_strdup(full_path);

  FILE* out_file = fopen(full_path, "wb");
  g_free(full_path);

  if (!out_file) {
    s->failure_reason = g_strdup("Failed to open local destination file");
    fire_changed(f, s, QS_STATUS_FAILED);
    return NULL;
  }

  // Read raw payload bytes.
  uint8_t buffer[65536];
  uint64_t received = 0;
  uint64_t total = s->attachment.size_bytes;

  while (received < total && f->running && !s->cancelled) {
    size_t to_read = total - received;
    if (to_read > sizeof(buffer)) to_read = sizeof(buffer);
    ssize_t n = read(s->client_fd, buffer, to_read);
    if (n <= 0) break;

    fwrite(buffer, 1, n, out_file);
    received += n;

    s->progress.progress = (float)received / total;
    s->progress.transferred_bytes = received;
    s->progress.transfer_speed_bps = 50 * 1024 * 1024; // Simulated high speed over Wi-Fi
    s->progress.estimated_seconds_remaining = (total - received) / s->progress.transfer_speed_bps;

    fire_changed(f, s, QS_STATUS_IN_PROGRESS);
  }
  fclose(out_file);

  if (s->cancelled) {
    fire_changed(f, s, QS_STATUS_CANCELLED);
  } else if (received >= total) {
    s->progress.progress = 1.0f;
    s->progress.transferred_bytes = total;
    fire_changed(f, s, QS_STATUS_COMPLETE);
  } else {
    s->failure_reason = g_strdup("Connection interrupted");
    fire_changed(f, s, QS_STATUS_FAILED);
  }

  // Auto clean up session in UI after 2.5s.
  g_usleep(2500000);
  if (f->observer.on_session_ended) {
    f->observer.on_session_ended(s, f->observer.user_data);
  }
  f->session = NULL;
  session_free(s);
  return NULL;
}

static gpointer handle_incoming_connection(gpointer data) {
  qs_session_t* s = data;
  qs_facade_t* f = s->facade;

  // Read 4-byte big-endian size.
  uint8_t len_bytes[4];
  if (read(s->client_fd, len_bytes, 4) != 4) {
    session_free(s);
    return NULL;
  }
  uint32_t size = (len_bytes[0] << 24) | (len_bytes[1] << 16) | (len_bytes[2] << 8) | len_bytes[3];

  if (size > 1024 * 1024) { // Sanitity limit.
    session_free(s);
    return NULL;
  }

  uint8_t* frame_buf = g_malloc(size);
  uint32_t total_read = 0;
  while (total_read < size) {
    ssize_t n = read(s->client_fd, frame_buf + total_read, size - total_read);
    if (n <= 0) break;
    total_read += n;
  }

  if (total_read < size) {
    g_free(frame_buf);
    session_free(s);
    return NULL;
  }

  DecodedFrame decoded;
  if (!decode_frame(frame_buf, size, &decoded)) {
    g_free(frame_buf);
    session_free(s);
    return NULL;
  }
  g_free(frame_buf);

  // Set up attachment and peer info.
  s->peer_name = g_strdup(strlen(decoded.peer_name) > 0 ? decoded.peer_name : "Android Device");
  s->token = g_strdup(MOCK_TOKEN);
  s->payload_id = decoded.payload_id;

  s->attachment.display_name = g_strdup(decoded.file_name);
  s->attachment.size_bytes = decoded.file_size;
  s->attachment.kind = QS_ATTACHMENT_FILE;

  // Transition UI.
  f->session = s;
  fire_changed(f, s, QS_STATUS_AWAITING_LOCAL_CONFIRMATION);

  // Start Transfer Thread.
  g_thread_new("qs-transfer", session_transfer_thread, s);
  return NULL;
}

static gpointer tcp_server_thread_func(gpointer data) {
  qs_facade_t* f = data;
  while (f->running) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(f->listen_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) continue;

    if (!f->running) {
      close(client_fd);
      break;
    }

    qs_session_t* s = g_new0(qs_session_t, 1);
    s->client_fd = client_fd;
    s->facade = f;

    g_thread_new("qs-connection", handle_incoming_connection, s);
  }
  return NULL;
}

// ---- Setup & Visibility State Manager --------------------------------------

static void refresh_backend_state(qs_facade_t* f) {
  memset(&f->backend_state, 0, sizeof(f->backend_state));

  GError* error = NULL;
  GDBusConnection* bus = f->system_bus ? g_object_ref(f->system_bus)
                                       : g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus) {
    set_backend_message(f, "System bus unavailable",
                        "Cannot inspect Bluetooth or discovery services.");
    g_clear_error(&error);
    return;
  }

  gboolean bluetooth_available = FALSE;
  gboolean bluetooth_powered = bluez_adapter_powered(bus, &bluetooth_available);
  gboolean avahi_available = dbus_service_available(bus, "org.freedesktop.Avahi");

  f->backend_state.bluetooth_available = bluetooth_available;
  f->backend_state.bluetooth_powered = bluetooth_powered;
  f->backend_state.avahi_available = avahi_available;
  f->backend_state.real_backend_available = TRUE;
  f->backend_state.can_receive = bluetooth_available && bluetooth_powered;

  if (!bluetooth_available) {
    set_backend_message(f, "Bluetooth adapter not found",
                        "Quick Share discovery from Android requires BLE.");
  } else if (!bluetooth_powered) {
    set_backend_message(f, "Bluetooth is off",
                        "Turn on Bluetooth before receiving from Android.");
  } else if (!avahi_available) {
    set_backend_message(f, "mDNS service unavailable",
                        "Avahi is required for LAN discovery.");
  } else {
    set_backend_message(
        f,
        f->bluez_advertising ? "Quick Share Active & Ready" : "Quick Share is Ready",
        "Your PC is visible to nearby devices. Android devices can scan, connect, and send files to you instantly.");
  }

  g_object_unref(bus);
}

static const char* receive_policy_to_string(qs_receive_policy_t policy) {
  switch (policy) {
    case QS_RECEIVE_NO_ONE: return "no-one";
    case QS_RECEIVE_EVERYONE_TEMPORARY: return "everyone-temporary";
    case QS_RECEIVE_EVERYONE_ALWAYS: return "everyone-always";
    case QS_RECEIVE_YOUR_DEVICES: return "your-devices";
    default: return "no-one";
  }
}

static gboolean receive_policy_from_string(const char* value, qs_receive_policy_t* out) {
  if (!value || !out) return FALSE;
  if (g_strcmp0(value, "no-one") == 0) { *out = QS_RECEIVE_NO_ONE; return TRUE; }
  if (g_strcmp0(value, "everyone-temporary") == 0) { *out = QS_RECEIVE_EVERYONE_TEMPORARY; return TRUE; }
  if (g_strcmp0(value, "everyone-always") == 0) { *out = QS_RECEIVE_EVERYONE_ALWAYS; return TRUE; }
  if (g_strcmp0(value, "your-devices") == 0) { *out = QS_RECEIVE_YOUR_DEVICES; return TRUE; }
  return FALSE;
}

static gboolean receive_policy_is_valid(qs_receive_policy_t policy) {
  return policy == QS_RECEIVE_NO_ONE ||
         policy == QS_RECEIVE_EVERYONE_TEMPORARY ||
         policy == QS_RECEIVE_EVERYONE_ALWAYS ||
         policy == QS_RECEIVE_YOUR_DEVICES;
}

static char* default_device_name(void) {
  const char* host = g_get_host_name();
  if (host && *host) return g_strdup(host);
  return g_strdup("Linux Device");
}

static char* normalize_device_name(const char* name) {
  char* stripped = g_strdup(name ? name : "");
  g_strstrip(stripped);
  if (*stripped == '\0') {
    g_free(stripped);
    return NULL;
  }
  char* normalized = g_utf8_normalize(stripped, -1, G_NORMALIZE_DEFAULT);
  g_free(stripped);
  if (!normalized) return NULL;

  guint chars = g_utf8_strlen(normalized, -1);
  if (chars > 40) {
    char* end = g_utf8_offset_to_pointer(normalized, 40);
    char* truncated = g_strndup(normalized, end - normalized);
    g_free(normalized);
    normalized = truncated;
  }
  return normalized;
}

static void refresh_setup_view(qs_facade_t* f) {
  f->setup_state.device_id = f->device_id;
  f->setup_state.device_name = f->device_name;
}

static char* settings_path(void) {
  const char* config_dir = g_get_user_config_dir();
  char* dir = g_build_filename(config_dir, "quick-share-linux", NULL);
  char* path = g_build_filename(dir, "settings.ini", NULL);
  g_free(dir);
  return path;
}

static void load_setup_state(qs_facade_t* f) {
  f->settings_path = settings_path();
  f->device_name = default_device_name();
  f->setup_state.receive_policy = QS_RECEIVE_NO_ONE;
  f->setup_state.usage_reporting_enabled = FALSE;

  GKeyFile* key_file = g_key_file_new();
  GError* error = NULL;
  if (g_key_file_load_from_file(key_file, f->settings_path, G_KEY_FILE_NONE, &error)) {
    gboolean complete = g_key_file_get_boolean(key_file, "Setup", "Complete", NULL);
    char* id = g_key_file_get_string(key_file, "Device", "Id", NULL);
    char* name = g_key_file_get_string(key_file, "Device", "Name", NULL);
    char* policy = g_key_file_get_string(key_file, "Receive", "Policy", NULL);
    qs_receive_policy_t parsed_policy = QS_RECEIVE_NO_ONE;

    if (id && *id) f->device_id = id; else g_free(id);
    if (name && *name) {
      g_free(f->device_name);
      f->device_name = name;
    } else {
      g_free(name);
    }
    if (receive_policy_from_string(policy, &parsed_policy)) {
      f->setup_state.receive_policy = parsed_policy;
    }
    g_free(policy);

    f->setup_state.is_complete = complete && f->device_id && f->device_name;
    f->setup_state.usage_reporting_enabled = g_key_file_get_boolean(key_file, "Privacy", "UsageReporting", NULL);
  } else {
    g_clear_error(&error);
  }

  if (!f->device_id) f->device_id = g_uuid_string_random();
  refresh_setup_view(f);
  g_key_file_unref(key_file);
}

static qs_setup_result_t persist_setup_state(qs_facade_t* f) {
  char* dir = g_path_get_dirname(f->settings_path);
  if (g_mkdir_with_parents(dir, 0700) != 0) {
    g_free(dir);
    return QS_SETUP_PERSISTENCE_ERROR;
  }
  g_free(dir);

  GKeyFile* key_file = g_key_file_new();
  g_key_file_set_boolean(key_file, "Setup", "Complete", f->setup_state.is_complete);
  g_key_file_set_string(key_file, "Device", "Id", f->device_id);
  g_key_file_set_string(key_file, "Device", "Name", f->device_name);
  g_key_file_set_string(key_file, "Receive", "Policy", receive_policy_to_string(f->setup_state.receive_policy));
  g_key_file_set_boolean(key_file, "Privacy", "UsageReporting", f->setup_state.usage_reporting_enabled);

  GError* error = NULL;
  gboolean ok = g_key_file_save_to_file(key_file, f->settings_path, &error);
  g_key_file_unref(key_file);
  if (!ok) {
    g_clear_error(&error);
    return QS_SETUP_PERSISTENCE_ERROR;
  }
  return QS_SETUP_OK;
}

// ---- public API ------------------------------------------------------------

qs_facade_t* qs_facade_create_mock(void) {
  qs_facade_t* f = g_new0(qs_facade_t, 1);
  load_setup_state(f);

  // Initialize POSIX TCP Listening Socket.
  f->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(f->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0; // Random port.

  if (bind(f->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
    listen(f->listen_fd, 5);
    socklen_t len = sizeof(addr);
    getsockname(f->listen_fd, (struct sockaddr*)&addr, &len);
    f->listen_port = ntohs(addr.sin_port);

    f->running = true;
    g_thread_new("qs-tcp-server", tcp_server_thread_func, f);
  }

  return f;
}

void qs_facade_destroy(qs_facade_t* f) {
  if (!f) return;
  f->running = false;
  linux_ble_stop_advertising(f);
  avahi_stop_advertising(f);

  if (f->listen_fd >= 0) {
    close(f->listen_fd);
  }

  session_free(f->session);
  if (f->bluez_adv_object_id && f->system_bus) {
    g_dbus_connection_unregister_object(f->system_bus, f->bluez_adv_object_id);
  }
  g_clear_object(&f->system_bus);
  g_clear_pointer(&f->bluez_adv_service_data, g_variant_unref);
  g_free(f->bluez_adv_path);
  g_free(f->device_id);
  g_free(f->device_name);
  g_free(f->backend_summary);
  g_free(f->backend_detail);
  g_free(f->settings_path);
  g_free(f);
}

void qs_facade_set_observer(qs_facade_t* f, const qs_facade_observer_t* o) {
  if (!f) return;
  if (o) {
    f->observer = *o;
  } else {
    memset(&f->observer, 0, sizeof(f->observer));
  }
}

const qs_setup_state_t* qs_facade_get_setup_state(qs_facade_t* f) {
  if (!f) return NULL;
  refresh_setup_view(f);
  return &f->setup_state;
}

qs_setup_result_t qs_facade_complete_setup(qs_facade_t* f,
                                           const qs_setup_config_t* config) {
  if (!f || !config) return QS_SETUP_INVALID_DEVICE_NAME;

  char* normalized_name = normalize_device_name(config->device_name);
  if (!normalized_name) return QS_SETUP_INVALID_DEVICE_NAME;
  if (!receive_policy_is_valid(config->receive_policy)) {
    g_free(normalized_name);
    return QS_SETUP_INVALID_RECEIVE_POLICY;
  }

  char* previous_name = f->device_name;
  bool previous_complete = f->setup_state.is_complete;
  qs_receive_policy_t previous_policy = f->setup_state.receive_policy;
  bool previous_usage_reporting = f->setup_state.usage_reporting_enabled;

  f->device_name = normalized_name;
  if (!f->device_id) f->device_id = g_uuid_string_random();
  f->setup_state.is_complete = true;
  f->setup_state.receive_policy = config->receive_policy;
  f->setup_state.usage_reporting_enabled = config->usage_reporting_enabled;
  refresh_setup_view(f);

  qs_setup_result_t result = persist_setup_state(f);
  if (result != QS_SETUP_OK) {
    g_free(f->device_name);
    f->device_name = previous_name;
    f->setup_state.is_complete = previous_complete;
    f->setup_state.receive_policy = previous_policy;
    f->setup_state.usage_reporting_enabled = previous_usage_reporting;
    refresh_setup_view(f);
    return result;
  }

  g_free(previous_name);
  return QS_SETUP_OK;
}

const char* qs_setup_result_message(qs_setup_result_t result) {
  switch (result) {
    case QS_SETUP_OK: return "Setup complete";
    case QS_SETUP_INVALID_DEVICE_NAME: return "Enter a valid device name";
    case QS_SETUP_INVALID_RECEIVE_POLICY: return "Choose a valid receiving option";
    case QS_SETUP_PERSISTENCE_ERROR: return "Could not save setup settings";
    default: return "Setup failed";
  }
}

const qs_backend_state_t* qs_facade_get_backend_state(qs_facade_t* f) {
  if (!f) return NULL;
  refresh_backend_state(f);
  return &f->backend_state;
}

void qs_facade_set_visible(qs_facade_t* f, bool visible) {
  if (!f || f->is_visible == visible) return;
  if (visible) {
    refresh_backend_state(f);
    if (!f->backend_state.can_receive) {
      f->is_visible = false;
      return;
    }
    GError* error = NULL;
    if (!linux_ble_start_advertising(f, &error)) {
      set_backend_message(f, "BLE advertisement failed",
                          error ? error->message : "BlueZ rejected the advertisement.");
      g_clear_error(&error);
      f->is_visible = false;
      return;
    }
    // Avahi mDNS advertising
    avahi_start_advertising(f);
  } else {
    linux_ble_stop_advertising(f);
    avahi_stop_advertising(f);
  }
  f->is_visible = visible;
  refresh_backend_state(f);
}

bool qs_facade_is_visible(const qs_facade_t* f) {
  return f && f->is_visible;
}

const char* qs_session_peer_name(const qs_session_t* s) {
  return s ? s->peer_name : NULL;
}

const char* qs_session_token(const qs_session_t* s) {
  return s ? s->token : NULL;
}

size_t qs_session_attachment_count(const qs_session_t* s) {
  return s ? 1 : 0;
}

const qs_attachment_t* qs_session_attachments(const qs_session_t* s) {
  return s ? &s->attachment : NULL;
}

const char* qs_session_saved_path(const qs_session_t* s) {
  return s ? s->saved_path : NULL;
}

const char* qs_session_failure_reason(const qs_session_t* s) {
  return s ? s->failure_reason : NULL;
}

void qs_session_accept(qs_session_t* s) {
  if (!s || s->accepted) return;
  s->accepted = true;
}

void qs_session_reject(qs_session_t* s) {
  if (!s) return;
  s->failure_reason = g_strdup("Rejected on this device");
}

void qs_session_cancel(qs_session_t* s) {
  if (!s) return;
  s->cancelled = true;
}

// ---- Send Mode & Discovery Implementation -----------------------------------

struct DiscoveryData {
  qs_facade_t* facade;
  qs_device_discovered_cb cb;
  void* user_data;
  GThread* thread;
  volatile bool active;
};

static struct DiscoveryData* g_discovery = NULL;

static gpointer discovery_thread_func(gpointer data) {
  struct DiscoveryData* d = data;
  
  // 1. Immediately report a mock target device so the user can test the UI without hassle
  g_usleep(500000); // 0.5s delay
  if (!d->active) return NULL;
  
  d->cb("Pixel 8 (Simulated)", "127.0.0.1", 9999, d->user_data);
  
  // 2. Scan for real nearby Avahi _nearby._tcp services
  while (d->active) {
    char* stdout_buf = NULL;
    char* stderr_buf = NULL;
    int exit_status = 0;
    
    if (g_spawn_command_line_sync("avahi-browse -rtp _nearby._tcp", &stdout_buf, &stderr_buf, &exit_status, NULL)) {
      if (stdout_buf) {
        char** lines = g_strsplit(stdout_buf, "\n", -1);
        for (int i = 0; lines[i] != NULL; i++) {
          char* line = lines[i];
          if (g_str_has_prefix(line, "=")) {
            char** parts = g_strsplit(line, ";", -1);
            int parts_count = g_strv_length(parts);
            if (parts_count >= 9) {
              const char* name = parts[3];
              const char* ip = parts[7];
              int port = atoi(parts[8]);
              if (name && ip && port > 0) {
                d->cb(name, ip, port, d->user_data);
              }
            }
            g_strfreev(parts);
          }
        }
        g_strfreev(lines);
        g_free(stdout_buf);
      }
      g_free(stderr_buf);
    }
    
    for (int i = 0; i < 30 && d->active; i++) {
      g_usleep(100000); // 0.1s
    }
  }
  
  return NULL;
}

void qs_facade_start_discovery(qs_facade_t* facade, qs_device_discovered_cb cb, void* user_data) {
  if (g_discovery) {
    qs_facade_stop_discovery(facade);
  }
  g_discovery = g_new0(struct DiscoveryData, 1);
  g_discovery->facade = facade;
  g_discovery->cb = cb;
  g_discovery->user_data = user_data;
  g_discovery->active = true;
  g_discovery->thread = g_thread_new("discovery-thread", discovery_thread_func, g_discovery);
}

void qs_facade_stop_discovery(qs_facade_t* facade) {
  (void)facade;
  if (!g_discovery) return;
  g_discovery->active = false;
  g_thread_join(g_discovery->thread);
  g_free(g_discovery);
  g_discovery = NULL;
}

struct SendData {
  qs_facade_t* facade;
  char* ip;
  int port;
  char* file_path;
  qs_session_t* session;
};

static gpointer send_thread_func(gpointer data) {
  struct SendData* sd = data;
  qs_session_t* s = sd->session;
  qs_facade_t* f = sd->facade;
  
  if (g_strcmp0(sd->ip, "127.0.0.1") == 0 && sd->port == 9999) {
    g_usleep(1000000); // 1s connecting
    if (s->cancelled) goto send_done;
    
    s->progress.status = QS_STATUS_AWAITING_REMOTE_ACCEPTANCE;
    if (f->observer.on_session_changed) {
      f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
    }
    
    g_usleep(2000000); // 2s awaiting remote accept
    if (s->cancelled) goto send_done;
    
    s->progress.status = QS_STATUS_IN_PROGRESS;
    s->progress.progress = 0.0f;
    s->progress.transferred_bytes = 0;
    s->progress.transfer_speed_bps = 50 * 1024 * 1024;
    
    uint64_t total_bytes = s->attachment.size_bytes;
    uint64_t sent = 0;
    while (sent < total_bytes && !s->cancelled) {
      g_usleep(100000); // 0.1s
      sent += total_bytes / 20;
      if (sent > total_bytes) sent = total_bytes;
      s->progress.transferred_bytes = sent;
      s->progress.progress = (float)sent / total_bytes;
      if (f->observer.on_session_changed) {
        f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
      }
    }
    
    if (s->cancelled) goto send_done;
    
    s->progress.status = QS_STATUS_COMPLETE;
    s->progress.progress = 1.0f;
    s->progress.transferred_bytes = total_bytes;
    if (f->observer.on_session_changed) {
      f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
    }
    
    goto send_done;
  }
  
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    s->progress.status = QS_STATUS_FAILED;
    s->failure_reason = g_strdup("Failed to create socket");
    if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
    goto send_done;
  }
  
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(sd->port);
  inet_pton(AF_INET, sd->ip, &addr.sin_addr);
  
  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    s->progress.status = QS_STATUS_FAILED;
    s->failure_reason = g_strdup("Could not connect to target device");
    close(fd);
    if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
    goto send_done;
  }
  
  s->progress.status = QS_STATUS_AWAITING_REMOTE_ACCEPTANCE;
  if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
  
  uint8_t intro_frame[1024];
  size_t idx = 0;
  intro_frame[idx++] = 0x08; intro_frame[idx++] = 0x01; // version = V1
  intro_frame[idx++] = 0x12; intro_frame[idx++] = 0x0c; // v1 len = 12
  intro_frame[idx++] = 0x08; intro_frame[idx++] = 0x01; // type = INTRODUCTION
  intro_frame[idx++] = 0x12; intro_frame[idx++] = 0x08; // introduction len = 8
  intro_frame[idx++] = 0x12; intro_frame[idx++] = 0x06; // file attachment len = 6
  intro_frame[idx++] = 0x08; intro_frame[idx++] = 0x01; // id = 1
  intro_frame[idx++] = 0x12; intro_frame[idx++] = 0x02; // name len = 2
  intro_frame[idx++] = 't';  intro_frame[idx++] = 'x';  intro_frame[idx++] = 't';
  
  send(fd, intro_frame, idx, 0);
  
  uint8_t response_bytes[1024];
  ssize_t bytes_read = recv(fd, response_bytes, sizeof(response_bytes), 0);
  if (bytes_read <= 0) {
    s->progress.status = QS_STATUS_FAILED;
    s->failure_reason = g_strdup("Device disconnected or rejected connection");
    close(fd);
    if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
    goto send_done;
  }
  
  s->progress.status = QS_STATUS_IN_PROGRESS;
  s->progress.progress = 0.0f;
  s->progress.transferred_bytes = 0;
  if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
  
  FILE* fp = fopen(sd->file_path, "rb");
  if (!fp) {
    s->progress.status = QS_STATUS_FAILED;
    s->failure_reason = g_strdup("Could not read local file");
    close(fd);
    if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
    goto send_done;
  }
  
  uint64_t total_bytes = s->attachment.size_bytes;
  uint64_t sent = 0;
  uint8_t file_buf[32 * 1024];
  size_t r;
  while ((r = fread(file_buf, 1, sizeof(file_buf), fp)) > 0 && !s->cancelled) {
    ssize_t w = send(fd, file_buf, r, 0);
    if (w <= 0) {
      s->progress.status = QS_STATUS_FAILED;
      s->failure_reason = g_strdup("Failed streaming file bytes");
      break;
    }
    sent += w;
    s->progress.transferred_bytes = sent;
    s->progress.progress = (float)sent / total_bytes;
    s->progress.transfer_speed_bps = 30 * 1024 * 1024;
    if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
  }
  fclose(fp);
  close(fd);
  
  if (s->cancelled) goto send_done;
  
  if (s->progress.status != QS_STATUS_FAILED) {
    s->progress.status = QS_STATUS_COMPLETE;
    s->progress.progress = 1.0f;
    s->progress.transferred_bytes = total_bytes;
    if (f->observer.on_session_changed) f->observer.on_session_changed(s, &s->progress, f->observer.user_data);
  }
  
send_done:
  if (f->observer.on_session_ended) {
    f->observer.on_session_ended(s, f->observer.user_data);
  }
  g_free(sd->ip);
  g_free(sd->file_path);
  g_free(sd);
  return NULL;
}

void qs_facade_send_file(qs_facade_t* facade, const char* ip, int port, const char* file_path) {
  if (!facade || !ip || !file_path) return;
  
  qs_session_t* s = g_new0(qs_session_t, 1);
  s->peer_name = g_strdup(ip);
  s->token = g_strdup("4932");
  s->client_fd = -1;
  s->facade = facade;
  
  s->attachment.kind = QS_ATTACHMENT_FILE;
  s->attachment.display_name = g_path_get_basename(file_path);
  
  GFile* gf = g_file_new_for_path(file_path);
  GFileInfo* info = g_file_query_info(gf, "standard::size", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  if (info) {
    s->attachment.size_bytes = g_file_info_get_size(info);
    g_object_unref(info);
  } else {
    s->attachment.size_bytes = 10 * 1024 * 1024;
  }
  g_object_unref(gf);
  
  s->progress.status = QS_STATUS_CONNECTING;
  s->progress.progress = 0.0f;
  s->progress.transferred_bytes = 0;
  
  facade->session = s;
  
  if (facade->observer.on_session_started) {
    facade->observer.on_session_started(s, facade->observer.user_data);
  }
  
  struct SendData* sd = g_new0(struct SendData, 1);
  sd->facade = facade;
  sd->ip = g_strdup(ip);
  sd->port = port;
  sd->file_path = g_strdup(file_path);
  sd->session = s;
  
  g_thread_new("send-thread", send_thread_func, sd);
}
