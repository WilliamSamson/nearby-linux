#include "splash_window.h"

#define QS_SPLASH_DURATION_MS 1200

typedef struct {
  AdwApplication* app;
  GtkWindow* window;
  QsSplashDoneCb done;
  gpointer done_data;
  GtkProgressBar* progress;
  double fraction;
} SplashCtx;

static gboolean on_splash_tick(gpointer data) {
  SplashCtx* ctx = data;
  ctx->fraction += 0.02; // Increment progress
  if (ctx->fraction >= 1.0) {
    ctx->fraction = 1.0;
    gtk_progress_bar_set_fraction(ctx->progress, 1.0);
    if (ctx->done) ctx->done(ctx->app, ctx->done_data);
    gtk_window_destroy(ctx->window);
    g_free(ctx);
    return G_SOURCE_REMOVE;
  }
  gtk_progress_bar_set_fraction(ctx->progress, ctx->fraction);
  return G_SOURCE_CONTINUE;
}

void qs_splash_run(AdwApplication* app, QsSplashDoneCb done,
                   gpointer user_data) {
  GtkWidget* window = adw_window_new();
  gtk_window_set_application(GTK_WINDOW(window), GTK_APPLICATION(app));
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  gtk_window_set_default_size(GTK_WINDOW(window), 680, 720);
  gtk_widget_set_size_request(window, 360, 480);
  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  gtk_window_set_title(GTK_WINDOW(window), "Quick Share");

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_add_css_class(box, "qs-splash-container");
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

  // Icon Discovery
  GtkWidget* icon = NULL;
  const char* paths[] = {
    "app/ui-gtk/data/app-icon.png",
    "data/app-icon.png"
  };

  for (size_t i = 0; i < G_N_ELEMENTS(paths); i++) {
    if (g_file_test(paths[i], G_FILE_TEST_EXISTS)) {
      icon = gtk_image_new_from_file(paths[i]);
      break;
    }
  }

  if (!icon) {
    icon = gtk_image_new_from_icon_name("send-to-symbolic");
  }
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 112);
  gtk_widget_add_css_class(icon, "qs-splash-icon");
  gtk_box_append(GTK_BOX(box), icon);

  GtkWidget* title = gtk_label_new("Quick Share");
  gtk_widget_add_css_class(title, "title-1");
  gtk_widget_add_css_class(title, "qs-splash-title");
  gtk_box_append(GTK_BOX(box), title);

  GtkWidget* progress = gtk_progress_bar_new();
  gtk_widget_set_size_request(progress, 240, 6);
  gtk_widget_set_halign(progress, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(progress, "qs-splash-loader");
  gtk_box_append(GTK_BOX(box), progress);

  adw_window_set_content(ADW_WINDOW(window), box);

  SplashCtx* ctx = g_new0(SplashCtx, 1);
  ctx->app = app;
  ctx->window = GTK_WINDOW(window);
  ctx->done = done;
  ctx->done_data = user_data;
  ctx->progress = GTK_PROGRESS_BAR(progress);
  ctx->fraction = 0.0;

  // Animate over ~1.2s (20ms * 50 steps)
  g_timeout_add(20, on_splash_tick, ctx);

  gtk_window_present(GTK_WINDOW(window));
}
