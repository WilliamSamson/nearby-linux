#include "views.h"

#include <adwaita.h>

#define QS_VISIBILITY_DURATION_SECONDS 600   // 10 minutes

struct QsViewHome {
  GtkWidget* root;

  GtkLabel* device_name_label;
  GtkLabel* hostname_subtitle;

  GtkWidget* status_button;        // flat tappable wrapping the dot + labels
  GtkWidget* status_dot;
  GtkLabel*  state_label;          // "Device is hidden" / "Receive from everyone"
  GtkLabel*  state_sub;            // hint / countdown / permanent explanation
  GtkWidget* keep_on_check;        // shown only while is_visible
  GtkSwitch* status_switch;        // the visible dashboard toggle switch!
  gboolean   is_visible;
  gboolean   keep_on;              // true => no countdown, stay visible

  GtkWidget* recent_section;       // box containing header + list
  GtkWidget* recent_empty;         // shown when list empty

  // 10-min countdown machinery
  guint    countdown_source;       // 0 when not running
  int      seconds_remaining;

  // Send Mode & Scanner Elements
  GtkWidget* right_column;
  GtkWidget* drop_zone;
  GtkWidget* scanner_box;
  GtkListBox* device_list_box;
  GtkWidget* spinner;
  char* selected_file_path;

  QsHomeToggleCb on_toggle;
  gpointer       on_toggle_data;

  QsHomeSettingsCb on_settings;
  gpointer         on_settings_data;

  QsHomeSendFileCb on_send_file;
  gpointer         on_send_file_data;

  QsHomeDeviceSelectedCb on_device_selected;
  gpointer               on_device_selected_data;
};

// ---- state rendering -------------------------------------------------------

static void render_hidden(QsViewHome* v) {
  gtk_label_set_text(v->state_label, "Device is hidden");
  gtk_label_set_text(v->state_sub,   "Tap to receive from everyone");
  gtk_widget_remove_css_class(v->status_dot, "online");
  gtk_widget_add_css_class(v->status_dot, "offline");
  if (v->keep_on_check) gtk_widget_set_visible(v->keep_on_check, FALSE);
  if (v->status_switch && gtk_switch_get_active(v->status_switch)) {
    gtk_switch_set_active(v->status_switch, FALSE);
  }
}

static void render_visible_title(QsViewHome* v) {
  gtk_label_set_text(v->state_label, "Receive from everyone");
  gtk_widget_remove_css_class(v->status_dot, "offline");
  gtk_widget_add_css_class(v->status_dot, "online");
  if (v->keep_on_check) gtk_widget_set_visible(v->keep_on_check, TRUE);
  if (v->status_switch && !gtk_switch_get_active(v->status_switch)) {
    gtk_switch_set_active(v->status_switch, TRUE);
  }
}

static void set_sub_remaining(QsViewHome* v) {
  int mm = v->seconds_remaining / 60;
  int ss = v->seconds_remaining % 60;
  char* sub = g_strdup_printf(
      "Anyone nearby can temporarily share with you · %d:%02d", mm, ss);
  gtk_label_set_text(v->state_sub, sub);
  g_free(sub);
}

static void set_sub_permanent(QsViewHome* v) {
  gtk_label_set_text(v->state_sub, "Anyone nearby can share with you");
}

// ---- countdown -------------------------------------------------------------

static gboolean countdown_tick(gpointer data) {
  QsViewHome* v = data;
  v->seconds_remaining--;
  if (v->seconds_remaining <= 0) {
    v->countdown_source = 0;
    v->is_visible = FALSE;
    render_hidden(v);
    if (v->on_toggle) v->on_toggle(FALSE, v->on_toggle_data);
    return G_SOURCE_REMOVE;
  }
  set_sub_remaining(v);
  return G_SOURCE_CONTINUE;
}

static void cancel_countdown(QsViewHome* v) {
  if (v->countdown_source != 0) {
    g_source_remove(v->countdown_source);
    v->countdown_source = 0;
  }
}

static void start_countdown(QsViewHome* v) {
  cancel_countdown(v);
  v->seconds_remaining = QS_VISIBILITY_DURATION_SECONDS;
  render_visible_title(v);
  set_sub_remaining(v);
  v->countdown_source = g_timeout_add_seconds(1, countdown_tick, v);
}

static void enter_visible_permanent(QsViewHome* v) {
  cancel_countdown(v);
  render_visible_title(v);
  set_sub_permanent(v);
}

static void enter_hidden(QsViewHome* v) {
  cancel_countdown(v);
  render_hidden(v);
}

// ---- signal handlers -------------------------------------------------------

