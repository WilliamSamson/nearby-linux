#include "views.h"

#include <adwaita.h>

#include "format_util.h"

struct QsViewTransferring {
  GtkWidget* root;
  GtkLabel* peer_label;
  GtkLabel* file_label;
  GtkProgressBar* progress_bar;
  GtkLabel* progress_text;
  GtkLabel* speed_label;
  GtkLabel* eta_label;
  GtkWidget* cancel_btn;

  QsIncomingActionCb on_cancel;
  gpointer           on_cancel_data;
};

static void free_self(gpointer data) { g_free(data); }

static void on_cancel_clicked(GtkButton* b G_GNUC_UNUSED, gpointer data) {
  QsViewTransferring* v = data;
  if (v->on_cancel) v->on_cancel(v->on_cancel_data);
}

QsViewTransferring* qs_view_transferring_new(void) {
  QsViewTransferring* v = g_new0(QsViewTransferring, 1);

  GtkWidget* clamp = adw_clamp_new();
  adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 540);
  adw_clamp_set_tightening_threshold(ADW_CLAMP(clamp), 400);
  gtk_widget_set_valign(clamp, GTK_ALIGN_CENTER);

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_margin_top(box, 48);
  gtk_widget_set_margin_bottom(box, 48);
  gtk_widget_set_margin_start(box, 32);
  gtk_widget_set_margin_end(box, 32);

  GtkWidget* icon = gtk_image_new_from_icon_name("folder-download-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 64);
  gtk_widget_add_css_class(icon, "qs-hero-icon");
  gtk_box_append(GTK_BOX(box), icon);

  GtkWidget* peer_label = gtk_label_new("");
  gtk_widget_add_css_class(peer_label, "title-2");
  gtk_label_set_xalign(GTK_LABEL(peer_label), 0.5f);
  gtk_box_append(GTK_BOX(box), peer_label);
  v->peer_label = GTK_LABEL(peer_label);

  GtkWidget* file_label = gtk_label_new("");
  gtk_widget_add_css_class(file_label, "dim-label");
  gtk_label_set_xalign(GTK_LABEL(file_label), 0.5f);
  gtk_label_set_ellipsize(GTK_LABEL(file_label), PANGO_ELLIPSIZE_MIDDLE);
  gtk_box_append(GTK_BOX(box), file_label);
  v->file_label = GTK_LABEL(file_label);

  GtkWidget* progress = gtk_progress_bar_new();
  gtk_box_append(GTK_BOX(box), progress);
  v->progress_bar = GTK_PROGRESS_BAR(progress);

  GtkWidget* progress_text = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(progress_text), 0.5f);
  gtk_box_append(GTK_BOX(box), progress_text);
  v->progress_text = GTK_LABEL(progress_text);

  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
  GtkWidget* speed = gtk_label_new("");
  gtk_widget_add_css_class(speed, "caption");
  gtk_widget_add_css_class(speed, "dim-label");
  GtkWidget* dot = gtk_label_new("·");
  gtk_widget_add_css_class(dot, "dim-label");
  GtkWidget* eta = gtk_label_new("");
  gtk_widget_add_css_class(eta, "caption");
  gtk_widget_add_css_class(eta, "dim-label");
  gtk_box_append(GTK_BOX(row), speed);
  gtk_box_append(GTK_BOX(row), dot);
  gtk_box_append(GTK_BOX(row), eta);
  gtk_box_append(GTK_BOX(box), row);
  v->speed_label = GTK_LABEL(speed);
  v->eta_label = GTK_LABEL(eta);

  GtkWidget* cancel = gtk_button_new_with_label("Cancel");
  gtk_widget_add_css_class(cancel, "pill");
  gtk_widget_set_halign(cancel, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(cancel, 12);
  g_signal_connect(cancel, "clicked", G_CALLBACK(on_cancel_clicked), v);
  gtk_box_append(GTK_BOX(box), cancel);
  v->cancel_btn = cancel;

  adw_clamp_set_child(ADW_CLAMP(clamp), box);
  v->root = clamp;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-transferring", v, free_self);
  return v;
}

GtkWidget* qs_view_transferring_root(QsViewTransferring* v) { return v->root; }

void qs_view_transferring_bind_session(QsViewTransferring* v,
                                       const qs_session_t* session) {
  gtk_label_set_text(v->peer_label, qs_session_peer_name(session));
  if (qs_session_attachment_count(session) > 0) {
    const qs_attachment_t* a = qs_session_attachments(session);
    gtk_label_set_text(v->file_label, a[0].display_name);
  }
}

void qs_view_transferring_set_progress(QsViewTransferring* v,
                                       const qs_progress_t* progress,
                                       uint64_t total_bytes) {
  gtk_progress_bar_set_fraction(v->progress_bar, progress->progress);
  char* bytes = qs_format_progress_bytes(progress->transferred_bytes,
                                         total_bytes);
  gtk_label_set_text(v->progress_text, bytes);
  g_free(bytes);

  char* speed = qs_format_speed(progress->transfer_speed_bps);
  gtk_label_set_text(v->speed_label, speed);
  g_free(speed);

  char* eta = qs_format_eta(progress->estimated_seconds_remaining);
  gtk_label_set_text(v->eta_label, eta);
  g_free(eta);
}

void qs_view_transferring_set_on_cancel(QsViewTransferring* v,
                                        QsIncomingActionCb cb,
                                        gpointer user_data) {
  v->on_cancel = cb;
  v->on_cancel_data = user_data;
}
