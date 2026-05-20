#ifndef QUICK_SHARE_APP_UI_GTK_UI_STATE_H_
#define QUICK_SHARE_APP_UI_GTK_UI_STATE_H_

#include <gtk/gtk.h>
#include <adwaita.h>

#include "share_session_service.h"
#include "views.h"

typedef struct {
  qs_service_t* service;            // not owned
  AdwViewStack* stack;            // not owned
  AdwToastOverlay* toast_overlay; // not owned

  QsViewHome* home;
  QsViewIncoming* incoming;
  QsViewTransferring* transferring;
  QsViewComplete* complete;
  QsViewFailed* failed;
  QsViewOnboarding* onboarding;
  QsViewSettings* settings;

  qs_session_t* current_session;  // valid only between started/ended callbacks
  uint64_t current_total_bytes;
  char* current_send_file_path;
} QsUiState;

QsUiState* qs_ui_state_new(qs_service_t* service,
                           AdwViewStack* stack,
                           AdwToastOverlay* toast_overlay,
                           QsViewHome* home,
                           QsViewIncoming* incoming,
                           QsViewTransferring* transferring,
                           QsViewComplete* complete,
                           QsViewFailed* failed,
                           QsViewOnboarding* onboarding,
                           QsViewSettings* settings);

void qs_ui_state_free(QsUiState* s);
void qs_ui_state_show_home(QsUiState* s);

#endif  // QUICK_SHARE_APP_UI_GTK_UI_STATE_H_
