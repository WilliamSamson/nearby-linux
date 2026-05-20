#include "ui_state.h"

static gboolean is_failure_status(qs_status_t s) {
  switch (s) {
    case QS_STATUS_FAILED:
    case QS_STATUS_TIMED_OUT:
    case QS_STATUS_MEDIA_UNAVAILABLE:
    case QS_STATUS_NOT_ENOUGH_SPACE:
    case QS_STATUS_UNSUPPORTED_ATTACHMENT_TYPE:
    case QS_STATUS_DEVICE_AUTHENTICATION_FAILED:
    case QS_STATUS_INCOMPLETE_PAYLOADS:
    case QS_STATUS_CANCELLED:
      return TRUE;
    default:
      return FALSE;
  }
}

static void show_page(AdwViewStack* stack, const char* name) {
  adw_view_stack_set_visible_child_name(stack, name);
}

static gboolean setup_allows_receiving(const qs_setup_state_t* setup) {
  return setup && setup->is_complete &&
         setup->receive_policy != QS_RECEIVE_NO_ONE;
}

static gboolean sync_visible_state(QsUiState* s, gboolean requested_visible) {
  qs_service_set_visible(s->service, requested_visible);
  gboolean actual_visible = qs_service_is_visible(s->service);
  qs_view_home_set_visible(s->home, actual_visible);
  qs_view_home_set_backend_state(s->home,
                                 qs_service_get_backend_state(s->service));
  return actual_visible;
}

static void apply_setup_state(QsUiState* s, const qs_setup_state_t* setup) {
  if (!s || !setup) return;
  if (setup->device_name) {
    qs_view_home_set_device_name(s->home, setup->device_name);
  }
  qs_view_home_set_backend_state(
      s->home, qs_service_get_backend_state(s->service));
  qs_view_home_set_notifications_enabled(
      s->home, qs_service_get_notifications_enabled(s->service));
  show_page(s->stack, setup->is_complete ? "home" : "onboarding");
  if (setup->is_complete) {
    sync_visible_state(s, setup_allows_receiving(setup));
  }
}

static void on_session_started(qs_session_t* session, void* user_data) {
  QsUiState* s = user_data;
  s->current_session = session;
  s->current_total_bytes = 0;
  if (qs_session_attachment_count(session) > 0) {
    s->current_total_bytes = qs_session_attachments(session)[0].size_bytes;
  }
  qs_view_incoming_bind_session(s->incoming, session);
  qs_view_incoming_set_awaiting_confirmation(s->incoming, FALSE);
  show_page(s->stack, "incoming");
}

static void on_session_changed(qs_session_t* session,
                               const qs_progress_t* progress, void* user_data) {
  QsUiState* s = user_data;
  switch (progress->status) {
    case QS_STATUS_CONNECTING:
    case QS_STATUS_AWAITING_REMOTE_ACCEPTANCE:
      qs_view_incoming_set_awaiting_confirmation(s->incoming, FALSE);
      show_page(s->stack, "incoming");
      break;
    case QS_STATUS_AWAITING_LOCAL_CONFIRMATION:
      qs_view_incoming_bind_session(s->incoming, session);
      qs_view_incoming_set_awaiting_confirmation(s->incoming, TRUE);
      show_page(s->stack, "incoming");
      break;
    case QS_STATUS_IN_PROGRESS:
      qs_view_transferring_bind_session(s->transferring, session);
      qs_view_transferring_set_progress(s->transferring, progress,
                                        s->current_total_bytes);
      show_page(s->stack, "transferring");
      break;
    case QS_STATUS_COMPLETE:
      qs_view_complete_bind_session(s->complete, session);
      show_page(s->stack, "complete");
      break;
    case QS_STATUS_REJECTED: {
      AdwToast* toast = adw_toast_new("Transfer declined");
      adw_toast_set_timeout(toast, 3);
      adw_toast_overlay_add_toast(s->toast_overlay, toast);
      show_page(s->stack, "home");
      break;
    }
    default:
      if (is_failure_status(progress->status)) {
        qs_view_failed_bind(s->failed, progress->status,
                            qs_session_failure_reason(session));
        show_page(s->stack, "failed");
      }
      break;
  }
}

