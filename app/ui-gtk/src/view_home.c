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
  GtkWidget* chevron_icon;         // arrow showing expanded/collapsed state
  GtkWidget* visibility_options_box; // expanded options drawer
  GtkWidget* hidden_option_btn;    // Option row for Hidden
  GtkWidget* everyone_option_btn;  // Option row for Everyone
  GtkWidget* hidden_check_icon;    // check mark on Hidden
  GtkWidget* everyone_check_icon;  // check mark on Everyone
  GtkWidget* everyone_details_box;  // details box below options
  gboolean   is_visible;
  gboolean   keep_on;              // true => no countdown, stay visible

  GtkWidget* recent_section;       // box containing header + list
  GtkWidget* recent_empty;         // shown when list empty

  // 10-min countdown machinery
  guint    countdown_source;       // 0 when not running
  int      seconds_remaining;

  // Send Mode & Scanner Elements
  GtkWidget* right_column;
  GtkWidget* notif_box;
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

static void update_visibility_option_ui(QsViewHome* v) {
  if (!v->hidden_check_icon || !v->everyone_check_icon) return;
  if (v->is_visible) {
    gtk_widget_set_visible(v->hidden_check_icon, FALSE);
    gtk_widget_set_visible(v->everyone_check_icon, TRUE);
    if (v->everyone_details_box) gtk_widget_set_visible(v->everyone_details_box, TRUE);
  } else {
    gtk_widget_set_visible(v->hidden_check_icon, TRUE);
    gtk_widget_set_visible(v->everyone_check_icon, FALSE);
    if (v->everyone_details_box) gtk_widget_set_visible(v->everyone_details_box, FALSE);
  }
}

static void render_hidden(QsViewHome* v) {
  gtk_label_set_text(v->state_label, "Device is hidden");
  gtk_label_set_text(v->state_sub,   "Tap to receive from everyone");
  gtk_widget_remove_css_class(v->status_dot, "online");
  gtk_widget_add_css_class(v->status_dot, "offline");
  update_visibility_option_ui(v);
}

static void render_visible_title(QsViewHome* v) {
  gtk_label_set_text(v->state_label, "Receive from everyone");
  gtk_widget_remove_css_class(v->status_dot, "offline");
  gtk_widget_add_css_class(v->status_dot, "online");
  update_visibility_option_ui(v);
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
  if (!v->visibility_options_box) return;
  gboolean is_expanded = gtk_widget_get_visible(v->visibility_options_box);
  gtk_widget_set_visible(v->visibility_options_box, !is_expanded);
  if (!is_expanded) {
    gtk_image_set_from_icon_name(GTK_IMAGE(v->chevron_icon), "pan-up-symbolic");
  } else {
    gtk_image_set_from_icon_name(GTK_IMAGE(v->chevron_icon), "pan-down-symbolic");
  }
}

static void on_hidden_option_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewHome* v = data;
  if (v->visibility_options_box) {
    gtk_widget_set_visible(v->visibility_options_box, FALSE);
    if (v->chevron_icon) gtk_image_set_from_icon_name(GTK_IMAGE(v->chevron_icon), "pan-down-symbolic");
  }
  if (!v->is_visible) return;
  v->is_visible = FALSE;
  enter_hidden(v);
  if (v->on_toggle) v->on_toggle(FALSE, v->on_toggle_data);
}

static void on_everyone_option_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewHome* v = data;
  if (v->visibility_options_box) {
    gtk_widget_set_visible(v->visibility_options_box, FALSE);
    if (v->chevron_icon) gtk_image_set_from_icon_name(GTK_IMAGE(v->chevron_icon), "pan-down-symbolic");
  }
  if (v->is_visible) return;
  v->is_visible = TRUE;
  if (v->keep_on) enter_visible_permanent(v); else start_countdown(v);
  if (v->on_toggle) v->on_toggle(TRUE, v->on_toggle_data);
}

static void on_keep_on_toggled(GtkCheckButton* btn, gpointer data) {
  QsViewHome* v = data;
  v->keep_on = gtk_check_button_get_active(btn);
  if (!v->is_visible) return;
  if (v->keep_on) enter_visible_permanent(v); else start_countdown(v);
}

static void on_select_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data);

