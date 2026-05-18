#include <gtk/gtk.h>
#include <adwaita.h>

#include "app_window.h"
#include "share_session_facade.h"
#include "splash_window.h"
#include "style.h"

#define APP_ID "dev.quickshare.UbuntuShare"

static void on_splash_done(AdwApplication* app,
                           gpointer user_data G_GNUC_UNUSED) {
  qs_facade_t* facade = qs_facade_create_mock();
  GtkWidget* window = qs_app_window_new(app, facade);
  gtk_window_present(GTK_WINDOW(window));
}

static void on_activate(GApplication* app, gpointer user_data G_GNUC_UNUSED) {
  AdwStyleManager* sm = adw_style_manager_get_default();
  adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_LIGHT);

  qs_app_install_css();
  qs_splash_run(ADW_APPLICATION(app), on_splash_done, NULL);
}

int main(int argc, char* argv[]) {
  g_set_application_name("Quick Share");
  g_set_prgname("dev.quickshare.UbuntuShare");

  AdwApplication* app =
      adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
