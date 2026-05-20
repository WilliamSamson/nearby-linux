#include "view_settings.h"
#include <adwaita.h>

struct QsViewSettings {
  GtkWidget* root;
  AdwEntryRow* name_row;
  AdwActionRow* save_row;
  char* save_path;
  AdwSwitchRow* notif_row;
  QsSettingsBackCb on_back;
  gpointer on_back_data;
};

static GtkWindow* parent_window(QsViewSettings* v) {
  GtkRoot* r = gtk_widget_get_root(v->root);
  return GTK_IS_WINDOW(r) ? GTK_WINDOW(r) : NULL;
}

static void on_back_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewSettings* v = data;
  if (v->on_back) v->on_back(v->on_back_data);
}

static void on_folder_selected(GObject* source, GAsyncResult* res,
                               gpointer data) {
  QsViewSettings* v = data;
  GError* err = NULL;
  GFile* folder =
      gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), res, &err);
  if (!folder) {
    g_clear_error(&err);
    return;
  }
  char* path = g_file_get_path(folder);
  if (path) {
    g_free(v->save_path);
    v->save_path = path;
    adw_action_row_set_subtitle(v->save_row, v->save_path);
  }
  g_object_unref(folder);
}

static void on_change_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewSettings* v = data;
  GtkFileDialog* dlg = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dlg, "Save received files to");
  if (v->save_path) {
    GFile* initial = g_file_new_for_path(v->save_path);
    gtk_file_dialog_set_initial_folder(dlg, initial);
    g_object_unref(initial);
  }
  gtk_file_dialog_select_folder(dlg, parent_window(v), NULL,
                                on_folder_selected, v);
  g_object_unref(dlg);
}

static void free_self(gpointer data) {
  QsViewSettings* v = data;
  g_free(v->save_path);
  g_free(v);
}

static GtkWidget* create_colored_icon(const char* name, const char* css_class) {
  GtkWidget* img = gtk_image_new_from_icon_name(name);
  gtk_widget_add_css_class(img, css_class);
  return img;
}