static void on_file_selected(GObject* source, GAsyncResult* res, gpointer data) {
  GtkFileDialog* dialog = GTK_FILE_DIALOG(source);
  QsViewHome* v = data;
  GError* error = NULL;
  GFile* file = gtk_file_dialog_open_finish(dialog, res, &error);
  if (file) {
    char* path = g_file_get_path(file);
    g_free(v->selected_file_path);
    v->selected_file_path = g_strdup(path);
    g_object_unref(file);
    g_free(path);
    
    gtk_widget_set_visible(v->drop_zone, FALSE);
    gtk_widget_set_visible(v->scanner_box, TRUE);
    gtk_spinner_start(GTK_SPINNER(v->spinner));
    
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(v->device_list_box))) != NULL) {
      gtk_list_box_remove(v->device_list_box, child);
    }
    
    if (v->on_send_file) {
      v->on_send_file(v->selected_file_path, v->on_send_file_data);
    }
  }
  g_clear_error(&error);
}

static void on_select_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewHome* v = data;
  GtkFileDialog* dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, "Select File to Send");
  gtk_file_dialog_open(dialog, GTK_WINDOW(gtk_widget_get_root(v->root)), NULL, on_file_selected, v);
}

static void on_drop_zone_click(GtkGestureClick* gesture G_GNUC_UNUSED, int n_press G_GNUC_UNUSED, double x G_GNUC_UNUSED, double y G_GNUC_UNUSED, gpointer data) {
  on_select_clicked(NULL, data);
}

struct DeviceItemData {
  char* ip;
  int port;
  QsViewHome* home_view;
};

static void free_device_item_data(gpointer data) {
  struct DeviceItemData* d = data;
  g_free(d->ip);
  g_free(d);
}

static void on_device_row_activated(GtkListBox* box G_GNUC_UNUSED, GtkListBoxRow* row, gpointer data) {
  (void)data;
  struct DeviceItemData* d = g_object_get_data(G_OBJECT(row), "device-data");
  if (d && d->home_view->on_device_selected) {
    d->home_view->on_device_selected(d->ip, d->port, d->home_view->on_device_selected_data);
  }
}

void qs_view_home_add_discovered_device(QsViewHome* v, const char* name, const char* ip, int port) {
  if (!v || !name || !ip) return;
  
  GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(v->device_list_box));
  while (child != NULL) {
    struct DeviceItemData* d = g_object_get_data(G_OBJECT(child), "device-data");
    if (d && g_strcmp0(d->ip, ip) == 0 && d->port == port) {
      return;
    }
    child = gtk_widget_get_next_sibling(child);
  }
  
  GtkWidget* row = adw_action_row_new();
  gtk_widget_add_css_class(row, "qs-scanner-device-row");
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), name);
  adw_action_row_set_subtitle(ADW_ACTION_ROW(row), ip);
  
  GtkWidget* icon = gtk_image_new_from_icon_name("phone-symbolic");
  adw_action_row_add_prefix(ADW_ACTION_ROW(row), icon);
  
  struct DeviceItemData* d = g_new0(struct DeviceItemData, 1);
  d->ip = g_strdup(ip);
  d->port = port;
  d->home_view = v;
  g_object_set_data_full(G_OBJECT(row), "device-data", d, free_device_item_data);
  
  gtk_list_box_append(v->device_list_box, row);
}

static void on_cancel_scan_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewHome* v = data;
  qs_view_home_reset_send_mode(v);
}

void qs_view_home_reset_send_mode(QsViewHome* v) {
  if (!v) return;
  gtk_spinner_stop(GTK_SPINNER(v->spinner));
  gtk_widget_set_visible(v->scanner_box, FALSE);
  gtk_widget_set_visible(v->drop_zone, TRUE);
  g_free(v->selected_file_path);
  v->selected_file_path = NULL;
}

static void free_self(gpointer data) {
  QsViewHome* v = data;
  if (v->countdown_source != 0) g_source_remove(v->countdown_source);
  g_free(v->selected_file_path);
  g_free(v);
}

// ---- construction ----------------------------------------------------------

