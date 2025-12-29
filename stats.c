#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db_queries.h"

#define MAX_RECORDS 1000
#define MAX_NAME 100
#define OUTPUT_FILE "leaderboard_output.txt"  // CHO CLIENT ĐỌC

typedef struct { char username[MAX_NAME]; int total; int count; } UserStat;

void show_leaderboard(const char *output_file) {
    // Use database to get leaderboard data instead of text file
    char leaderboard[8192] = "";
    
    // Call database function to get leaderboard (uses SQL GROUP BY)
    // For now, output empty leaderboard as db_get_leaderboard needs room_id parameter
    // TODO: Modify to get leaderboard for all rooms combined or specific room
    
    FILE *out = fopen(output_file, "w");
    if (!out) return;
    fprintf(out, "=== LEADERBOARD (Avg Score) ===\n");
    fprintf(out, "Database leaderboard feature in progress\n");
    fclose(out);
}

// DÙNG TRONG SERVER
#ifdef TEST_STATS
int main() {
    show_leaderboard("leaderboard_output.txt");
    return 0;
}
#endif