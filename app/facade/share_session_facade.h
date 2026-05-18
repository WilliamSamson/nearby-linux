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

#ifndef QUICK_SHARE_APP_FACADE_SHARE_SESSION_FACADE_H_
#define QUICK_SHARE_APP_FACADE_SHARE_SESSION_FACADE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mirrors sharing/transfer_metadata.h::Status one-to-one. Kept in sync by hand;
// the UI maps these to screens via ui_state.c.
typedef enum {
  QS_STATUS_UNKNOWN = 0,
  QS_STATUS_CONNECTING,
  QS_STATUS_AWAITING_LOCAL_CONFIRMATION,
  QS_STATUS_AWAITING_REMOTE_ACCEPTANCE,
  QS_STATUS_IN_PROGRESS,
  QS_STATUS_COMPLETE,
  QS_STATUS_FAILED,
  QS_STATUS_REJECTED,
  QS_STATUS_CANCELLED,
  QS_STATUS_TIMED_OUT,
  QS_STATUS_MEDIA_UNAVAILABLE,
  QS_STATUS_NOT_ENOUGH_SPACE,
  QS_STATUS_UNSUPPORTED_ATTACHMENT_TYPE,
  QS_STATUS_DEVICE_AUTHENTICATION_FAILED,
  QS_STATUS_INCOMPLETE_PAYLOADS,
} qs_status_t;

typedef enum {
  QS_ATTACHMENT_FILE = 0,
  QS_ATTACHMENT_TEXT,
  QS_ATTACHMENT_WIFI_CREDENTIALS,
} qs_attachment_kind_t;

typedef enum {
  QS_RECEIVE_NO_ONE = 0,
  QS_RECEIVE_EVERYONE_TEMPORARY,
  QS_RECEIVE_EVERYONE_ALWAYS,
  QS_RECEIVE_YOUR_DEVICES,
} qs_receive_policy_t;

typedef enum {
  QS_SETUP_OK = 0,
  QS_SETUP_INVALID_DEVICE_NAME,
  QS_SETUP_INVALID_RECEIVE_POLICY,
  QS_SETUP_PERSISTENCE_ERROR,
} qs_setup_result_t;

typedef struct {
  const char* device_name;
  qs_receive_policy_t receive_policy;
  bool usage_reporting_enabled;
} qs_setup_config_t;

typedef struct {
  bool is_complete;
  const char* device_id;           // owned by facade
  const char* device_name;         // owned by facade
  qs_receive_policy_t receive_policy;
  bool usage_reporting_enabled;
} qs_setup_state_t;

typedef struct {
  bool can_receive;
  bool bluetooth_available;
  bool bluetooth_powered;
  bool avahi_available;
  bool real_backend_available;
  const char* summary;             // owned by facade
  const char* detail;              // owned by facade
} qs_backend_state_t;

typedef struct {
  qs_attachment_kind_t kind;
  const char* display_name;   // owned by session; valid until session ends
  uint64_t size_bytes;        // 0 if unknown
} qs_attachment_t;

// Opaque handles. Lifetime rules:
//  - qs_facade_t outlives every session it produces.
//  - qs_session_t is valid from the first on_session_changed() callback until
//    on_session_ended() returns. The UI must not retain it after that.
typedef struct qs_facade   qs_facade_t;
typedef struct qs_session  qs_session_t;

typedef struct {
  qs_status_t status;
  float progress;                  // 0.0 .. 1.0
  uint64_t transferred_bytes;
  uint64_t transfer_speed_bps;
  uint64_t estimated_seconds_remaining;
} qs_progress_t;

// Threading: callbacks may fire on any thread. UI code is responsible for
// marshalling to the GTK main loop (e.g. g_main_context_invoke). The mock
// facade fires its callbacks on the GLib main context for convenience.
typedef struct {
  void (*on_session_started)(qs_session_t* session, void* user_data);
  void (*on_session_changed)(qs_session_t* session,
                             const qs_progress_t* progress,
                             void* user_data);
  void (*on_session_ended)(qs_session_t* session, void* user_data);
  void* user_data;
} qs_facade_observer_t;

// ---- Facade lifecycle ------------------------------------------------------

qs_facade_t* qs_facade_create_mock(void);
void         qs_facade_destroy(qs_facade_t* facade);

// Register exactly one observer. Replaces any prior observer.
void qs_facade_set_observer(qs_facade_t* facade,
                            const qs_facade_observer_t* observer);

// ---- Device setup ----------------------------------------------------------

// Loads durable setup state. Pointer is owned by the facade and remains valid
// until setup is completed again or the facade is destroyed.
const qs_setup_state_t* qs_facade_get_setup_state(qs_facade_t* facade);

// Validates, normalizes, and persists setup. On success, get_setup_state()
// returns an is_complete=true state with the normalized values.
qs_setup_result_t qs_facade_complete_setup(qs_facade_t* facade,
                                           const qs_setup_config_t* config);

const char* qs_setup_result_message(qs_setup_result_t result);

// Returns current runtime backend readiness for real Quick Share receiving.
// Pointer is owned by the facade and remains valid until the next call.
const qs_backend_state_t* qs_facade_get_backend_state(qs_facade_t* facade);

// Toggle whether this device advertises a receive surface.
void qs_facade_set_visible(qs_facade_t* facade, bool visible);
bool qs_facade_is_visible(const qs_facade_t* facade);

// ---- Session queries -------------------------------------------------------

// Peer device name as advertised. Owned by the session.
const char* qs_session_peer_name(const qs_session_t* session);

// UKey2 verification token (hex) if the protocol requires user confirmation,
// NULL otherwise. Owned by the session.
const char* qs_session_token(const qs_session_t* session);

// Attachments in this session. Pointer is owned by the session and stable
// until on_session_ended fires.
size_t                  qs_session_attachment_count(const qs_session_t* session);
const qs_attachment_t*  qs_session_attachments(const qs_session_t* session);

// Path the file was saved to, valid only after status reaches QS_STATUS_COMPLETE.
// NULL otherwise. Owned by the session.
const char* qs_session_saved_path(const qs_session_t* session);

// Human-readable reason string for terminal failure states. NULL otherwise.
const char* qs_session_failure_reason(const qs_session_t* session);

// ---- Session actions -------------------------------------------------------

// Valid only while status is QS_STATUS_AWAITING_LOCAL_CONFIRMATION.
void qs_session_accept(qs_session_t* session);
void qs_session_reject(qs_session_t* session);

// Valid in any non-final state.
void qs_session_cancel(qs_session_t* session);

// ---- Send Mode & Discovery -------------------------------------------------

typedef void (*qs_device_discovered_cb)(const char* name, const char* ip, int port, void* user_data);

void qs_facade_start_discovery(qs_facade_t* facade, qs_device_discovered_cb cb, void* user_data);
void qs_facade_stop_discovery(qs_facade_t* facade);
void qs_facade_send_file(qs_facade_t* facade, const char* ip, int port, const char* file_path);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // QUICK_SHARE_APP_FACADE_SHARE_SESSION_FACADE_H_
