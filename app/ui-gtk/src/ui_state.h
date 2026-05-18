#ifndef QUICK_SHARE_APP_UI_GTK_UI_STATE_H_
#define QUICK_SHARE_APP_UI_GTK_UI_STATE_H_

#include <gtk/gtk.h>
#include <adwaita.h>

#include "share_session_facade.h"
#include "views.h"

typedef struct {
  qs_facade_t* facade;            // not owned
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
} QsUiState;

QsUiState* qs_ui_state_new(qs_facade_t* facade,
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
