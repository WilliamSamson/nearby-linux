#ifndef QUICK_SHARE_APP_UI_GTK_SPLASH_WINDOW_H_
#define QUICK_SHARE_APP_UI_GTK_SPLASH_WINDOW_H_

#include <adwaita.h>

typedef void (*QsSplashDoneCb)(AdwApplication* app, gpointer user_data);

// Shows the splash window for ~1.2 s, then destroys it and invokes `done`.
// `done` runs on the GLib main context. Callers should construct the main
// window inside `done`.
void qs_splash_run(AdwApplication* app, QsSplashDoneCb done,
                   gpointer user_data);

#endif  // QUICK_SHARE_APP_UI_GTK_SPLASH_WINDOW_H_
