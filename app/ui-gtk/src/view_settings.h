#pragma once

#include <adwaita.h>

typedef struct QsViewSettings QsViewSettings;

typedef void (*QsSettingsBackCb)(gpointer user_data);

QsViewSettings* qs_view_settings_new(void);
GtkWidget* qs_view_settings_root(QsViewSettings* v);

void qs_view_settings_set_on_back(QsViewSettings* v, QsSettingsBackCb cb, gpointer user_data);

void qs_view_settings_set_device_name(QsViewSettings* v, const char* name);
const char* qs_view_settings_get_device_name(QsViewSettings* v);

void qs_view_settings_set_save_path(QsViewSettings* v, const char* path);
const char* qs_view_settings_get_save_path(QsViewSettings* v);

void qs_view_settings_set_notifications_enabled(QsViewSettings* v, gboolean enabled);
gboolean qs_view_settings_get_notifications_enabled(QsViewSettings* v);
