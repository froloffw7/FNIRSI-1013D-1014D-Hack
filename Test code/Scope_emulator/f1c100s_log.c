//----------------------------------------------------------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "f1c100s_log.h"
//----------------------------------------------------------------------------------------------------------------------------------

static FILE *log;

//----------------------------------------------------------------------------------------------------------------------------------
static void __attribute__((constructor)) open_log()
{
	log = fopen("f1c100s.log", "w");
}

//----------------------------------------------------------------------------------------------------------------------------------
static void __attribute__((destructor)) close_log() 
{
	if (log) fclose(log);
	log = 0;
}

//----------------------------------------------------------------------------------------------------------------------------------
void f1c100s_log_mask(int lvl, const char *fmt, ...)
{
	if (log) {
		va_list args;

		va_start(args, fmt);
		if (lvl == LOG_GUEST_ERROR)
			fprintf(log, "ERR ");
		else if (lvl == LOG_GUEST_WARNING)
			fprintf(log, "WRN ");
		else if (lvl == LOG_GUEST_DEBUG)
			fprintf(log, "DBG ");
		else if (lvl == LOG_GUEST_TRACE)
			fprintf(log, "TRC ");
		else if (lvl == LOG_UNIMP)
			fprintf(log, "UNI ");
		vfprintf(log, fmt, args);
		va_end(args);
	}	
}

//----------------------------------------------------------------------------------------------------------------------------------
void f1c100s_log(const char *fmt, ...)
{
	if (log) {
		va_list args;

		va_start(args, fmt);
		vfprintf(log, fmt, args);
		va_end(args);
	}	
}

//----------------------------------------------------------------------------------------------------------------------------------
void f1c100s_log_flush()
{
	if (log) fflush(log);
}