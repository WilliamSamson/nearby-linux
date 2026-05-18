#pragma once

#include <adwaita.h>

typedef struct QsViewSettings QsViewSettings;

typedef void (*QsSettingsBackCb)(gpointer user_data);

QsViewSettings* qs_view_settings_new(void);
GtkWidget* qs_view_settings_root(QsViewSettings* v);

void qs_view_settings_set_on_back(QsViewSettings* v, QsSettingsBackCb cb, gpointer user_data);