static void on_status_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewHome* v = data;
  v->is_visible = !v->is_visible;
  if (v->is_visible) {
    if (v->keep_on) enter_visible_permanent(v); else start_countdown(v);
  } else {
    enter_hidden(v);
  }
  if (v->on_toggle) v->on_toggle(v->is_visible, v->on_toggle_data);
}

static void on_keep_on_toggled(GtkCheckButton* btn, gpointer data) {
  QsViewHome* v = data;
  v->keep_on = gtk_check_button_get_active(btn);
  if (!v->is_visible) return;
  if (v->keep_on) enter_visible_permanent(v); else start_countdown(v);
}

static void free_self(gpointer data) {
  QsViewHome* v = data;
  if (v->countdown_source != 0) g_source_remove(v->countdown_source);
  g_free(v);
}

// ---- construction ----------------------------------------------------------

static GtkWidget* build_left_column(QsViewHome* v) {
  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_hexpand(box, TRUE);
  gtk_widget_set_size_request(box, 260, -1);
  gtk_widget_set_margin_start(box, 0); // Flush to left
  gtk_widget_set_margin_end(box, 0);
  gtk_widget_set_margin_top(box, 0);
  gtk_widget_set_margin_bottom(box, 0);
  gtk_widget_add_css_class(box, "qs-home-left");

  GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 32);
  gtk_widget_set_margin_start(content, 32);
  gtk_widget_set_margin_end(content, 32);
  gtk_widget_set_margin_top(content, 32);
  gtk_widget_set_margin_bottom(content, 32);
  gtk_box_append(GTK_BOX(box), content);

  v->device_name_label = GTK_LABEL(gtk_label_new("Ubuntu Device"));
  gtk_widget_add_css_class(GTK_WIDGET(v->device_name_label), "title-2");
  gtk_label_set_xalign(v->device_name_label, 0.0f);
  gtk_box_append(GTK_BOX(content), GTK_WIDGET(v->device_name_label));

  GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

  v->status_dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(v->status_dot, "qs-status-dot");
  gtk_widget_add_css_class(v->status_dot, "offline");
  gtk_widget_set_valign(v->status_dot, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(row), v->status_dot);

  GtkWidget* state_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  v->state_label = GTK_LABEL(gtk_label_new("Device is hidden"));
  gtk_widget_add_css_class(GTK_WIDGET(v->state_label), "bold");
  gtk_label_set_xalign(v->state_label, 0.0f);
  gtk_box_append(GTK_BOX(state_box), GTK_WIDGET(v->state_label));

  v->state_sub = GTK_LABEL(gtk_label_new("Tap to receive from everyone"));
  gtk_widget_add_css_class(GTK_WIDGET(v->state_sub), "dim-label");
  gtk_widget_add_css_class(GTK_WIDGET(v->state_sub), "caption");
  gtk_label_set_xalign(v->state_sub, 0.0f);
  gtk_label_set_wrap(v->state_sub, TRUE);
  gtk_label_set_max_width_chars(v->state_sub, 40);
  gtk_box_append(GTK_BOX(state_box), GTK_WIDGET(v->state_sub));

  gtk_box_append(GTK_BOX(row), state_box);

  GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(spacer, TRUE);
  gtk_box_append(GTK_BOX(row), spacer);

  GtkWidget* chevron = gtk_image_new_from_icon_name("pan-end-symbolic");
  gtk_widget_add_css_class(chevron, "dim-label");
  gtk_widget_add_css_class(chevron, "qs-status-chevron");
  gtk_widget_set_valign(chevron, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(row), chevron);

  v->status_button = gtk_button_new();
  gtk_widget_add_css_class(v->status_button, "flat");
  gtk_widget_add_css_class(v->status_button, "qs-status-toggle");
  gtk_widget_set_halign(v->status_button, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(v->status_button, TRUE);
  gtk_button_set_child(GTK_BUTTON(v->status_button), row);
  gtk_widget_set_tooltip_text(v->status_button,
                              "Toggle visibility to nearby devices");
  g_signal_connect(v->status_button, "clicked",
                   G_CALLBACK(on_status_clicked), v);
  gtk_box_append(GTK_BOX(content), v->status_button);

  v->keep_on_check = gtk_check_button_new_with_label("Keep on permanently");
  gtk_widget_add_css_class(v->keep_on_check, "caption");
  gtk_widget_set_margin_start(v->keep_on_check, 10);
  gtk_widget_set_visible(v->keep_on_check, FALSE);
  g_signal_connect(v->keep_on_check, "toggled",
                   G_CALLBACK(on_keep_on_toggled), v);
  gtk_box_append(GTK_BOX(content), v->keep_on_check);

  return box;
}

static void on_settings_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewHome* v = data;
  if (v->on_settings) v->on_settings(v->on_settings_data);
}

