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

#ifndef QUICK_SHARE_APP_FACADE_SHARE_SESSION_SERVICE_H_
#define QUICK_SHARE_APP_FACADE_SHARE_SESSION_SERVICE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mirrors sharing/transfer_metadata.h::Status one-to-one.
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
  const char* device_id;           // owned by service
  const char* device_name;         // owned by service
  qs_receive_policy_t receive_policy;
  bool usage_reporting_enabled;
} qs_setup_state_t;

typedef struct {
  bool can_receive;
  bool bluetooth_available;
  bool bluetooth_powered;
  bool avahi_available;
  bool real_backend_available;
  const char* summary;             // owned by service
  const char* detail;              // owned by service
} qs_backend_state_t;

typedef struct {
  qs_attachment_kind_t kind;
  const char* display_name;   // owned by session; valid until session ends
  uint64_t size_bytes;        // 0 if unknown
} qs_attachment_t;

typedef struct qs_service  qs_service_t;
typedef struct qs_session  qs_session_t;

typedef struct {
  qs_status_t status;
  float progress;                  // 0.0 .. 1.0
  uint64_t transferred_bytes;
  uint64_t transfer_speed_bps;
  uint64_t estimated_seconds_remaining;
} qs_progress_t;

typedef struct {
  void (*on_session_started)(qs_session_t* session, void* user_data);
  void (*on_session_changed)(qs_session_t* session,
                             const qs_progress_t* progress,
                             void* user_data);
  void (*on_session_ended)(qs_session_t* session, void* user_data);
  void* user_data;
} qs_service_observer_t;

// ---- Service lifecycle -----------------------------------------------------

qs_service_t* qs_service_create(void);
void          qs_service_destroy(qs_service_t* service);

// Register exactly one observer. Replaces any prior observer.
void qs_service_set_observer(qs_service_t* service,
                             const qs_service_observer_t* observer);

// ---- Device setup ----------------------------------------------------------

const qs_setup_state_t* qs_service_get_setup_state(qs_service_t* service);

qs_setup_result_t qs_service_complete_setup(qs_service_t* service,
                                           const qs_setup_config_t* config);

const char* qs_setup_result_message(qs_setup_result_t result);

const qs_backend_state_t* qs_service_get_backend_state(qs_service_t* service);

// Toggle whether this device advertises a receive surface.
void qs_service_set_visible(qs_service_t* service, bool visible);
bool qs_service_is_visible(const qs_service_t* service);

// Queries/sets save path for received files.
const char* qs_service_get_save_path(const qs_service_t* service);
void qs_service_set_save_path(qs_service_t* service, const char* path);

// Queries/sets whether system notifications are enabled.
bool qs_service_get_notifications_enabled(const qs_service_t* service);
void qs_service_set_notifications_enabled(qs_service_t* service, bool enabled);

// ---- Session queries -------------------------------------------------------

const char* qs_session_peer_name(const qs_session_t* session);
const char* qs_session_token(const qs_session_t* session);
size_t      qs_session_attachment_count(const qs_session_t* session);
const qs_attachment_t* qs_session_attachments(const qs_session_t* session);
const char* qs_session_saved_path(const qs_session_t* session);
const char* qs_session_failure_reason(const qs_session_t* session);

// ---- Session actions -------------------------------------------------------

void qs_session_accept(qs_session_t* session);
void qs_session_reject(qs_session_t* session);
void qs_session_cancel(qs_session_t* session);

// ---- Send Mode & Discovery -------------------------------------------------

typedef void (*qs_device_discovered_cb)(const char* name, const char* ip, int port, void* user_data);

void qs_service_start_discovery(qs_service_t* service, qs_device_discovered_cb cb, void* user_data);
void qs_service_stop_discovery(qs_service_t* service);
void qs_service_send_file(qs_service_t* service, const char* ip, int port, const char* file_path);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // QUICK_SHARE_APP_FACADE_SHARE_SESSION_SERVICE_H_
