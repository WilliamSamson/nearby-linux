#ifndef QUICK_SHARE_APP_UI_GTK_APP_WINDOW_H_
#define QUICK_SHARE_APP_UI_GTK_APP_WINDOW_H_

#include <gtk/gtk.h>
#include <adwaita.h>

#include "share_session_service.h"

// Builds the main window for the given application, attaches a mock service,
// and wires the UI state machine. Window owns everything; closing it tears
// the chain down.
GtkWidget* qs_app_window_new(AdwApplication* app, qs_service_t* service);

#endif  // QUICK_SHARE_APP_UI_GTK_APP_WINDOW_H_