static GtkWidget* build_left_column(QsViewHome* v) {
  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_hexpand(box, TRUE);
  gtk_widget_set_size_request(box, 260, -1);
  gtk_widget_set_margin_start(box, 0);
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

  v->chevron_icon = gtk_image_new_from_icon_name("pan-down-symbolic");
  gtk_widget_set_valign(v->chevron_icon, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(row), v->chevron_icon);

  v->status_button = gtk_button_new();
  gtk_widget_add_css_class(v->status_button, "flat");
  gtk_widget_add_css_class(v->status_button, "qs-status-toggle");
  gtk_widget_set_halign(v->status_button, GTK_ALIGN_FILL);
  gtk_widget_set_hexpand(v->status_button, TRUE);
  gtk_button_set_child(GTK_BUTTON(v->status_button), row);
  gtk_widget_set_tooltip_text(v->status_button,
                              "Change visibility settings");
  g_signal_connect(v->status_button, "clicked",
                   G_CALLBACK(on_status_clicked), v);
  gtk_box_append(GTK_BOX(content), v->status_button);

  // Drawer options
  v->visibility_options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_add_css_class(v->visibility_options_box, "qs-visibility-options-box");
  gtk_widget_set_visible(v->visibility_options_box, FALSE);

  // Hidden option button
  v->hidden_option_btn = gtk_button_new();
  gtk_widget_add_css_class(v->hidden_option_btn, "flat");
  gtk_widget_add_css_class(v->hidden_option_btn, "qs-visibility-option");
  GtkWidget* hidden_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  v->hidden_check_icon = gtk_image_new_from_icon_name("object-select-symbolic");
  gtk_widget_add_css_class(v->hidden_check_icon, "qs-visibility-option-icon");
  gtk_box_append(GTK_BOX(hidden_row), v->hidden_check_icon);
  GtkWidget* hidden_texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  GtkWidget* hidden_title = gtk_label_new("Hidden");
  gtk_widget_add_css_class(hidden_title, "bold");
  gtk_label_set_xalign(GTK_LABEL(hidden_title), 0.0f);
  GtkWidget* hidden_desc = gtk_label_new("Not visible to anyone");
  gtk_widget_add_css_class(hidden_desc, "dim-label");
  gtk_widget_add_css_class(hidden_desc, "caption");
  gtk_label_set_xalign(GTK_LABEL(hidden_desc), 0.0f);
  gtk_box_append(GTK_BOX(hidden_texts), hidden_title);
  gtk_box_append(GTK_BOX(hidden_texts), hidden_desc);
  gtk_box_append(GTK_BOX(hidden_row), hidden_texts);
  gtk_button_set_child(GTK_BUTTON(v->hidden_option_btn), hidden_row);
  g_signal_connect(v->hidden_option_btn, "clicked", G_CALLBACK(on_hidden_option_clicked), v);
  gtk_box_append(GTK_BOX(v->visibility_options_box), v->hidden_option_btn);

  // Everyone option button
  v->everyone_option_btn = gtk_button_new();
  gtk_widget_add_css_class(v->everyone_option_btn, "flat");
  gtk_widget_add_css_class(v->everyone_option_btn, "qs-visibility-option");
  GtkWidget* everyone_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  v->everyone_check_icon = gtk_image_new_from_icon_name("object-select-symbolic");
  gtk_widget_add_css_class(v->everyone_check_icon, "qs-visibility-option-icon");
  gtk_box_append(GTK_BOX(everyone_row), v->everyone_check_icon);
  GtkWidget* everyone_texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  GtkWidget* everyone_title = gtk_label_new("Everyone");
  gtk_widget_add_css_class(everyone_title, "bold");
  gtk_label_set_xalign(GTK_LABEL(everyone_title), 0.0f);
  GtkWidget* everyone_desc = gtk_label_new("Visible to everyone nearby");
  gtk_widget_add_css_class(everyone_desc, "dim-label");
  gtk_widget_add_css_class(everyone_desc, "caption");
  gtk_label_set_xalign(GTK_LABEL(everyone_desc), 0.0f);
  gtk_box_append(GTK_BOX(everyone_texts), everyone_title);
  gtk_box_append(GTK_BOX(everyone_texts), everyone_desc);
  gtk_box_append(GTK_BOX(everyone_row), everyone_texts);
  gtk_button_set_child(GTK_BUTTON(v->everyone_option_btn), everyone_row);
  g_signal_connect(v->everyone_option_btn, "clicked", G_CALLBACK(on_everyone_option_clicked), v);
  gtk_box_append(GTK_BOX(v->visibility_options_box), v->everyone_option_btn);

  // Everyone details box (for permanent toggle)
  v->everyone_details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_start(v->everyone_details_box, 12);
  gtk_widget_set_margin_top(v->everyone_details_box, 8);
  gtk_widget_set_margin_bottom(v->everyone_details_box, 4);

  v->keep_on_check = gtk_check_button_new_with_label("Keep on permanently");
  gtk_widget_add_css_class(v->keep_on_check, "caption");
  g_signal_connect(v->keep_on_check, "toggled",
                   G_CALLBACK(on_keep_on_toggled), v);
  gtk_box_append(GTK_BOX(v->everyone_details_box), v->keep_on_check);
  gtk_box_append(GTK_BOX(v->visibility_options_box), v->everyone_details_box);

  // Synchronize options display
  update_visibility_option_ui(v);

  gtk_box_append(GTK_BOX(content), v->visibility_options_box);

  return box;
}

