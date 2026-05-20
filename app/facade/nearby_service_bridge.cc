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

#include "app/facade/share_session_service.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <glib.h>

#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_service_factory.h"
#include "sharing/internal/api/linux/sharing_platform_linux.h"
#include "internal/platform/logging.h"
#include "sharing/transfer_metadata.h"
#include "sharing/share_target.h"
#include "sharing/attachment_container.h"
#include "sharing/file_attachment.h"

extern "C" {

struct qs_session {
  qs_service_t* service;
  int64_t target_id;
  std::string device_name;
  std::string token;
  std::vector<std::string> attachment_names;
  std::vector<qs_attachment_t> attachments;
  std::string saved_path;
  qs_progress_t progress;
};

class BridgeTransferUpdateCallback;
class BridgeShareTargetDiscoveredCallback;

struct qs_service {
  std::unique_ptr<nearby::sharing::api::SharingPlatformLinux> platform;
  nearby::sharing::NearbySharingService* service;
  qs_service_observer_t observer;
  qs_setup_state_t setup_state;
  std::string setup_state_device_name;
  std::string setup_state_device_id;
  qs_backend_state_t backend_state;
  std::string backend_state_summary;
  std::string backend_state_detail;
  std::string save_path;
  bool is_visible = false;
  
  std::unordered_map<int64_t, std::unique_ptr<qs_session_t>> sessions;

  std::unique_ptr<BridgeTransferUpdateCallback> receive_callback;
  std::unique_ptr<BridgeTransferUpdateCallback> send_transfer_callback;
  std::unique_ptr<BridgeShareTargetDiscoveredCallback> discovery_callback;
  
  qs_device_discovered_cb discovery_cb = nullptr;
  void* discovery_user_data = nullptr;
};

class BridgeTransferUpdateCallback : public nearby::sharing::TransferUpdateCallback {
 public:
  BridgeTransferUpdateCallback(qs_service_t* service) : service_(service) {}

  void OnTransferUpdate(const nearby::sharing::ShareTarget& share_target,
                        const nearby::sharing::AttachmentContainer& attachment_container,
                        const nearby::sharing::TransferMetadata& transfer_metadata) override;

 private:
  qs_service_t* service_;
};

class BridgeShareTargetDiscoveredCallback : public nearby::sharing::ShareTargetDiscoveredCallback {
 public:
  BridgeShareTargetDiscoveredCallback(qs_service_t* service) : service_(service) {}

  void OnShareTargetDiscovered(const nearby::sharing::ShareTarget& share_target) override;
  void OnShareTargetLost(const nearby::sharing::ShareTarget& share_target) override {}
  void OnShareTargetUpdated(const nearby::sharing::ShareTarget& share_target) override;

 private:
  qs_service_t* service_;
};

static qs_status_t MapStatus(nearby::sharing::TransferMetadata::Status status) {
  switch (status) {
    case nearby::sharing::TransferMetadata::Status::kUnknown:
      return QS_STATUS_UNKNOWN;
    case nearby::sharing::TransferMetadata::Status::kConnecting:
      return QS_STATUS_CONNECTING;
    case nearby::sharing::TransferMetadata::Status::kAwaitingLocalConfirmation:
      return QS_STATUS_AWAITING_LOCAL_CONFIRMATION;
    case nearby::sharing::TransferMetadata::Status::kAwaitingRemoteAcceptance:
      return QS_STATUS_AWAITING_REMOTE_ACCEPTANCE;
    case nearby::sharing::TransferMetadata::Status::kInProgress:
      return QS_STATUS_IN_PROGRESS;
    case nearby::sharing::TransferMetadata::Status::kComplete:
      return QS_STATUS_COMPLETE;
    case nearby::sharing::TransferMetadata::Status::kFailed:
      return QS_STATUS_FAILED;
    case nearby::sharing::TransferMetadata::Status::kRejected:
      return QS_STATUS_REJECTED;
    case nearby::sharing::TransferMetadata::Status::kCancelled:
      return QS_STATUS_CANCELLED;
    case nearby::sharing::TransferMetadata::Status::kTimedOut:
      return QS_STATUS_TIMED_OUT;
    case nearby::sharing::TransferMetadata::Status::kMediaUnavailable:
      return QS_STATUS_MEDIA_UNAVAILABLE;
    case nearby::sharing::TransferMetadata::Status::kNotEnoughSpace:
      return QS_STATUS_NOT_ENOUGH_SPACE;
    case nearby::sharing::TransferMetadata::Status::kUnsupportedAttachmentType:
      return QS_STATUS_UNSUPPORTED_ATTACHMENT_TYPE;
    case nearby::sharing::TransferMetadata::Status::kDeviceAuthenticationFailed:
      return QS_STATUS_DEVICE_AUTHENTICATION_FAILED;
    case nearby::sharing::TransferMetadata::Status::kIncompletePayloads:
      return QS_STATUS_INCOMPLETE_PAYLOADS;
  }
  return QS_STATUS_UNKNOWN;
}

struct SessionUpdateArgs {
  qs_service_t* service;
  int64_t target_id;
  std::string device_name;
  nearby::sharing::TransferMetadata::Status status;
  float progress;
  uint64_t transferred_bytes;
  std::string token;
  std::vector<std::string> file_names;
  std::vector<uint64_t> file_sizes;
  std::string saved_path;
};

void BridgeTransferUpdateCallback::OnTransferUpdate(
    const nearby::sharing::ShareTarget& share_target,
    const nearby::sharing::AttachmentContainer& attachment_container,
    const nearby::sharing::TransferMetadata& transfer_metadata) {
  
  auto* args = new SessionUpdateArgs();
  args->service = service_;
  args->target_id = share_target.id;
  args->device_name = share_target.device_name;
  args->status = transfer_metadata.status();
  args->progress = transfer_metadata.progress() / 100.0f;
  args->transferred_bytes = transfer_metadata.transferred_bytes();
  if (transfer_metadata.token().has_value()) {
    args->token = *transfer_metadata.token();
  }
  for (const auto& file : attachment_container.GetFileAttachments()) {
    args->file_names.push_back(std::string(file.file_name()));
    args->file_sizes.push_back(file.size());
  }
  if (transfer_metadata.status() == nearby::sharing::TransferMetadata::Status::kComplete) {
    if (!attachment_container.GetFileAttachments().empty()) {
      const auto& path_opt = attachment_container.GetFileAttachments()[0].file_path();
      if (path_opt.has_value()) {
        args->saved_path = path_opt->ToString();
      }
    }
  }

  g_main_context_invoke(nullptr, [](gpointer data) -> gboolean {
    auto* args = static_cast<SessionUpdateArgs*>(data);
    auto& sessions = args->service->sessions;
    auto it = sessions.find(args->target_id);
    qs_session_t* session = nullptr;
    bool is_new = false;
    if (it == sessions.end()) {
      session = new qs_session_t();
      session->service = args->service;
      session->target_id = args->target_id;
      sessions[args->target_id] = std::unique_ptr<qs_session_t>(session);
      is_new = true;
    } else {
      session = it->second.get();
    }

    session->device_name = args->device_name;
    session->token = args->token;
    session->saved_path = args->saved_path;
    session->attachment_names = args->file_names;
    session->attachments.clear();
    for (size_t i = 0; i < session->attachment_names.size(); ++i) {
      qs_attachment_t att;
      att.kind = QS_ATTACHMENT_FILE;
      att.display_name = session->attachment_names[i].c_str();
      att.size_bytes = args->file_sizes[i];
      session->attachments.push_back(att);
    }

    session->progress.status = MapStatus(args->status);
    session->progress.progress = args->progress;
    session->progress.transferred_bytes = args->transferred_bytes;
    session->progress.transfer_speed_bps = 0;
    session->progress.estimated_seconds_remaining = 0;

    if (is_new) {
      if (args->service->observer.on_session_started) {
        args->service->observer.on_session_started(session, args->service->observer.user_data);
      }
    }

    if (args->service->observer.on_session_changed) {
      args->service->observer.on_session_changed(session, &session->progress, args->service->observer.user_data);
    }

    if (nearby::sharing::TransferMetadata::IsFinalStatus(args->status)) {
      if (args->service->observer.on_session_ended) {
        args->service->observer.on_session_ended(session, args->service->observer.user_data);
      }
      sessions.erase(args->target_id);
    }

    delete args;
    return G_SOURCE_REMOVE;
  }, args);
}

struct DiscoveredArgs {
  qs_service_t* service;
  std::string device_name;
  std::string id_str;
};

void BridgeShareTargetDiscoveredCallback::OnShareTargetDiscovered(
    const nearby::sharing::ShareTarget& share_target) {
  auto* args = new DiscoveredArgs();
  args->service = service_;
  args->device_name = share_target.device_name;
  args->id_str = std::to_string(share_target.id);

  g_main_context_invoke(nullptr, [](gpointer data) -> gboolean {
    auto* args = static_cast<DiscoveredArgs*>(data);
    if (args->service->discovery_cb) {
      args->service->discovery_cb(args->device_name.c_str(), args->id_str.c_str(), 0, args->service->discovery_user_data);
    }
    delete args;
    return G_SOURCE_REMOVE;
  }, args);
}

void BridgeShareTargetDiscoveredCallback::OnShareTargetUpdated(
    const nearby::sharing::ShareTarget& share_target) {
  OnShareTargetDiscovered(share_target);
}

qs_service_t* qs_service_create(void) {
  auto* service = new qs_service_t();
  service->platform = std::make_unique<nearby::sharing::api::SharingPlatformLinux>();
  
  auto* factory = nearby::sharing::NearbySharingServiceFactory::GetInstance();
  service->service = factory->CreateSharingService(
      *service->platform,
      nullptr, // analytics_recorder
      nullptr, // event_logger
      false    // supports_file_sync
  );

  if (!service->service) {
    LOG(ERROR) << "Failed to create NearbySharingService";
    delete service;
    return nullptr;
  }

  // Initialize callbacks.
  service->receive_callback = std::make_unique<BridgeTransferUpdateCallback>(service);
  service->send_transfer_callback = std::make_unique<BridgeTransferUpdateCallback>(service);
  service->discovery_callback = std::make_unique<BridgeShareTargetDiscoveredCallback>(service);

  // Initialize states.
  service->setup_state.is_complete = true;
  service->save_path = "/tmp";

  return service;
}

void qs_service_destroy(qs_service_t* service) {
  delete service;
}

void qs_service_set_observer(qs_service_t* service,
                             const qs_service_observer_t* observer) {
  if (service) {
    if (observer) {
      service->observer = *observer;
    } else {
      service->observer = {};
    }
  }
}

const qs_setup_state_t* qs_service_get_setup_state(qs_service_t* service) {
  if (!service) return nullptr;
  service->setup_state_device_name = service->service->GetSettings()->GetDeviceName();
  service->setup_state_device_id = "linux-device";
  
  service->setup_state.is_complete = true;
  service->setup_state.device_name = service->setup_state_device_name.c_str();
  service->setup_state.device_id = service->setup_state_device_id.c_str();
  service->setup_state.receive_policy = QS_RECEIVE_EVERYONE_ALWAYS;
  service->setup_state.usage_reporting_enabled = false;
  return &service->setup_state;
}

qs_setup_result_t qs_service_complete_setup(qs_service_t* service,
                                           const qs_setup_config_t* config) {
  if (!service || !config) return QS_SETUP_PERSISTENCE_ERROR;
  service->service->GetSettings()->SetDeviceName(config->device_name, [](nearby::sharing::DeviceNameValidationResult result) {
    LOG(INFO) << "SetDeviceName result: " << (int)result;
  });
  return QS_SETUP_OK;
}

const char* qs_setup_result_message(qs_setup_result_t result) {
  switch (result) {
    case QS_SETUP_OK:
      return "Setup completed successfully";
    case QS_SETUP_INVALID_DEVICE_NAME:
      return "Invalid device name";
    case QS_SETUP_INVALID_RECEIVE_POLICY:
      return "Invalid receive policy";
    case QS_SETUP_PERSISTENCE_ERROR:
      return "Failed to save settings";
  }
  return "Unknown setup error";
}

const qs_backend_state_t* qs_service_get_backend_state(qs_service_t* service) {
  if (!service) return nullptr;
  service->backend_state.can_receive = true;
  service->backend_state.bluetooth_available = service->service->IsBluetoothPresent();
  service->backend_state.bluetooth_powered = service->service->IsBluetoothPowered();
  service->backend_state.avahi_available = true;
  service->backend_state.real_backend_available = true;
  service->backend_state_summary = "Active";
  service->backend_state_detail = "Nearby Sharing Service is initialized and running.";
  service->backend_state.summary = service->backend_state_summary.c_str();
  service->backend_state.detail = service->backend_state_detail.c_str();
  return &service->backend_state;
}

void qs_service_set_visible(qs_service_t* service, bool visible) {
  if (!service) return;
  service->is_visible = visible;
  if (visible) {
    service->service->RegisterReceiveSurface(
        service->receive_callback.get(),
        nearby::sharing::NearbySharingService::ReceiveSurfaceState::kForeground,
        nearby::sharing::Advertisement::BlockedVendorId::kNone,
        [](nearby::sharing::NearbySharingService::StatusCodes status) {
          LOG(INFO) << "RegisterReceiveSurface status: " << (int)status;
        }
    );
  } else {
    service->service->UnregisterReceiveSurface(
        service->receive_callback.get(),
        [](nearby::sharing::NearbySharingService::StatusCodes status) {
          LOG(INFO) << "UnregisterReceiveSurface status: " << (int)status;
        }
    );
  }
}

bool qs_service_is_visible(const qs_service_t* service) {
  return service ? service->is_visible : false;
}

const char* qs_service_get_save_path(const qs_service_t* service) {
  return service ? service->save_path.c_str() : nullptr;
}

void qs_service_set_save_path(qs_service_t* service, const char* path) {
  if (service && path) {
    service->save_path = path;
    service->service->GetSettings()->SetCustomSavePathAsync(path, [](){});
  }
}

bool qs_service_get_notifications_enabled(const qs_service_t* service) {
  return true;
}

void qs_service_set_notifications_enabled(qs_service_t* service, bool enabled) {
}

const char* qs_session_peer_name(const qs_session_t* session) {
  return session ? session->device_name.c_str() : nullptr;
}

const char* qs_session_token(const qs_session_t* session) {
  return (session && !session->token.empty()) ? session->token.c_str() : nullptr;
}

size_t qs_session_attachment_count(const qs_session_t* session) {
  return session ? session->attachments.size() : 0;
}

const qs_attachment_t* qs_session_attachments(const qs_session_t* session) {
  return (session && !session->attachments.empty()) ? session->attachments.data() : nullptr;
}

const char* qs_session_saved_path(const qs_session_t* session) {
  return (session && !session->saved_path.empty()) ? session->saved_path.c_str() : nullptr;
}

const char* qs_session_failure_reason(const qs_session_t* session) {
  return nullptr; 
}

void qs_session_accept(qs_session_t* session) {
  if (!session || !session->service) return;
  session->service->service->Accept(session->target_id, [](nearby::sharing::NearbySharingService::StatusCodes status) {
    LOG(INFO) << "Accept status: " << (int)status;
  });
}

void qs_session_reject(qs_session_t* session) {
  if (!session || !session->service) return;
  session->service->service->Reject(session->target_id, [](nearby::sharing::NearbySharingService::StatusCodes status) {
    LOG(INFO) << "Reject status: " << (int)status;
  });
}

void qs_session_cancel(qs_session_t* session) {
  if (!session || !session->service) return;
  session->service->service->Cancel(session->target_id, [](nearby::sharing::NearbySharingService::StatusCodes status) {
    LOG(INFO) << "Cancel status: " << (int)status;
  });
}

void qs_service_start_discovery(qs_service_t* service, qs_device_discovered_cb cb, void* user_data) {
  if (!service) return;
  service->discovery_cb = cb;
  service->discovery_user_data = user_data;
  service->service->RegisterSendSurface(
      service->send_transfer_callback.get(),
      service->discovery_callback.get(),
      nearby::sharing::NearbySharingService::SendSurfaceState::kForeground,
      nearby::sharing::Advertisement::BlockedVendorId::kNone,
      false,
      [](nearby::sharing::NearbySharingService::StatusCodes status) {
        LOG(INFO) << "RegisterSendSurface status: " << (int)status;
      }
  );
}

void qs_service_stop_discovery(qs_service_t* service) {
  if (!service) return;
  service->discovery_cb = nullptr;
  service->discovery_user_data = nullptr;
  service->service->UnregisterSendSurface(
      service->send_transfer_callback.get(),
      [](nearby::sharing::NearbySharingService::StatusCodes status) {
        LOG(INFO) << "UnregisterSendSurface status: " << (int)status;
      }
  );
}

void qs_service_send_file(qs_service_t* service, const char* ip, int port, const char* file_path) {
  if (!service || !ip) return;
  try {
    int64_t target_id = std::stoll(ip);
    auto container = std::make_unique<nearby::sharing::AttachmentContainer::Builder>();
    container->AddFileAttachment(nearby::sharing::FileAttachment(nearby::FilePath(file_path)));
    
    service->service->SendAttachments(target_id, container->Build(), [](nearby::sharing::NearbySharingService::StatusCodes status) {
      LOG(INFO) << "SendAttachments status: " << (int)status;
    });
  } catch (const std::exception& e) {
    LOG(ERROR) << "Failed to parse target ID from ip: " << ip << " error: " << e.what();
  }
}

} // extern "C"
