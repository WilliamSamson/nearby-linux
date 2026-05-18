#include "views.h"

#include <adwaita.h>

#include "format_util.h"

struct QsViewIncoming {
  GtkWidget* root;
  GtkLabel* peer_name_label;
  GtkLabel* subtitle_label;
  AdwActionRow* file_row;
  GtkLabel* token_label;
  GtkWidget* token_box;     // hidden when there's no token
  GtkWidget* spinner;
  GtkWidget* button_box;
  GtkWidget* accept_btn;
  GtkWidget* reject_btn;

  QsIncomingActionCb on_accept;
  gpointer           on_accept_data;
  QsIncomingActionCb on_reject;
  gpointer           on_reject_data;
};

static void free_self(gpointer data) { g_free(data); }

static void on_accept_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewIncoming* v = data;
  if (v->on_accept) v->on_accept(v->on_accept_data);
}

static void on_reject_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewIncoming* v = data;
  if (v->on_reject) v->on_reject(v->on_reject_data);
}

QsViewIncoming* qs_view_incoming_new(void) {
  QsViewIncoming* v = g_new0(QsViewIncoming, 1);

  GtkWidget* clamp = adw_clamp_new();
  adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 540);
  adw_clamp_set_tightening_threshold(ADW_CLAMP(clamp), 400);
  gtk_widget_set_valign(clamp, GTK_ALIGN_CENTER);

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_margin_top(box, 48);
  gtk_widget_set_margin_bottom(box, 48);
  gtk_widget_set_margin_start(box, 32);
  gtk_widget_set_margin_end(box, 32);

  GtkWidget* icon = gtk_image_new_from_icon_name("phone-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 80);
  gtk_widget_add_css_class(icon, "qs-hero-icon");
  gtk_box_append(GTK_BOX(box), icon);

  GtkWidget* peer_label = gtk_label_new("");
  gtk_widget_add_css_class(peer_label, "title-2");
  gtk_label_set_xalign(GTK_LABEL(peer_label), 0.5f);
  gtk_box_append(GTK_BOX(box), peer_label);
  v->peer_name_label = GTK_LABEL(peer_label);

  GtkWidget* subtitle = gtk_label_new("wants to share with you");
  gtk_widget_add_css_class(subtitle, "dim-label");
  gtk_label_set_xalign(GTK_LABEL(subtitle), 0.5f);
  gtk_box_append(GTK_BOX(box), subtitle);
  v->subtitle_label = GTK_LABEL(subtitle);

  GtkWidget* list = gtk_list_box_new();
  gtk_widget_add_css_class(list, "boxed-list");
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);

  GtkWidget* file_row = adw_action_row_new();
  GtkWidget* file_icon = gtk_image_new_from_icon_name("text-x-generic-symbolic");
  adw_action_row_add_prefix(ADW_ACTION_ROW(file_row), file_icon);
  gtk_list_box_append(GTK_LIST_BOX(list), file_row);
  v->file_row = ADW_ACTION_ROW(file_row);

  gtk_box_append(GTK_BOX(box), list);

  GtkWidget* token_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_halign(token_box, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(token_box, "qs-card");
  gtk_widget_set_size_request(token_box, 160, -1);
  
  GtkWidget* token_caption = gtk_label_new("PIN");
  gtk_widget_add_css_class(token_caption, "caption");
  gtk_widget_add_css_class(token_caption, "dim-label");
  gtk_box_append(GTK_BOX(token_box), token_caption);
  
  GtkWidget* token_label = gtk_label_new("");
  gtk_widget_add_css_class(token_label, "title-1");
  gtk_widget_add_css_class(token_label, "numeric");
  gtk_box_append(GTK_BOX(token_box), token_label);
  gtk_box_append(GTK_BOX(box), token_box);
  v->token_label = GTK_LABEL(token_label);
  v->token_box = token_box;

  GtkWidget* spinner = gtk_spinner_new();
  gtk_spinner_set_spinning(GTK_SPINNER(spinner), TRUE);
  gtk_widget_set_halign(spinner, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request(spinner, 32, 32);
  gtk_widget_set_visible(spinner, FALSE);
  gtk_box_append(GTK_BOX(box), spinner);
  v->spinner = spinner;

  GtkWidget* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
  gtk_box_set_homogeneous(GTK_BOX(button_box), TRUE);

  GtkWidget* reject = gtk_button_new_with_label("Decline");
  gtk_widget_add_css_class(reject, "pill");
  g_signal_connect(reject, "clicked", G_CALLBACK(on_reject_clicked), v);
  gtk_box_append(GTK_BOX(button_box), reject);

  GtkWidget* accept = gtk_button_new_with_label("Accept");
  gtk_widget_add_css_class(accept, "pill");
  gtk_widget_add_css_class(accept, "suggested-action");
  g_signal_connect(accept, "clicked", G_CALLBACK(on_accept_clicked), v);
  gtk_box_append(GTK_BOX(button_box), accept);

  gtk_box_append(GTK_BOX(box), button_box);
  v->button_box = button_box;
  v->accept_btn = accept;
  v->reject_btn = reject;

  adw_clamp_set_child(ADW_CLAMP(clamp), box);
  v->root = clamp;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-incoming", v, free_self);
  return v;
}

GtkWidget* qs_view_incoming_root(QsViewIncoming* v) { return v->root; }

void qs_view_incoming_bind_session(QsViewIncoming* v,
                                   const qs_session_t* session) {
  const char* peer = qs_session_peer_name(session);
  gtk_label_set_text(v->peer_name_label, peer ? peer : "Someone nearby");

  size_t n = qs_session_attachment_count(session);
  if (n > 0) {
    const qs_attachment_t* atts = qs_session_attachments(session);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(v->file_row),
                                  atts[0].display_name);
    char* size = qs_format_bytes(atts[0].size_bytes);
    adw_action_row_set_subtitle(ADW_ACTION_ROW(v->file_row), size);
    g_free(size);
    char* sub = g_strdup_printf(n == 1
                                  ? "wants to share 1 item"
                                  : "wants to share %zu items",
                                n);
    gtk_label_set_text(v->subtitle_label, sub);
    g_free(sub);
  }

  const char* token = qs_session_token(session);
  if (token && *token) {
    gtk_label_set_text(v->token_label, token);
    gtk_widget_set_visible(v->token_box, TRUE);
  } else {
    gtk_widget_set_visible(v->token_box, FALSE);
  }
}

void qs_view_incoming_set_awaiting_confirmation(QsViewIncoming* v,
                                                gboolean awaiting) {
  gtk_widget_set_visible(v->button_box, awaiting);
  gtk_widget_set_visible(v->spinner, !awaiting);
}

void qs_view_incoming_set_on_accept(QsViewIncoming* v,
                                    QsIncomingActionCb cb, gpointer user_data) {
  v->on_accept = cb;
  v->on_accept_data = user_data;
}

void qs_view_incoming_set_on_reject(QsViewIncoming* v,
                                    QsIncomingActionCb cb, gpointer user_data) {
  v->on_reject = cb;
  v->on_reject_data = user_data;
}
