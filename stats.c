#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db_queries.h"

#define MAX_RECORDS 1000
#define MAX_NAME 100

typedef struct { char username[MAX_NAME]; int total; int count; } UserStat;

void show_leaderboard(const char *output_file) {
    // Leaderboard data is now retrieved directly from database using db_get_leaderboard()
    // No longer writing to text files
    // See server.c LEADERBOARD handler for implementation
}