QsViewSettings* qs_view_settings_new(void) {
  QsViewSettings* v = g_new0(QsViewSettings, 1);

  const char* xdg_dl = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
  v->save_path = g_strdup(xdg_dl ? xdg_dl : g_get_home_dir());

  // Register desktop icon directory to default GtkIconTheme
  GdkDisplay* display = gdk_display_get_default();
  if (display) {
    GtkIconTheme* theme = gtk_icon_theme_get_for_display(display);
    gtk_icon_theme_add_search_path(theme, "/home/kayode-olalere/Codes/nearby-linux/app/ui-gtk/data");
    gtk_icon_theme_add_search_path(theme, "app/ui-gtk/data");
  }

  GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Simple Inline Title and Back Button
  GtkWidget* title_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_widget_set_margin_start(title_box, 24);
  gtk_widget_set_margin_end(title_box, 24);
  gtk_widget_set_margin_top(title_box, 24);
  gtk_widget_set_margin_bottom(title_box, 12);

  GtkWidget* back_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
  gtk_widget_add_css_class(back_btn, "flat");
  gtk_widget_add_css_class(back_btn, "circular");
  gtk_widget_add_css_class(back_btn, "qs-settings-back-btn");
  gtk_widget_set_valign(back_btn, GTK_ALIGN_CENTER);
  g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), v);
  gtk_box_append(GTK_BOX(title_box), back_btn);

  GtkWidget* title_label = gtk_label_new("Settings");
  gtk_widget_add_css_class(title_label, "qs-settings-title");
  gtk_widget_set_valign(title_label, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(title_box), title_label);

  gtk_box_append(GTK_BOX(main_box), title_box);

  // Content
  GtkWidget* page = adw_preferences_page_new();

  // ---- Group 1: device + folder -------------------------------------------
  AdwPreferencesGroup* g1 =
      ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), g1);

  v->name_row = ADW_ENTRY_ROW(adw_entry_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(v->name_row), "Device name");
  gtk_editable_set_text(GTK_EDITABLE(v->name_row), g_get_host_name());
  adw_entry_row_add_prefix(v->name_row, create_colored_icon("computer-symbolic", "qs-icon-blue"));
  adw_preferences_group_add(g1, GTK_WIDGET(v->name_row));

  v->save_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(v->save_row),
                                "Save received files to");
  adw_action_row_set_subtitle(v->save_row, v->save_path);
  adw_action_row_add_prefix(v->save_row, create_colored_icon("folder-download-symbolic", "qs-icon-green"));

  GtkWidget* change_btn = gtk_button_new_with_label("Change");
  gtk_widget_add_css_class(change_btn, "flat");
  gtk_widget_set_valign(change_btn, GTK_ALIGN_CENTER);
  g_signal_connect(change_btn, "clicked", G_CALLBACK(on_change_clicked), v);
  adw_action_row_add_suffix(v->save_row, change_btn);

  adw_preferences_group_add(g1, GTK_WIDGET(v->save_row));

  // ---- Group 2: visibility + startup --------------------------------------
  AdwPreferencesGroup* g2 =
      ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), g2);

  AdwActionRow* vis_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(vis_row),
                                "Device visibility");
  adw_action_row_set_subtitle(vis_row, "No one");
  adw_action_row_add_prefix(vis_row, create_colored_icon("avatar-default-symbolic", "qs-icon-orange"));
  GtkWidget* vis_chev = gtk_image_new_from_icon_name("go-next-symbolic");
  gtk_widget_add_css_class(vis_chev, "dim-label");
  adw_action_row_add_suffix(vis_row, vis_chev);
  adw_action_row_set_activatable_widget(vis_row, GTK_WIDGET(vis_chev));
  adw_preferences_group_add(g2, GTK_WIDGET(vis_row));

  AdwSwitchRow* startup_row = ADW_SWITCH_ROW(adw_switch_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(startup_row),
                                "Run automatically at startup");
  adw_action_row_add_prefix(ADW_ACTION_ROW(startup_row), create_colored_icon("system-run-symbolic", "qs-icon-violet"));
  adw_preferences_group_add(g2, GTK_WIDGET(startup_row));

  v->notif_row = ADW_SWITCH_ROW(adw_switch_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(v->notif_row),
                                "Show desktop notifications");
  adw_action_row_add_prefix(ADW_ACTION_ROW(v->notif_row), create_colored_icon("preferences-desktop-notification-symbolic", "qs-icon-yellow"));
  adw_preferences_group_add(g2, GTK_WIDGET(v->notif_row));

  // ---- Group 3: diagnostics + about ---------------------------------------
  AdwPreferencesGroup* g3 =
      ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), g3);

  AdwSwitchRow* diag_row = ADW_SWITCH_ROW(adw_switch_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(diag_row),
                                "Send usage and diagnostics data");
  adw_action_row_add_prefix(ADW_ACTION_ROW(diag_row), create_colored_icon("utilities-system-monitor-symbolic", "qs-icon-teal"));
  adw_preferences_group_add(g3, GTK_WIDGET(diag_row));

  // About Quick Share directly showing details inline with the full color desktop icon prefix
  AdwActionRow* about_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(about_row),
                                "About Quick Share");
  adw_action_row_set_subtitle(about_row,
      "Quick Share Linux v0.1.0 • Share files with nearby devices using Google's Quick Share protocol.");
  
  GtkWidget* about_img = gtk_image_new_from_icon_name("app-icon");
  gtk_image_set_pixel_size(GTK_IMAGE(about_img), 36);
  adw_action_row_add_prefix(about_row, about_img);
  adw_preferences_group_add(g3, GTK_WIDGET(about_row));

  gtk_box_append(GTK_BOX(main_box), page);

  v->root = main_box;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-settings", v, free_self);
  return v;
}

void qs_view_settings_set_device_name(QsViewSettings* v, const char* name) {
  if (v && v->name_row) {
    gtk_editable_set_text(GTK_EDITABLE(v->name_row), name ? name : "");
  }
}

const char* qs_view_settings_get_device_name(QsViewSettings* v) {
  return (v && v->name_row) ? gtk_editable_get_text(GTK_EDITABLE(v->name_row)) : "";
}

void qs_view_settings_set_save_path(QsViewSettings* v, const char* path) {
  if (!v) return;
  g_free(v->save_path);
  v->save_path = g_strdup(path);
  if (v->save_row) {
    adw_action_row_set_subtitle(v->save_row, v->save_path);
  }
}

const char* qs_view_settings_get_save_path(QsViewSettings* v) {
  return v ? v->save_path : NULL;
}

void qs_view_settings_set_notifications_enabled(QsViewSettings* v, gboolean enabled) {
  if (v && v->notif_row) {
    adw_switch_row_set_active(v->notif_row, enabled);
  }
}

gboolean qs_view_settings_get_notifications_enabled(QsViewSettings* v) {
  return (v && v->notif_row) ? adw_switch_row_get_active(v->notif_row) : FALSE;
}

GtkWidget* qs_view_settings_root(QsViewSettings* v) { return v->root; }

void qs_view_settings_set_on_back(QsViewSettings* v, QsSettingsBackCb cb,
                                  gpointer user_data) {
  v->on_back = cb;
  v->on_back_data = user_data;
}
