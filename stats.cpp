#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_RECORDS 1000
#define MAX_NAME 100
#define DATA_DIR "data"
#define RESULT_FILE "data/results.txt"

typedef struct {
    char username[MAX_NAME];
    char room[MAX_NAME];
    int score;
} Record;

typedef struct {
    char username[MAX_NAME];
    int totalScore;
    int count;
} UserStat;


int load_results_txt(Record *records, int maxRecords) {

    FILE *fp = fopen(RESULT_FILE, "r");
    if (!fp) return 0;

    int count = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (count >= maxRecords) break;
        if (strlen(line) < 3) continue;

        char *room = strtok(line, ",");
        char *user = strtok(NULL, ",");
        char *scoreStr = strtok(NULL, ",\n");

        if (!room || !user || !scoreStr) continue;

        Record r;
        strncpy(r.room, room, MAX_NAME);
        strncpy(r.username, user, MAX_NAME);
        r.score = atoi(scoreStr);

        records[count++] = r;
    }
    fclose(fp);
    return count;
}

void show_leaderboard() {
    Record records[MAX_RECORDS];
    int total = load_results_txt(records, MAX_RECORDS);
    if (total == 0) {
        printf("No data.\n");
        return;
    }

    UserStat stats[MAX_RECORDS];
    int userCount = 0;

    // accumulate scores
    for (int i = 0; i < total; i++) {
        int found = 0;
        for (int j = 0; j < userCount; j++) {
            if (strcmp(stats[j].username, records[i].username) == 0) {
                stats[j].totalScore += records[i].score;
                stats[j].count++;
                found = 1;
                break;
            }
        }
        if (!found) {
            strcpy(stats[userCount].username, records[i].username);
            stats[userCount].totalScore = records[i].score;
            stats[userCount].count = 1;
            userCount++;
        }
    }

    // compute average
    double avgScores[MAX_RECORDS];
    for (int i = 0; i < userCount; i++) {
        avgScores[i] = (double)stats[i].totalScore / stats[i].count;
    }

    // sort descending
    for (int i = 0; i < userCount - 1; i++) {
        for (int j = i + 1; j < userCount; j++) {
            if (avgScores[j] > avgScores[i]) {
                double tmpScore = avgScores[i];
                avgScores[i] = avgScores[j];
                avgScores[j] = tmpScore;

                UserStat tmp = stats[i];
                stats[i] = stats[j];
                stats[j] = tmp;
            }
        }
    }

    printf("\n=== Leaderboard ===\n");
    for (int i = 0; i < userCount; i++) {
        printf("%-15s : %.2f\n", stats[i].username, avgScores[i]);
    }
}

#ifdef TEST_STATS
int main() {
    show_leaderboard();
    return 0;
}
#endif
