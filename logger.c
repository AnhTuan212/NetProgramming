#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define LOG_FILE "data/logs.txt"

void writeLog(const char *event) {
    if (!event) return;

    // Create data directory if needed
    mkdir("data", 0755);

    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

    fprintf(fp, "%s - %s\n", timebuf, event);
    fclose(fp);
}