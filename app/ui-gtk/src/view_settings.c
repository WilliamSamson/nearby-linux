#include "view_settings.h"
#include <adwaita.h>

struct QsViewSettings {
  GtkWidget* root;
  AdwActionRow* save_row;
  char* save_path;
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

static void on_about_activated(AdwActionRow* row G_GNUC_UNUSED,
                               gpointer data) {
  QsViewSettings* v = data;
  AdwAboutWindow* about = ADW_ABOUT_WINDOW(adw_about_window_new());
  adw_about_window_set_application_name(about, "Quick Share");
  adw_about_window_set_application_icon(about, "dev.quickshare.UbuntuShare");
  adw_about_window_set_version(about, "0.1.0");
  adw_about_window_set_developer_name(about, "Quick Share Ubuntu");
  adw_about_window_set_comments(about,
      "Share files with nearby devices using Google's Quick Share protocol.");
  adw_about_window_set_license_type(about, GTK_LICENSE_APACHE_2_0);
  GtkWindow* parent = parent_window(v);
  if (parent) gtk_window_set_transient_for(GTK_WINDOW(about), parent);
  gtk_window_present(GTK_WINDOW(about));
}

static void free_self(gpointer data) {
  QsViewSettings* v = data;
  g_free(v->save_path);
  g_free(v);
}

QsViewSettings* qs_view_settings_new(void) {
  QsViewSettings* v = g_new0(QsViewSettings, 1);

  const char* xdg_dl = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
  v->save_path = g_strdup(xdg_dl ? xdg_dl : g_get_home_dir());

  GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  // Header
  GtkWidget* header = adw_header_bar_new();
  adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(header), FALSE);
  adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(header), FALSE);
  GtkWidget* back_btn = gtk_button_new_from_icon_name("go-previous-symbolic");
  gtk_widget_add_css_class(back_btn, "flat");
  g_signal_connect(back_btn, "clicked", G_CALLBACK(on_back_clicked), v);
  adw_header_bar_pack_start(ADW_HEADER_BAR(header), back_btn);

  GtkWidget* title = gtk_label_new("Settings");
  gtk_widget_add_css_class(title, "bold");
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), title);
  gtk_box_append(GTK_BOX(main_box), header);

  // Content
  GtkWidget* page = adw_preferences_page_new();

  // ---- Group 1: device + folder -------------------------------------------
  AdwPreferencesGroup* g1 =
      ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), g1);

  AdwEntryRow* name_row = ADW_ENTRY_ROW(adw_entry_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(name_row), "Device name");
  gtk_editable_set_text(GTK_EDITABLE(name_row), g_get_host_name());
  adw_preferences_group_add(g1, GTK_WIDGET(name_row));

  v->save_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(v->save_row),
                                "Save received files to");
  adw_action_row_set_subtitle(v->save_row, v->save_path);

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
  GtkWidget* vis_chev = gtk_image_new_from_icon_name("go-next-symbolic");
  gtk_widget_add_css_class(vis_chev, "dim-label");
  adw_action_row_add_suffix(vis_row, vis_chev);
  adw_action_row_set_activatable_widget(vis_row, GTK_WIDGET(vis_chev));
  adw_preferences_group_add(g2, GTK_WIDGET(vis_row));

  AdwSwitchRow* startup_row = ADW_SWITCH_ROW(adw_switch_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(startup_row),
                                "Run automatically at startup");
  adw_preferences_group_add(g2, GTK_WIDGET(startup_row));

  // ---- Group 3: diagnostics + about ---------------------------------------
  AdwPreferencesGroup* g3 =
      ADW_PREFERENCES_GROUP(adw_preferences_group_new());
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), g3);

  AdwSwitchRow* diag_row = ADW_SWITCH_ROW(adw_switch_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(diag_row),
                                "Send usage and diagnostics data");
  adw_preferences_group_add(g3, GTK_WIDGET(diag_row));

  AdwActionRow* about_row = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(about_row),
                                "About Quick Share");
  GtkWidget* about_chev = gtk_image_new_from_icon_name("go-next-symbolic");
  gtk_widget_add_css_class(about_chev, "dim-label");
  adw_action_row_add_suffix(about_row, about_chev);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(about_row), TRUE);
  g_signal_connect(about_row, "activated",
                   G_CALLBACK(on_about_activated), v);
  adw_preferences_group_add(g3, GTK_WIDGET(about_row));

  gtk_box_append(GTK_BOX(main_box), page);

  v->root = main_box;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-settings", v, free_self);
  return v;
}

GtkWidget* qs_view_settings_root(QsViewSettings* v) { return v->root; }

void qs_view_settings_set_on_back(QsViewSettings* v, QsSettingsBackCb cb,
                                  gpointer user_data) {
  v->on_back = cb;
  v->on_back_data = user_data;
}
