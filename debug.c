/*
 * debug.c
 *
 *  Created on: 2 Sep 2020
 *      Author: mgrosvenor
 */

#define _POSIX_C_SOURCE  200809L

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>


#include "debug.h"


int ifwr_dbg_out_(
        bool info,
        ifwr_dbg_mode_e mode,
        int line_num,
        const char* filename,
        const char* function,
        const char* format, ... ) //Intentionally using char* here as these are passed in as constants
{
    va_list args;
    va_start(args,format);
    char* fn =  (char*)filename;
    char* mode_str = NULL;
    switch(mode){
        case IFWR_ERR:   mode_str = "Error"; break;
        case IFWR_FAT:   mode_str = "Fatal"; break;
        case IFWR_DBG:   mode_str = "Debug"; break;
        case IFWR_WARN:  mode_str = "Warning:"; break;
    }
    if(info) dprintf(OUTPUT_TO,"[%s - %s:%i:%s()]  ", mode_str, basename(fn), (int)line_num, function);
    int result = vdprintf(OUTPUT_TO,format,args);
    va_end(args);

    if(mode == IFWR_FAT){
        exit(-1);
    }

    return result;
}
