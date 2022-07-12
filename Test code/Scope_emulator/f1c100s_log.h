#define LOG_GUEST_ERROR    0
#define LOG_GUEST_WARNING  1
#define LOG_GUEST_DEBUG    2
#define LOG_GUEST_TRACE    3

#define LOG_UNIMP          9

void f1c100s_log(const char *fmt, ...);
void f1c100s_log_mask(int lvl, const char *fmt, ...);
void f1c100s_log_flush();
