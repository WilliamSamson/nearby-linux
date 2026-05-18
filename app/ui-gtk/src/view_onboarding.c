#include "views.h"

#include <adwaita.h>

struct QsViewOnboarding {
  GtkWidget* root;
  AdwViewStack* stack;

  GtkEntry* name_entry;
  GtkCheckButton* usage_check;
  GtkWidget* start_btn;
  GtkWidget* finish_btn;
  qs_receive_policy_t receive_policy;

  QsOnboardingDoneCb on_done;
  gpointer           on_done_data;
};

static void free_self(gpointer data) { g_free(data); }

static void on_start_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewOnboarding* v = data;
  adw_view_stack_set_visible_child_name(v->stack, "setup");
}

static void on_finish_clicked(GtkButton* btn G_GNUC_UNUSED, gpointer data) {
  QsViewOnboarding* v = data;
  const char* name = gtk_editable_get_text(GTK_EDITABLE(v->name_entry));
  qs_setup_config_t config = {
    .device_name = name,
    .receive_policy = v->receive_policy,
    .usage_reporting_enabled =
        v->usage_check && gtk_check_button_get_active(v->usage_check),
  };
  if (v->on_done) v->on_done(&config, v->on_done_data);
}

static GtkWidget* build_welcome_slide(QsViewOnboarding* v) {
  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start(box, 32);
  gtk_widget_set_margin_end(box, 32);

  // Icon Discovery
  GtkWidget* icon = NULL;
  const char* paths[] = {
    "app/ui-gtk/data/app-icon.png",
    "data/app-icon.png"
  };

  for (size_t i = 0; i < G_N_ELEMENTS(paths); i++) {
    if (g_file_test(paths[i], G_FILE_TEST_EXISTS)) {
      icon = gtk_image_new_from_file(paths[i]);
      g_message("Loaded icon from: %s", paths[i]);
      break;
    }
  }

  if (!icon) {
    g_message("No custom icon found, using fallback.");
    icon = gtk_image_new_from_icon_name("send-to-symbolic");
  }
  gtk_image_set_pixel_size(GTK_IMAGE(icon), 128);
  gtk_widget_add_css_class(icon, "qs-hero-icon");
  gtk_box_append(GTK_BOX(box), icon);

  GtkWidget* title = gtk_label_new("Welcome to Quick Share");
  gtk_widget_add_css_class(title, "title-1");
  gtk_label_set_xalign(GTK_LABEL(title), 0.5f);
  gtk_box_append(GTK_BOX(box), title);

  GtkWidget* sub = gtk_label_new("The easiest way to share files with nearby Android, Windows, and Linux devices.");
  gtk_widget_add_css_class(sub, "dim-label");
  gtk_label_set_wrap(GTK_LABEL(sub), TRUE);
  gtk_label_set_justify(GTK_LABEL(sub), GTK_JUSTIFY_CENTER);
  gtk_box_append(GTK_BOX(box), sub);

  GtkWidget* btn = gtk_button_new_with_label("Get Started");
  gtk_widget_add_css_class(btn, "pill");
  gtk_widget_add_css_class(btn, "suggested-action");
  gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(btn, 12);
  g_signal_connect(btn, "clicked", G_CALLBACK(on_start_clicked), v);
  gtk_box_append(GTK_BOX(box), btn);
  v->start_btn = btn;

  return box;
}

static void set_receive_policy(QsViewOnboarding* v, qs_receive_policy_t policy) {
  if (!v) return;
  v->receive_policy = policy;
  if (v->finish_btn) gtk_button_set_label(GTK_BUTTON(v->finish_btn), "Done");
}

static void on_policy_radio_toggled(GtkCheckButton* radio, gpointer data) {
  if (!gtk_check_button_get_active(radio)) return;
  set_receive_policy(data, GPOINTER_TO_INT(g_object_get_data(
                               G_OBJECT(radio), "qs-receive-policy")));
}

static void on_policy_row_activated(GtkListBoxRow* row G_GNUC_UNUSED,
                                    gpointer data) {
  gtk_check_button_set_active(GTK_CHECK_BUTTON(data), TRUE);
}