static void on_session_ended(qs_session_t* session G_GNUC_UNUSED,
                             void* user_data) {
  QsUiState* s = user_data;
  s->current_session = NULL;
}

static void on_home_settings(gpointer user_data) {
  QsUiState* s = user_data;
  
  // Populate settings view with values from the service
  const qs_setup_state_t* setup = qs_service_get_setup_state(s->service);
  if (setup && setup->device_name) {
    qs_view_settings_set_device_name(s->settings, setup->device_name);
  }
  const char* save_path = qs_service_get_save_path(s->service);
  if (save_path) {
    qs_view_settings_set_save_path(s->settings, save_path);
  }
  gboolean notif_enabled = qs_service_get_notifications_enabled(s->service);
  qs_view_settings_set_notifications_enabled(s->settings, notif_enabled);

  show_page(s->stack, "settings");
}

static void on_settings_back(gpointer user_data) {
  QsUiState* s = user_data;

  // Read updated values from settings view
  const char* name = qs_view_settings_get_device_name(s->settings);
  const char* save_path = qs_view_settings_get_save_path(s->settings);
  gboolean notif_enabled = qs_view_settings_get_notifications_enabled(s->settings);

  // Update service
  const qs_setup_state_t* old_setup = qs_service_get_setup_state(s->service);
  qs_setup_config_t config = {
    .device_name = name,
    .receive_policy = old_setup ? old_setup->receive_policy : QS_RECEIVE_NO_ONE,
    .usage_reporting_enabled = old_setup ? old_setup->usage_reporting_enabled : FALSE,
  };
  qs_service_complete_setup(s->service, &config);

  if (save_path) {
    qs_service_set_save_path(s->service, save_path);
  }
  qs_service_set_notifications_enabled(s->service, notif_enabled);

  // Refresh setup and visible state
  apply_setup_state(s, qs_service_get_setup_state(s->service));

  show_page(s->stack, "home");
}

static void on_home_toggle(gboolean visible, gpointer user_data) {
  QsUiState* s = user_data;
  gboolean actual_visible = sync_visible_state(s, visible);
  const qs_backend_state_t* backend = qs_service_get_backend_state(s->service);
  if (visible && !actual_visible) {
    const char* message =
        backend && backend->summary ? backend->summary : "Receiving is not available";
    AdwToast* toast = adw_toast_new(message);
    adw_toast_set_timeout(toast, 4);
    adw_toast_overlay_add_toast(s->toast_overlay, toast);
  }
}

static void on_incoming_accept(gpointer user_data) {
  QsUiState* s = user_data;
  if (s->current_session) qs_session_accept(s->current_session);
}

static void on_incoming_reject(gpointer user_data) {
  QsUiState* s = user_data;
  if (s->current_session) qs_session_reject(s->current_session);
}

static void on_transferring_cancel(gpointer user_data) {
  QsUiState* s = user_data;
  if (s->current_session) qs_session_cancel(s->current_session);
}

static void on_complete_done(gpointer user_data) {
  qs_ui_state_show_home(user_data);
}

static void on_complete_show_in_folder(gpointer user_data) {
  QsUiState* s = user_data;
  AdwToast* toast = adw_toast_new("Open in Files (not wired up yet)");
  adw_toast_set_timeout(toast, 3);
  adw_toast_overlay_add_toast(s->toast_overlay, toast);
}

static void on_failed_retry(gpointer user_data) {
  qs_ui_state_show_home(user_data);
}

static void on_failed_dismiss(gpointer user_data) {
  qs_ui_state_show_home(user_data);
}

static void on_onboarding_done(const qs_setup_config_t* config,
                                gpointer user_data) {
  QsUiState* s = user_data;
  qs_setup_result_t result = qs_service_complete_setup(s->service, config);
  if (result != QS_SETUP_OK) {
    AdwToast* toast = adw_toast_new(qs_setup_result_message(result));
    adw_toast_set_timeout(toast, 4);
    adw_toast_overlay_add_toast(s->toast_overlay, toast);
    return;
  }
  apply_setup_state(s, qs_service_get_setup_state(s->service));
}

