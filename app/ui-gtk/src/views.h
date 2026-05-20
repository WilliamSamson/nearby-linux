#ifndef QUICK_SHARE_APP_UI_GTK_VIEWS_H_
#define QUICK_SHARE_APP_UI_GTK_VIEWS_H_

#include <gtk/gtk.h>

#include "share_session_service.h"

// Each view owns its widget tree. The opaque struct is freed when the view is
// destroyed (along with its root widget, via g_object_set_data_full).
//
// All views push state via setters and pull events back via small callbacks.
// They never see the service directly.

// ---- Home -----------------------------------------------------------------

typedef struct QsViewHome QsViewHome;

typedef void (*QsHomeToggleCb)(gboolean visible, gpointer user_data);
typedef void (*QsHomeSettingsCb)(gpointer user_data);

QsViewHome* qs_view_home_new(void);
GtkWidget*  qs_view_home_root(QsViewHome* v);
void        qs_view_home_set_visible(QsViewHome* v, gboolean visible);
void        qs_view_home_set_device_name(QsViewHome* v, const char* name);
void        qs_view_home_set_backend_state(QsViewHome* v,
                                           const qs_backend_state_t* state);
void        qs_view_home_set_on_toggle(QsViewHome* v, QsHomeToggleCb cb,
                                       gpointer user_data);
void        qs_view_home_set_on_settings(QsViewHome* v, QsHomeSettingsCb cb,
                                         gpointer user_data);

typedef void (*QsHomeSendFileCb)(const char* file_path, gpointer user_data);
typedef void (*QsHomeDeviceSelectedCb)(const char* ip, int port, gpointer user_data);

void qs_view_home_set_on_send_file(QsViewHome* v, QsHomeSendFileCb cb, gpointer user_data);
void qs_view_home_set_on_device_selected(QsViewHome* v, QsHomeDeviceSelectedCb cb, gpointer user_data);
void qs_view_home_add_discovered_device(QsViewHome* v, const char* name, const char* ip, int port);
void qs_view_home_reset_send_mode(QsViewHome* v);
void qs_view_home_set_notifications_enabled(QsViewHome* v, gboolean enabled);

// ---- Incoming -------------------------------------------------------------

typedef struct QsViewIncoming QsViewIncoming;

typedef void (*QsIncomingActionCb)(gpointer user_data);

QsViewIncoming* qs_view_incoming_new(void);
GtkWidget*      qs_view_incoming_root(QsViewIncoming* v);
void            qs_view_incoming_bind_session(QsViewIncoming* v,
                                              const qs_session_t* session);
void qs_view_incoming_set_awaiting_confirmation(QsViewIncoming* v,
                                                gboolean awaiting);
void qs_view_incoming_set_on_accept(QsViewIncoming* v,
                                    QsIncomingActionCb cb, gpointer user_data);
void qs_view_incoming_set_on_reject(QsViewIncoming* v,
                                    QsIncomingActionCb cb, gpointer user_data);

// ---- Transferring ---------------------------------------------------------

typedef struct QsViewTransferring QsViewTransferring;

QsViewTransferring* qs_view_transferring_new(void);
GtkWidget*          qs_view_transferring_root(QsViewTransferring* v);
void qs_view_transferring_bind_session(QsViewTransferring* v,
                                       const qs_session_t* session);
void qs_view_transferring_set_progress(QsViewTransferring* v,
                                       const qs_progress_t* progress,
                                       uint64_t total_bytes);
void qs_view_transferring_set_on_cancel(QsViewTransferring* v,
                                        QsIncomingActionCb cb,
                                        gpointer user_data);

// ---- Complete -------------------------------------------------------------

typedef struct QsViewComplete QsViewComplete;

QsViewComplete* qs_view_complete_new(void);
GtkWidget*      qs_view_complete_root(QsViewComplete* v);
void qs_view_complete_bind_session(QsViewComplete* v,
                                   const qs_session_t* session);
void qs_view_complete_set_on_show_in_folder(QsViewComplete* v,
                                            QsIncomingActionCb cb,
                                            gpointer user_data);
void qs_view_complete_set_on_done(QsViewComplete* v,
                                  QsIncomingActionCb cb, gpointer user_data);

// ---- Failed ---------------------------------------------------------------

typedef struct QsViewFailed QsViewFailed;

QsViewFailed* qs_view_failed_new(void);
GtkWidget*    qs_view_failed_root(QsViewFailed* v);
void qs_view_failed_bind(QsViewFailed* v, qs_status_t status,
                         const char* reason);
void qs_view_failed_set_on_retry(QsViewFailed* v, QsIncomingActionCb cb,
                                 gpointer user_data);
void qs_view_failed_set_on_dismiss(QsViewFailed* v, QsIncomingActionCb cb,
                                   gpointer user_data);

// ---- Onboarding -----------------------------------------------------------

typedef struct QsViewOnboarding QsViewOnboarding;

typedef void (*QsOnboardingDoneCb)(const qs_setup_config_t* config,
                                   gpointer user_data);

QsViewOnboarding* qs_view_onboarding_new(void);
GtkWidget*        qs_view_onboarding_root(QsViewOnboarding* v);
void qs_view_onboarding_set_on_done(QsViewOnboarding* v, QsOnboardingDoneCb cb,
                                    gpointer user_data);

// ---- Settings -------------------------------------------------------------

#include "view_settings.h"

#endif  // QUICK_SHARE_APP_UI_GTK_VIEWS_H_