static GtkWidget* create_policy_radio(QsViewOnboarding* v,
                                      GtkCheckButton* group,
                                      qs_receive_policy_t policy,
                                      gboolean active) {
  GtkWidget* radio = gtk_check_button_new();
  gtk_widget_set_valign(radio, GTK_ALIGN_CENTER);
  if (group) gtk_check_button_set_group(GTK_CHECK_BUTTON(radio), group);
  g_object_set_data(G_OBJECT(radio), "qs-receive-policy",
                    GINT_TO_POINTER(policy));
  g_signal_connect(radio, "toggled", G_CALLBACK(on_policy_radio_toggled), v);
  gtk_check_button_set_active(GTK_CHECK_BUTTON(radio), active);
  return radio;
}

static GtkWidget* build_setup_slide(QsViewOnboarding* v) {
  GtkWidget* scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_vexpand(scroll, TRUE);

  GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 24);
  gtk_widget_set_valign(box, GTK_ALIGN_START);
  gtk_widget_set_margin_start(box, 32);
  gtk_widget_set_margin_end(box, 32);
  gtk_widget_set_margin_top(box, 32);
  gtk_widget_set_margin_bottom(box, 32);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);

  GtkWidget* title = gtk_label_new("Setup Quick Share");
  gtk_widget_add_css_class(title, "title-2");
  gtk_label_set_xalign(GTK_LABEL(title), 0.5f);
  gtk_box_append(GTK_BOX(box), title);

  GtkWidget* list = gtk_list_box_new();
  gtk_widget_add_css_class(list, "boxed-list");
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);

  GtkWidget* name_row = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(name_row), "Visible to others as");
  const char* hostname = g_get_host_name();
  gtk_editable_set_text(GTK_EDITABLE(name_row), hostname ? hostname : "Ubuntu Device");
  gtk_list_box_append(GTK_LIST_BOX(list), name_row);
  v->name_entry = GTK_ENTRY(name_row);

  gtk_box_append(GTK_BOX(box), list);

  // Sending Tile
  GtkWidget* sending_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_add_css_class(sending_box, "qs-card");
  gtk_widget_add_css_class(sending_box, "pill");
  
  GtkWidget* sending_title = gtk_label_new("Sending");
  gtk_widget_add_css_class(sending_title, "bold");
  gtk_label_set_xalign(GTK_LABEL(sending_title), 0.0f);
  gtk_box_append(GTK_BOX(sending_box), sending_title);

  GtkWidget* sending_sub = gtk_label_new("You can share with anyone nearby who has chosen to be visible");
  gtk_widget_add_css_class(sending_sub, "dim-label");
  gtk_widget_add_css_class(sending_sub, "caption");
  gtk_label_set_wrap(GTK_LABEL(sending_sub), TRUE);
  gtk_label_set_xalign(GTK_LABEL(sending_sub), 0.0f);
  gtk_box_append(GTK_BOX(sending_box), sending_sub);

  gtk_box_append(GTK_BOX(box), sending_box);

  // Receiving Section
  GtkWidget* receiving_title_label = gtk_label_new("Receiving");
  gtk_widget_add_css_class(receiving_title_label, "bold");
  gtk_label_set_xalign(GTK_LABEL(receiving_title_label), 0.0f);
  gtk_box_append(GTK_BOX(box), receiving_title_label);

  GtkWidget* receiving_sub_label = gtk_label_new("Choose who can share with you");
  gtk_widget_add_css_class(receiving_sub_label, "dim-label");
  gtk_label_set_xalign(GTK_LABEL(receiving_sub_label), 0.0f);
  gtk_box_append(GTK_BOX(box), receiving_sub_label);

  GtkWidget* rec_list = gtk_list_box_new();
  gtk_widget_add_css_class(rec_list, "boxed-list");
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(rec_list), GTK_SELECTION_NONE);

  // No one
  AdwActionRow* row_none = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_none), "No one");
  adw_action_row_set_subtitle(row_none, "Receiving turned off. No one can share with you untill you make your devices visible");
  GtkWidget* radio_none =
      create_policy_radio(v, NULL, QS_RECEIVE_NO_ONE, TRUE);
  adw_action_row_add_prefix(row_none, radio_none);
  gtk_widget_set_can_focus(radio_none, FALSE);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row_none), TRUE);
  g_signal_connect(row_none, "activated", G_CALLBACK(on_policy_row_activated),
                   radio_none);
  gtk_list_box_append(GTK_LIST_BOX(rec_list), GTK_WIDGET(row_none));

  // Everyone
  AdwExpanderRow* row_everyone = ADW_EXPANDER_ROW(adw_expander_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_everyone), "Everyone");
  adw_expander_row_set_subtitle(row_everyone, "Anyone can temporarily share with you when they are nearby. You'll be asked to approve their requests");
  
  AdwActionRow* sub_temp = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(sub_temp), "use everyone mode temporarily");
  adw_action_row_set_subtitle(sub_temp, "for your privacy , no one can share with you after a few minutes");
  gtk_widget_set_margin_start(GTK_WIDGET(sub_temp), 24); // Indent
  GtkWidget* radio_temp =
      create_policy_radio(v, GTK_CHECK_BUTTON(radio_none),
                          QS_RECEIVE_EVERYONE_TEMPORARY, FALSE);
  adw_action_row_add_prefix(sub_temp, radio_temp);
  gtk_widget_set_can_focus(radio_temp, FALSE);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(sub_temp), TRUE);
  g_signal_connect(sub_temp, "activated", G_CALLBACK(on_policy_row_activated),
                   radio_temp);
  adw_expander_row_add_row(row_everyone, GTK_WIDGET(sub_temp));

  AdwActionRow* sub_always = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(sub_always), "keep everyone mode on all the time");
  gtk_widget_set_margin_start(GTK_WIDGET(sub_always), 24); // Indent
  GtkWidget* radio_always =
      create_policy_radio(v, GTK_CHECK_BUTTON(radio_none),
                          QS_RECEIVE_EVERYONE_ALWAYS, FALSE);
  adw_action_row_add_prefix(sub_always, radio_always);
  gtk_widget_set_can_focus(radio_always, FALSE);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(sub_always), TRUE);
  g_signal_connect(sub_always, "activated", G_CALLBACK(on_policy_row_activated),
                   radio_always);
  adw_expander_row_add_row(row_everyone, GTK_WIDGET(sub_always));
  
  gtk_list_box_append(GTK_LIST_BOX(rec_list), GTK_WIDGET(row_everyone));

  // Your devices
  AdwActionRow* row_devices = ADW_ACTION_ROW(adw_action_row_new());
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row_devices), "Your devices");
  GtkWidget* radio_devices =
      create_policy_radio(v, GTK_CHECK_BUTTON(radio_none),
                          QS_RECEIVE_YOUR_DEVICES, FALSE);
  adw_action_row_add_prefix(row_devices, radio_devices);
  gtk_widget_set_can_focus(radio_devices, FALSE);
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row_devices), TRUE);
  g_signal_connect(row_devices, "activated",
                   G_CALLBACK(on_policy_row_activated), radio_devices);
  gtk_list_box_append(GTK_LIST_BOX(rec_list), GTK_WIDGET(row_devices));

  gtk_box_append(GTK_BOX(box), rec_list);

  // Help Improve Checkbox
  GtkWidget* help_check = gtk_check_button_new_with_label("Help improve this app by automatically sending usage info and crash report to developer");
  gtk_widget_add_css_class(help_check, "caption");
  gtk_widget_set_margin_top(help_check, 12);
  gtk_box_append(GTK_BOX(box), help_check);
  v->usage_check = GTK_CHECK_BUTTON(help_check);

  GtkWidget* btn = gtk_button_new_with_label("More");
  gtk_widget_add_css_class(btn, "pill");
  gtk_widget_add_css_class(btn, "suggested-action");
  gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(btn, 12);
  g_signal_connect(btn, "clicked", G_CALLBACK(on_finish_clicked), v);
  gtk_box_append(GTK_BOX(box), btn);
  v->finish_btn = btn;

  return scroll;
}

QsViewOnboarding* qs_view_onboarding_new(void) {
  QsViewOnboarding* v = g_new0(QsViewOnboarding, 1);
  v->receive_policy = QS_RECEIVE_NO_ONE;

  v->stack = ADW_VIEW_STACK(adw_view_stack_new());

  adw_view_stack_add_named(v->stack, build_welcome_slide(v), "welcome");
  adw_view_stack_add_named(v->stack, build_setup_slide(v), "setup");

  GtkWidget* clamp = adw_clamp_new();
  adw_clamp_set_maximum_size(ADW_CLAMP(clamp), 600);
  adw_clamp_set_child(ADW_CLAMP(clamp), GTK_WIDGET(v->stack));

  v->root = clamp;
  g_object_set_data_full(G_OBJECT(v->root), "qs-view-onboarding", v, free_self);
  return v;
}

GtkWidget* qs_view_onboarding_root(QsViewOnboarding* v) { return v->root; }

void qs_view_onboarding_set_on_done(QsViewOnboarding* v, QsOnboardingDoneCb cb,
                                    gpointer user_data) {
  v->on_done = cb;
  v->on_done_data = user_data;
}
