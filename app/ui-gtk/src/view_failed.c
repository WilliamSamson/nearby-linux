#include "views.h"

#include <adwaita.h>

struct QsViewFailed {
  GtkWidget* root;
  GtkLabel* title_label;
  GtkLabel* subtitle_label;
  GtkImage* icon_image;

  QsIncomingActionCb on_retry;
  gpointer           on_retry_data;
  QsIncomingActionCb on_dismiss;
  gpointer           on_dismiss_data;
};

static void free_self(gpointer data) { g_free(data); }

static void on_retry_clicked(GtkButton* b G_GNUC_UNUSED, gpointer data) {
  QsViewFailed* v = data;
  if (v->on_retry) v->on_retry(v->on_retry_data);
}

static void on_dismiss_clicked(GtkButton* b G_GNUC_UNUSED, gpointer data) {
  QsViewFailed* v = data;
  if (v->on_dismiss) v->on_dismiss(v->on_dismiss_data);
}

QsViewFailed* qs_view_failed_new(void) {
  QsViewFailed* v = g_new0(QsViewFailed, 1);

  GtkWidget* clamp = adw_clamp_new();
  adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 540);
  adw_clamp_set_tightening_threshold(ADW_CLAMP(clamp), 400);
  gtk_widget_set_valign(clamp, GTK_ALIGN_CENTER);

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_margin_top(box, 48);
  gtk_widget_set_margin_bottom(box, 48);
  gtk_widget_set_margin_start(box, 32);
  gtk_widget_set_margin_end(box, 32);

  GtkWidget* icon = gtk_image_new_from_icon_name("dialog-warning-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 96);
  gtk_widget_add_css_class(icon, "qs-hero-icon");
  gtk_box_append(GTK_BOX(box), icon);
  v->icon_image = GTK_IMAGE(icon);

  GtkWidget* title = gtk_label_new("Transfer failed");
  gtk_widget_add_css_class(title, "title-1");
  gtk_label_set_xalign(GTK_LABEL(title), 0.5f);
  gtk_box_append(GTK_BOX(box), title);
  v->title_label = GTK_LABEL(title);

  GtkWidget* subtitle = gtk_label_new("");
  gtk_widget_add_css_class(subtitle, "dim-label");
  gtk_label_set_wrap(GTK_LABEL(subtitle), TRUE);
  gtk_label_set_justify(GTK_LABEL(subtitle), GTK_JUSTIFY_CENTER);
  gtk_label_set_xalign(GTK_LABEL(subtitle), 0.5f);
  gtk_box_append(GTK_BOX(box), subtitle);
  v->subtitle_label = GTK_LABEL(subtitle);

  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_halign(row, GTK_ALIGN_CENTER);

  GtkWidget* dismiss = gtk_button_new_with_label("Dismiss");
  gtk_widget_add_css_class(dismiss, "pill");
  g_signal_connect(dismiss, "clicked", G_CALLBACK(on_dismiss_clicked), v);
  gtk_box_append(GTK_BOX(row), dismiss);

  GtkWidget* retry = gtk_button_new_with_label("Try again");
  gtk_widget_add_css_class(retry, "pill");
  gtk_widget_add_css_class(retry, "suggested-action");
  g_signal_connect(retry, "clicked", G_CALLBACK(on_retry_clicked), v);
  gtk_box_append(GTK_BOX(row), retry);

  gtk_box_append(GTK_BOX(box), row);

  adw_clamp_set_child(ADW_CLAMP(clamp), box);
  v->root = clamp;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-failed", v, free_self);
  return v;
}

GtkWidget* qs_view_failed_root(QsViewFailed* v) { return v->root; }

void qs_view_failed_bind(QsViewFailed* v, qs_status_t status,
                         const char* reason) {
  const char* title = "Transfer failed";
  const char* icon_name = "dialog-warning-symbolic";
  switch (status) {
    case QS_STATUS_CANCELLED:
      title = "Transfer cancelled";
      icon_name = "process-stop-symbolic";
      break;
    case QS_STATUS_REJECTED:
      title = "Transfer declined";
      icon_name = "action-unavailable-symbolic";
      break;
    case QS_STATUS_TIMED_OUT:
      title = "Transfer timed out";
      break;
    case QS_STATUS_NOT_ENOUGH_SPACE:
      title = "Not enough space";
      break;
    case QS_STATUS_UNSUPPORTED_ATTACHMENT_TYPE:
      title = "Unsupported file";
      break;
    case QS_STATUS_DEVICE_AUTHENTICATION_FAILED:
      title = "Verification failed";
      break;
    default:
      break;
  }
  gtk_label_set_text(v->title_label, title);
  gtk_image_set_from_icon_name(v->icon_image, icon_name);
  gtk_label_set_text(v->subtitle_label, reason ? reason : "");
}

void qs_view_failed_set_on_retry(QsViewFailed* v, QsIncomingActionCb cb,
                                 gpointer user_data) {
  v->on_retry = cb;
  v->on_retry_data = user_data;
}

void qs_view_failed_set_on_dismiss(QsViewFailed* v, QsIncomingActionCb cb,
                                   gpointer user_data) {
  v->on_dismiss = cb;
  v->on_dismiss_data = user_data;
}
