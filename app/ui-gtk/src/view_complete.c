#include "views.h"

#include <adwaita.h>

struct QsViewComplete {
  GtkWidget* root;
  GtkLabel* title_label;
  GtkLabel* subtitle_label;
  GtkWidget* show_in_folder_btn;
  GtkWidget* done_btn;

  QsIncomingActionCb on_show_in_folder;
  gpointer           on_show_in_folder_data;
  QsIncomingActionCb on_done;
  gpointer           on_done_data;
};

static void free_self(gpointer data) { g_free(data); }

static void on_show_clicked(GtkButton* b G_GNUC_UNUSED, gpointer data) {
  QsViewComplete* v = data;
  if (v->on_show_in_folder) v->on_show_in_folder(v->on_show_in_folder_data);
}

static void on_done_clicked(GtkButton* b G_GNUC_UNUSED, gpointer data) {
  QsViewComplete* v = data;
  if (v->on_done) v->on_done(v->on_done_data);
}

QsViewComplete* qs_view_complete_new(void) {
  QsViewComplete* v = g_new0(QsViewComplete, 1);

  GtkWidget* clamp = adw_clamp_new();
  adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 540);
  adw_clamp_set_tightening_threshold(ADW_CLAMP(clamp), 400);
  gtk_widget_set_valign(clamp, GTK_ALIGN_CENTER);

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_margin_top(box, 48);
  gtk_widget_set_margin_bottom(box, 48);
  gtk_widget_set_margin_start(box, 32);
  gtk_widget_set_margin_end(box, 32);

  GtkWidget* icon = gtk_image_new_from_icon_name("emblem-ok-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 96);
  gtk_widget_add_css_class(icon, "qs-hero-icon");
  gtk_box_append(GTK_BOX(box), icon);

  GtkWidget* title = gtk_label_new("Received");
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

  GtkWidget* button_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_halign(button_row, GTK_ALIGN_CENTER);

  GtkWidget* done = gtk_button_new_with_label("Done");
  gtk_widget_add_css_class(done, "pill");
  g_signal_connect(done, "clicked", G_CALLBACK(on_done_clicked), v);
  gtk_box_append(GTK_BOX(button_row), done);
  v->done_btn = done;

  GtkWidget* show = gtk_button_new_with_label("Show in folder");
  gtk_widget_add_css_class(show, "pill");
  gtk_widget_add_css_class(show, "suggested-action");
  g_signal_connect(show, "clicked", G_CALLBACK(on_show_clicked), v);
  gtk_box_append(GTK_BOX(button_row), show);
  v->show_in_folder_btn = show;

  gtk_box_append(GTK_BOX(box), button_row);

  adw_clamp_set_child(ADW_CLAMP(clamp), box);
  v->root = clamp;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-complete", v, free_self);
  return v;
}

GtkWidget* qs_view_complete_root(QsViewComplete* v) { return v->root; }

void qs_view_complete_bind_session(QsViewComplete* v,
                                   const qs_session_t* session) {
  const char* file_name = "file";
  if (qs_session_attachment_count(session) > 0) {
    const qs_attachment_t* a = qs_session_attachments(session);
    file_name = a[0].display_name;
  }
  char* title = g_strdup_printf("Received %s", file_name);
  gtk_label_set_text(v->title_label, title);
  g_free(title);

  const char* path = qs_session_saved_path(session);
  if (path && *path) {
    char* sub = g_strdup_printf("Saved to %s", path);
    gtk_label_set_text(v->subtitle_label, sub);
    g_free(sub);
    gtk_widget_set_visible(v->show_in_folder_btn, TRUE);
  } else {
    gtk_label_set_text(v->subtitle_label, "");
    gtk_widget_set_visible(v->show_in_folder_btn, FALSE);
  }
}

void qs_view_complete_set_on_show_in_folder(QsViewComplete* v,
                                            QsIncomingActionCb cb,
                                            gpointer user_data) {
  v->on_show_in_folder = cb;
  v->on_show_in_folder_data = user_data;
}

void qs_view_complete_set_on_done(QsViewComplete* v,
                                  QsIncomingActionCb cb, gpointer user_data) {
  v->on_done = cb;
  v->on_done_data = user_data;
}
