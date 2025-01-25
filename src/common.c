// SPDX-FileCopyrightText: 2007-2016 The original vcontrold authors (cf. doc/original_authors.txt)
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Common functions for vcontrold like logging and converting

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>

#include "common.h"

int syslogger = 0;
int debug = 0;
FILE *logFD;
char errMsg[2000];
int errClass = 99;
int dbgFD = -1;

int initLog(int useSyslog, char *logfile, int debugSwitch)
{
    // If needed, opes the syslog or log file
    if (useSyslog) {
        syslogger = 1;
        openlog("vito", LOG_PID, LOG_LOCAL0);
        syslog(LOG_LOCAL0, "vito started");
    }

    if (logfile) {
        // Log file is not NULL and not empty
        if (strcmp(logfile, "") == 0) {
            return 0;
        }

        logFD = fopen(logfile, "a");
        if (! logFD) {
            printf("Could not open %s: %s", logfile, strerror (errno));
            return 0 ;
        }
    }

    debug = debugSwitch;
    memset(errMsg, 0, sizeof(errMsg));

    return 1;
}

void logIT (int class, char *string, ...)
{
    va_list arguments;
    time_t t;
    char *tPtr;
    char *cPtr;
    time(&t);
    tPtr = ctime(&t);
    char *print_buffer;
    int pid;
    long avail;

    va_start(arguments, string);
    vasprintf(&print_buffer, string, arguments);
    va_end(arguments);

    if (class <= LOG_ERR)  {
        avail = sizeof(errMsg) - strlen(errMsg) - 2;
        if ( avail > 0 ) {
            strncat(errMsg, print_buffer, avail);
            strcat(errMsg, "\n");
        } else {
            strcpy(&errMsg[sizeof(errMsg) - 12], "OVERFLOW\n");
            // Should solve the semop error
        }
    }

    errClass = class;
    // Remove control characters
    cPtr = tPtr;
    while (*cPtr) {
        if (iscntrl(*cPtr)) {
            *cPtr = ' ';
        }
        cPtr++;
    }

    if (dbgFD >= 0) {
        // The debug FD is set and we firstly send the info there
        dprintf(dbgFD, "DEBUG:%s: %s\n", tPtr, print_buffer);
    }

    if (! debug && (class  > LOG_NOTICE)) {
        free(print_buffer);
        return;
    }

    pid = getpid();

    if (syslogger) {
        syslog(class, "%s", print_buffer);
    }

    if (logFD) {
        fprintf(logFD, "[%d] %s: %s\n", pid, tPtr, print_buffer);
        fflush(logFD);
    }

    // Output only if 2 is open as STDERR
    if (isatty(2)) {
        fprintf(stderr, "[%d] %s: %s\n", pid, tPtr, print_buffer);
    }

    free(print_buffer);
}

void sendErrMsg(int fd)
{
    char string[256];

    if ((fd >= 0) && (errClass <= 3)) {
        snprintf(string, sizeof(string), "ERR: %s", errMsg);
        write(fd, string, strlen(string));
        errClass = 99; // Thus it's only displayed once
        memset(errMsg, 0, sizeof(errMsg));
    }

    *errMsg = '\0';
    // Back to start, no matter if we actually output
    // Can be commented out for debugging, then we get the errors in errMsg
}

void setDebugFD(int fd)
{
    dbgFD = fd;
}

char hex2chr(char *hex)
{
    char buffer[16];
    int hex_value = -1;

    snprintf(buffer, sizeof(buffer), "0x%s", hex);
    if (sscanf(hex, "%x", &hex_value) != 1) {
        logIT(LOG_WARNING, "Invalid hex char in %s", hex);
    }

    return hex_value;
}

int char2hex(char *outString, const char *charPtr, int len)
{
    int n;
    char string[MAXBUF];

    memset(string, 0, sizeof(string));
    for (n = 0; n < len; n++) {
        unsigned char byte = *charPtr++ & 255;
        snprintf(string, sizeof(string), "%02X ", byte);
        strcat(outString, string);
    }

    // Remove last space
    outString[strlen(outString) - 1] = '\0';

    return len;
}

short string2chr(char *line, char *buf, short bufsize)
{
    char *sptr = line;
    char hex[3];
    short count = 0;

    while (*sptr != '\0' && count < bufsize) {
        // Skip whitespace
        if (*sptr == ' ') {
            sptr++;
            continue;
        }

        // Ensure there are enough characters left for a hex pair
        if (sptr[0] != '\0' && sptr[1] != '\0') {
            strncpy(hex, sptr, 2);
            hex[2] = '\0';
            buf[count++] = hex2chr(hex);
            sptr += 2;  // Move to the next hex pair
        } else {
            // Not enough characters left to form a hex pair, break out
            break;
        }
    }

    return count;
}
