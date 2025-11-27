#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_RECORDS 1000
#define MAX_NAME 100
#define RESULT_FILE "data/results.txt"
#define OUTPUT_FILE "leaderboard_output.txt"  // CHO CLIENT ĐỌC

typedef struct { char username[MAX_NAME]; char room[MAX_NAME]; int score; } Record;
typedef struct { char username[MAX_NAME]; int total; int count; } UserStat;

void show_leaderboard(const char *output_file) {
    Record recs[MAX_RECORDS];
    int n = 0;
    FILE *fp = fopen(RESULT_FILE, "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp) && n < MAX_RECORDS) {
        char room[100], user[100], score_str[20];
        if (sscanf(line, "%99[^,],%99[^,],%19s", room, user, score_str) == 3) {
            strcpy(recs[n].room, room);
            strcpy(recs[n].username, user);
            recs[n].score = atoi(score_str);
            n++;
        }
    }
    fclose(fp);

    UserStat stats[MAX_RECORDS];
    int uc = 0;
    for (int i = 0; i < n; i++) {
        int found = 0;
        for (int j = 0; j < uc; j++) {
            if (strcmp(stats[j].username, recs[i].username) == 0) {
                stats[j].total += recs[i].score;
                stats[j].count++;
                found = 1; break;
            }
        }
        if (!found && uc < MAX_RECORDS) {
            strcpy(stats[uc].username, recs[i].username);
            stats[uc].total = recs[i].score;
            stats[uc].count = 1;
            uc++;
        }
    }

    // Sort
    for (int i = 0; i < uc-1; i++) {
        for (int j = i+1; j < uc; j++) {
            double a1 = (double)stats[i].total / stats[i].count;
            double a2 = (double)stats[j].total / stats[j].count;
            if (a2 > a1) {
                UserStat tmp = stats[i]; stats[i] = stats[j]; stats[j] = tmp;
            }
        }
    }

    // GHI RA FILE
    FILE *out = fopen(output_file, "w");
    if (!out) return;
    fprintf(out, "=== LEADERBOARD (Avg Score) ===\n");
    for (int i = 0; i < uc; i++) {
        fprintf(out, "%-15s : %.2f\n", stats[i].username, (double)stats[i].total / stats[i].count);
    }
    fclose(out);
}

// DÙNG TRONG SERVER
#ifdef TEST_STATS
int main() {
    show_leaderboard("leaderboard_output.txt");
    return 0;
}
#endif