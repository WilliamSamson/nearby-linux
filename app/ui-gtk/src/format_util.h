#ifndef QUICK_SHARE_APP_UI_GTK_FORMAT_UTIL_H_
#define QUICK_SHARE_APP_UI_GTK_FORMAT_UTIL_H_

#include <stdint.h>

// Caller owns the returned string; free with g_free().
char* qs_format_bytes(uint64_t bytes);

// "X.X MB of Y.Y MB"
char* qs_format_progress_bytes(uint64_t done, uint64_t total);

// "12.3 MB/s"
char* qs_format_speed(uint64_t bytes_per_second);

// "~12 s left" / "~3 min left"
char* qs_format_eta(uint64_t seconds_remaining);

#endif  // QUICK_SHARE_APP_UI_GTK_FORMAT_UTIL_H_