static GtkWidget* build_right_column(QsViewHome* v) {
  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 32);
  gtk_widget_set_hexpand(box, TRUE);
  gtk_widget_set_margin_start(box, 32);
  gtk_widget_set_margin_end(box, 32);
  gtk_widget_set_margin_top(box, 32);
  gtk_widget_set_margin_bottom(box, 32);

  // Notification Prompt
  GtkWidget* notif_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(notif_box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_bottom(notif_box, 12);
  
  GtkWidget* notif_label = gtk_label_new("Turn on system notification");
  gtk_widget_add_css_class(notif_label, "caption");
  gtk_box_append(GTK_BOX(notif_box), notif_label);

  GtkWidget* settings_btn = gtk_button_new_with_label("Settings");
  gtk_widget_add_css_class(settings_btn, "flat");
  gtk_widget_add_css_class(settings_btn, "qs-link-button");
  g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_clicked), v);
  gtk_box_append(GTK_BOX(notif_box), settings_btn);

  gtk_box_append(GTK_BOX(box), notif_box);

  // Big Logo
  GtkWidget* logo = NULL;
  const char* paths[] = {
    "app/ui-gtk/data/app-icon.png",
    "data/app-icon.png"
  };
  for (size_t i = 0; i < G_N_ELEMENTS(paths); i++) {
    if (g_file_test(paths[i], G_FILE_TEST_EXISTS)) {
      logo = gtk_image_new_from_file(paths[i]);
      break;
    }
  }
  if (!logo) logo = gtk_image_new_from_icon_name("send-to-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(logo), 180);
  gtk_widget_set_halign(logo, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(logo, "qs-logo-float");
  gtk_box_append(GTK_BOX(box), logo);

  // Drop Zone
  GtkWidget* drop_zone = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
  gtk_widget_add_css_class(drop_zone, "qs-drop-zone");
  gtk_widget_set_halign(drop_zone, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(drop_zone, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request(drop_zone, 380, 240);

  GtkWidget* upload_icon = gtk_image_new_from_icon_name("folder-download-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(upload_icon), 56);
  gtk_widget_add_css_class(upload_icon, "dim-label");
  gtk_box_append(GTK_BOX(drop_zone), upload_icon);

  GtkWidget* drop_sub = gtk_label_new("drop files or folders to send");
  gtk_widget_add_css_class(drop_sub, "dim-label");
  gtk_box_append(GTK_BOX(drop_zone), drop_sub);

  GtkWidget* select_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign(select_box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(select_box, 12);
  
  GtkWidget* plus_icon = gtk_image_new_from_icon_name("list-add-symbolic");
  gtk_box_append(GTK_BOX(select_box), plus_icon);

  GtkWidget* select_label = gtk_label_new("select");
  gtk_widget_add_css_class(select_label, "bold");
  gtk_box_append(GTK_BOX(select_box), select_label);

  gtk_box_append(GTK_BOX(drop_zone), select_box);
  gtk_box_append(GTK_BOX(box), drop_zone);

  return box;
}

QsViewHome* qs_view_home_new(void) {
  QsViewHome* v = g_new0(QsViewHome, 1);

  GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_add_css_class(main_box, "qs-home-layout");

  GtkWidget* left = build_left_column(v);
  GtkWidget* right = build_right_column(v);

  gtk_box_append(GTK_BOX(main_box), left);
  gtk_box_append(GTK_BOX(main_box), right);

  v->root = main_box;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-home", v, free_self);
  return v;
}

GtkWidget* qs_view_home_root(QsViewHome* v) { return v->root; }

void qs_view_home_set_visible(QsViewHome* v, gboolean visible) {
  if (!v) return;
  if (v->is_visible == visible) return;
  v->is_visible = visible;
  if (visible) {
    if (v->keep_on) enter_visible_permanent(v); else start_countdown(v);
  } else {
    enter_hidden(v);
  }
}

void qs_view_home_set_device_name(QsViewHome* v, const char* name) {
  if (!v || !name) return;
  gtk_label_set_text(v->device_name_label, name);
}

void qs_view_home_set_backend_state(QsViewHome* v,
                                    const qs_backend_state_t* state) {
  if (!v || !state || v->is_visible) return;
  if (state->summary && *state->summary) {
    gtk_label_set_text(v->state_label, state->summary);
  }
  if (state->detail && *state->detail) {
    gtk_label_set_text(v->state_sub, state->detail);
  }
}

void qs_view_home_set_on_toggle(QsViewHome* v, QsHomeToggleCb cb,
                                gpointer user_data) {
  v->on_toggle = cb;
  v->on_toggle_data = user_data;
}

void qs_view_home_set_on_settings(QsViewHome* v, QsHomeSettingsCb cb,
                                  gpointer user_data) {
  v->on_settings = cb;
  v->on_settings_data = user_data;
}