static void on_settings_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewHome* v = data;
  if (v->on_settings) v->on_settings(v->on_settings_data);
}

static void on_notif_close_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  GtkWidget* box = data;
  gtk_widget_set_visible(box, FALSE);
}

static GtkWidget* build_right_column(QsViewHome* v) {
  v->right_column = gtk_box_new(GTK_ORIENTATION_VERTICAL, 32);
  gtk_widget_set_hexpand(v->right_column, TRUE);
  gtk_widget_set_margin_start(v->right_column, 32);
  gtk_widget_set_margin_end(v->right_column, 32);
  gtk_widget_set_margin_top(v->right_column, 32);
  gtk_widget_set_margin_bottom(v->right_column, 32);

  v->notif_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign(v->notif_box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(v->notif_box, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(v->notif_box, "qs-warning-banner");
  gtk_widget_set_margin_bottom(v->notif_box, 12);
  
  GtkWidget* warning_icon = gtk_image_new_from_icon_name("preferences-desktop-notification-symbolic");
  gtk_widget_add_css_class(warning_icon, "qs-icon-orange");
  gtk_box_append(GTK_BOX(v->notif_box), warning_icon);

  GtkWidget* notif_label = gtk_label_new("Turn on system notification");
  gtk_widget_add_css_class(notif_label, "qs-warning-banner-text");
  gtk_box_append(GTK_BOX(v->notif_box), notif_label);

  GtkWidget* settings_btn = gtk_button_new_with_label("Settings");
  gtk_widget_add_css_class(settings_btn, "qs-warning-banner-btn");
  g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_clicked), v);
  gtk_box_append(GTK_BOX(v->notif_box), settings_btn);

  GtkWidget* close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
  gtk_widget_add_css_class(close_btn, "flat");
  gtk_widget_add_css_class(close_btn, "circular");
  gtk_widget_set_tooltip_text(close_btn, "Dismiss warning");
  g_signal_connect(close_btn, "clicked", G_CALLBACK(on_notif_close_clicked), v->notif_box);
  gtk_box_append(GTK_BOX(v->notif_box), close_btn);

  gtk_box_append(GTK_BOX(v->right_column), v->notif_box);

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
  gtk_box_append(GTK_BOX(v->right_column), logo);

  v->drop_zone = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
  gtk_widget_add_css_class(v->drop_zone, "qs-drop-zone");
  gtk_widget_set_halign(v->drop_zone, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(v->drop_zone, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request(v->drop_zone, 380, 240);

  GtkGesture* gesture = gtk_gesture_click_new();
  g_signal_connect(gesture, "pressed", G_CALLBACK(on_drop_zone_click), v);
  gtk_widget_add_controller(v->drop_zone, GTK_EVENT_CONTROLLER(gesture));

  GtkWidget* upload_icon = gtk_image_new_from_icon_name("document-send-symbolic");
  gtk_image_set_pixel_size(GTK_IMAGE(upload_icon), 56);
  gtk_widget_add_css_class(upload_icon, "qs-drop-zone-icon");
  gtk_box_append(GTK_BOX(v->drop_zone), upload_icon);

  GtkWidget* drop_sub = gtk_label_new("Drag & Drop Files Here to Share Instantly");
  gtk_widget_add_css_class(drop_sub, "qs-drop-zone-text");
  gtk_box_append(GTK_BOX(v->drop_zone), drop_sub);

  GtkWidget* select_btn = gtk_button_new_with_label("Select File");
  gtk_widget_add_css_class(select_btn, "pill");
  gtk_widget_add_css_class(select_btn, "suggested-action");
  g_signal_connect(select_btn, "clicked", G_CALLBACK(on_select_clicked), v);
  gtk_box_append(GTK_BOX(v->drop_zone), select_btn);

  gtk_box_append(GTK_BOX(v->right_column), v->drop_zone);

  v->scanner_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
  gtk_widget_add_css_class(v->scanner_box, "qs-scanner-container");
  gtk_widget_set_halign(v->scanner_box, GTK_ALIGN_FILL);
  gtk_widget_set_valign(v->scanner_box, GTK_ALIGN_CENTER);
  gtk_widget_set_visible(v->scanner_box, FALSE);

  GtkWidget* scan_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_add_css_class(scan_header, "qs-scanner-header");
  v->spinner = gtk_spinner_new();
  gtk_box_append(GTK_BOX(scan_header), v->spinner);

  GtkWidget* scan_title = gtk_label_new("Scanning for nearby devices...");
  gtk_widget_add_css_class(scan_title, "qs-scanner-title");
  gtk_box_append(GTK_BOX(scan_header), scan_title);
  gtk_box_append(GTK_BOX(v->scanner_box), scan_header);

  v->device_list_box = GTK_LIST_BOX(gtk_list_box_new());
  gtk_widget_add_css_class(GTK_WIDGET(v->device_list_box), "boxed-list");
  gtk_list_box_set_selection_mode(v->device_list_box, GTK_SELECTION_NONE);
  g_signal_connect(v->device_list_box, "row-activated", G_CALLBACK(on_device_row_activated), v);
  gtk_box_append(GTK_BOX(v->scanner_box), GTK_WIDGET(v->device_list_box));

  GtkWidget* cancel_btn = gtk_button_new_with_label("Cancel Scan");
  gtk_widget_add_css_class(cancel_btn, "pill");
  gtk_widget_set_halign(cancel_btn, GTK_ALIGN_CENTER);
  g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_scan_clicked), v);
  gtk_box_append(GTK_BOX(v->scanner_box), cancel_btn);

  gtk_box_append(GTK_BOX(v->right_column), v->scanner_box);

  return v->right_column;
}

