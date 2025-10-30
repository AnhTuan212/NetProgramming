#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_Q 100
#define MAX_LINE 1024

// The QItem struct is now defined here, instead of in a .h file
typedef struct {
    int id;
    char text[256];
    char A[128];
    char B[128];
    char C[128];
    char D[128];
    char correct; // 'A'..'D'
    char topic[64];
    char difficulty[32];
} QItem;


// Helper to strip "A) " labels
static void strip_label(char *str, size_t maxLen) {
    if (strlen(str) > 3 && str[1] == ')' && str[2] == ' ') {
        memmove(str, str + 3, strlen(str) - 2); // "A) Text" -> "Text"
    }
}

// Load all questions from data/questions.txt
// Format: id|question|A) opt|B) opt|C) opt|D) opt|A|Topic|Difficulty
int loadQuestionsTxt(const char *filename, QItem questions[], int maxQ, const char* topic, const char* diff) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char line[MAX_LINE];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < maxQ) {
        if (strlen(line) < 10) continue; // Need at least minimal data

        line[strcspn(line, "\n")] = 0; // Remove trailing newline

        char *parts[9];
        int i = 0;
        char *token = strtok(line, "|");
        while (token && i < 9) {
            parts[i++] = token;
            token = strtok(NULL, "|");
        }
        if (i < 9) continue; // Must have all 9 parts

        QItem q;
        q.id = atoi(parts[0]);
        strncpy(q.text, parts[1], sizeof(q.text)-1);
        strncpy(q.A, parts[2], sizeof(q.A)-1);
        strncpy(q.B, parts[3], sizeof(q.B)-1);
        strncpy(q.C, parts[4], sizeof(q.C)-1);
        strncpy(q.D, parts[5], sizeof(q.D)-1);
        q.correct = parts[6][0];
        strncpy(q.topic, parts[7], sizeof(q.topic)-1);
        strncpy(q.difficulty, parts[8], sizeof(q.difficulty)-1);

        // Null-terminate all strings
        q.text[sizeof(q.text)-1] = 0;
        q.A[sizeof(q.A)-1] = 0;
        q.B[sizeof(q.B)-1] = 0;
        q.C[sizeof(q.C)-1] = 0;
        q.D[sizeof(q.D)-1] = 0;
        q.topic[sizeof(q.topic)-1] = 0;
        q.difficulty[sizeof(q.difficulty)-1] = 0;

        // --- Filtering ---
        // If a topic filter is provided and it doesn't match, skip
        if (topic && topic[0] != '\0' && strcasecmp(q.topic, topic) != 0) {
            continue;
        }
        // If a difficulty filter is provided and it doesn't match, skip
        if (diff && diff[0] != '\0' && strcasecmp(q.difficulty, diff) != 0) {
            continue;
        }

        // Optional: strip "A) ", "B) " labels
        strip_label(q.A, sizeof(q.A));
        strip_label(q.B, sizeof(q.B));
        strip_label(q.C, sizeof(q.C));
        strip_label(q.D, sizeof(q.D));

        questions[count++] = q;
    }

    fclose(fp);
    return count;
}