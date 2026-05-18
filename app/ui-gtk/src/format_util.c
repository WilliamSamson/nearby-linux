#include "format_util.h"

#include <glib.h>
#include <stdio.h>

char* qs_format_bytes(uint64_t bytes) {
  if (bytes < 1024) {
    return g_strdup_printf("%lu B", bytes);
  } else if (bytes < 1024 * 1024) {
    return g_strdup_printf("%.1f KB", bytes / 1024.0);
  } else if (bytes < 1024 * 1024 * 1024) {
    return g_strdup_printf("%.1f MB", bytes / (1024.0 * 1024.0));
  } else {
    return g_strdup_printf("%.1f GB", bytes / (1024.0 * 1024.0 * 1024.0));
  }
}

char* qs_format_progress_bytes(uint64_t done, uint64_t total) {
  char* s_done = qs_format_bytes(done);
  char* s_total = qs_format_bytes(total);
  char* res = g_strdup_printf("%s of %s", s_done, s_total);
  g_free(s_done);
  g_free(s_total);
  return res;
}

char* qs_format_speed(uint64_t bps) {
  char* s = qs_format_bytes(bps);
  char* res = g_strdup_printf("%s/s", s);
  g_free(s);
  return res;
}

char* qs_format_eta(uint64_t sec) {
  if (sec == 0) return g_strdup("Completing...");
  if (sec < 60) return g_strdup_printf("~%lu s left", sec);
  return g_strdup_printf("~%lu min left", (sec + 30) / 60);
}