QsViewHome* qs_view_home_new(void) {
  QsViewHome* v = g_new0(QsViewHome, 1);

  GtkWidget* container_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_add_css_class(container_box, "qs-home-layout");

  GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_vexpand(main_box, TRUE);

  GtkWidget* left = build_left_column(v);
  GtkWidget* right = build_right_column(v);

  gtk_box_append(GTK_BOX(main_box), left);
  gtk_box_append(GTK_BOX(main_box), right);

  gtk_box_append(GTK_BOX(container_box), main_box);

  // Footer Row
  GtkWidget* footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_start(footer_box, 24);
  gtk_widget_set_margin_end(footer_box, 24);
  gtk_widget_set_margin_bottom(footer_box, 16);
  gtk_widget_set_margin_top(footer_box, 8);

  GtkWidget* brand_label = gtk_label_new("Quick Share Linux");
  gtk_widget_add_css_class(brand_label, "dim-label");
  gtk_widget_set_halign(brand_label, GTK_ALIGN_START);
  gtk_widget_set_hexpand(brand_label, TRUE);
  gtk_box_append(GTK_BOX(footer_box), brand_label);

  GtkWidget* settings_btn = gtk_button_new_from_icon_name("emblem-system-symbolic");
  gtk_widget_add_css_class(settings_btn, "flat");
  gtk_widget_add_css_class(settings_btn, "circular");
  gtk_widget_set_tooltip_text(settings_btn, "Preferences");
  gtk_widget_set_halign(settings_btn, GTK_ALIGN_END);
  g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_clicked), v);
  gtk_box_append(GTK_BOX(footer_box), settings_btn);

  gtk_box_append(GTK_BOX(container_box), footer_box);

  v->root = container_box;
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
  } else {
    gtk_label_set_text(v->state_label, "Device is hidden");
  }
  if (state->detail && *state->detail) {
    gtk_label_set_text(v->state_sub, state->detail);
  } else {
    gtk_label_set_text(v->state_sub, "Tap to receive from everyone");
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

void qs_view_home_set_on_send_file(QsViewHome* v, QsHomeSendFileCb cb, gpointer user_data) {
  v->on_send_file = cb;
  v->on_send_file_data = user_data;
}

void qs_view_home_set_on_device_selected(QsViewHome* v, QsHomeDeviceSelectedCb cb, gpointer user_data) {
  v->on_device_selected = cb;
  v->on_device_selected_data = user_data;
}

void qs_view_home_set_notifications_enabled(QsViewHome* v, gboolean enabled) {
  if (v && v->notif_box) {
    gtk_widget_set_visible(v->notif_box, !enabled);
  }
}