struct DiscoveredDevice {
  char* name;
  char* ip;
  int port;
  QsUiState* s;
};

static gboolean add_device_idle(gpointer data) {
  struct DiscoveredDevice* d = data;
  qs_view_home_add_discovered_device(d->s->home, d->name, d->ip, d->port);
  g_free(d->name);
  g_free(d->ip);
  g_free(d);
  return G_SOURCE_REMOVE;
}

static void on_device_discovered(const char* name, const char* ip, int port, void* user_data) {
  QsUiState* s = user_data;
  struct DiscoveredDevice* d = g_new0(struct DiscoveredDevice, 1);
  d->name = g_strdup(name);
  d->ip = g_strdup(ip);
  d->port = port;
  d->s = s;
  g_main_context_invoke(NULL, add_device_idle, d);
}

static void on_home_send_file(const char* file_path, gpointer user_data) {
  QsUiState* s = user_data;
  g_free(s->current_send_file_path);
  s->current_send_file_path = g_strdup(file_path);
  qs_service_start_discovery(s->service, on_device_discovered, s);
}

static void on_home_device_selected(const char* ip, int port, gpointer user_data) {
  QsUiState* s = user_data;
  qs_service_stop_discovery(s->service);
  
  qs_view_home_reset_send_mode(s->home);
  
  qs_service_send_file(s->service, ip, port, s->current_send_file_path);
}

QsUiState* qs_ui_state_new(qs_service_t* service,
                           AdwViewStack* stack,
                           AdwToastOverlay* toast_overlay,
                           QsViewHome* home,
                           QsViewIncoming* incoming,
                           QsViewTransferring* transferring,
                           QsViewComplete* complete,
                           QsViewFailed* failed,
                           QsViewOnboarding* onboarding,
                           QsViewSettings* settings) {
  QsUiState* s = g_new0(QsUiState, 1);
  s->service = service;
  s->stack = stack;
  s->toast_overlay = toast_overlay;
  s->home = home;
  s->incoming = incoming;
  s->transferring = transferring;
  s->complete = complete;
  s->failed = failed;
  s->onboarding = onboarding;
  s->settings = settings;

  qs_view_home_set_on_toggle(home, on_home_toggle, s);
  qs_view_home_set_on_settings(home, on_home_settings, s);
  qs_view_home_set_on_send_file(home, on_home_send_file, s);
  qs_view_home_set_on_device_selected(home, on_home_device_selected, s);
  qs_view_incoming_set_on_accept(incoming, on_incoming_accept, s);
  qs_view_incoming_set_on_reject(incoming, on_incoming_reject, s);
  qs_view_transferring_set_on_cancel(transferring, on_transferring_cancel, s);
  qs_view_complete_set_on_done(complete, on_complete_done, s);
  qs_view_complete_set_on_show_in_folder(complete,
                                         on_complete_show_in_folder, s);
  qs_view_failed_set_on_retry(failed, on_failed_retry, s);
  qs_view_failed_set_on_dismiss(failed, on_failed_dismiss, s);
  qs_view_onboarding_set_on_done(onboarding, on_onboarding_done, s);
  qs_view_settings_set_on_back(settings, on_settings_back, s);

  qs_service_observer_t obs = {
    .on_session_started = on_session_started,
    .on_session_changed = on_session_changed,
    .on_session_ended = on_session_ended,
    .user_data = s,
  };
  qs_service_set_observer(service, &obs);
  apply_setup_state(s, qs_service_get_setup_state(service));
  return s;
}

void qs_ui_state_free(QsUiState* s) {
  if (!s) return;
  qs_service_set_observer(s->service, NULL);
  g_free(s->current_send_file_path);
  g_free(s);
}

void qs_ui_state_show_home(QsUiState* s) {
  show_page(s->stack, "home");
}
