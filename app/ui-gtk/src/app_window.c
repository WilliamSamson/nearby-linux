#include "app_window.h"

#include "ui_state.h"
#include "views.h"

typedef struct {
  QsUiState* ui_state;
  qs_service_t* service;
} WindowData;

static void window_data_free(gpointer data, GObject* obj G_GNUC_UNUSED) {
  WindowData* d = data;
  if (!d) return;
  qs_ui_state_free(d->ui_state);
  qs_service_destroy(d->service);
  g_free(d);
}

GtkWidget* qs_app_window_new(AdwApplication* app, qs_service_t* service) {
  GtkWidget* window = adw_application_window_new(GTK_APPLICATION(app));
  gtk_window_set_title(GTK_WINDOW(window), "Quick Share");
  gtk_window_set_default_size(GTK_WINDOW(window), 880, 720);
  gtk_widget_set_size_request(window, 360, 480);

  // Set Window Icon
  const char* icon_path = "app/ui-gtk/data/app-icon.png";
  if (g_file_test(icon_path, G_FILE_TEST_EXISTS)) {
    GFile* file = g_file_new_for_path(icon_path);
    GIcon* icon = g_file_icon_new(file);
    // Note: GTK4 doesn't have a direct 'set_icon' on window anymore, 
    // it uses the desktop entry icon. But we can try to set it via GtkWindow.
    // However, we'll use it in the UI as well to ensure it's visible.
    g_object_unref(file);
    g_object_unref(icon);
  }

  // Build view stack + 5 children.
  QsViewHome* home = qs_view_home_new();
  QsViewIncoming* incoming = qs_view_incoming_new();
  QsViewTransferring* transferring = qs_view_transferring_new();
  QsViewComplete* complete = qs_view_complete_new();
  QsViewFailed* failed = qs_view_failed_new();
  QsViewOnboarding* onboarding = qs_view_onboarding_new();
  QsViewSettings* settings = qs_view_settings_new();

  AdwViewStack* stack = ADW_VIEW_STACK(adw_view_stack_new());
  adw_view_stack_add_named(stack, qs_view_home_root(home),         "home");
  adw_view_stack_add_named(stack, qs_view_incoming_root(incoming), "incoming");
  adw_view_stack_add_named(stack, qs_view_transferring_root(transferring),
                           "transferring");
  adw_view_stack_add_named(stack, qs_view_complete_root(complete), "complete");
  adw_view_stack_add_named(stack, qs_view_failed_root(failed),     "failed");
  adw_view_stack_add_named(stack, qs_view_onboarding_root(onboarding), "onboarding");
  adw_view_stack_add_named(stack, qs_view_settings_root(settings), "settings");
  adw_view_stack_set_visible_child_name(stack, "onboarding");

  AdwToastOverlay* overlay = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
  adw_toast_overlay_set_child(overlay, GTK_WIDGET(stack));

  // Header bar.
  GtkWidget* header = adw_header_bar_new();

  GtkWidget* toolbar = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), GTK_WIDGET(overlay));

  adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), toolbar);

  // Responsive: AdwClamp already tightens below 400 px. The breakpoint hook
  // lives here so future slices can attach setters (e.g. shrink margins).
  AdwBreakpoint* bp = adw_breakpoint_new(
      adw_breakpoint_condition_parse("max-width: 600sp"));
  adw_application_window_add_breakpoint(ADW_APPLICATION_WINDOW(window), bp);

  // Wire actions.
  WindowData* data = g_new0(WindowData, 1);
  data->service = service;
  data->ui_state = qs_ui_state_new(service, stack, overlay, home, incoming,
                                   transferring, complete, failed, onboarding, settings);

  g_object_weak_ref(G_OBJECT(window), window_data_free, data);
  return window;
}
