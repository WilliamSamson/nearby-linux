#include "splash_window.h"

void qs_splash_run(AdwApplication* app, QsSplashDoneCb done,
                   gpointer user_data) {
  if (done) {
    done(app, user_data);
  }
}
