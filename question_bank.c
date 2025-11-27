#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < maxQ) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        // Copy line để strtok không phá
        char temp[1024];
        strcpy(temp, line);

        char *token = strtok(temp, "|");
        if (!token) continue;

        QItem q;
        q.id = atoi(token);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.text, token, sizeof(q.text)-1); q.text[sizeof(q.text)-1] = '\0';

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.A, token, sizeof(q.A)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.B, token, sizeof(q.B)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.C, token, sizeof(q.C)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.D, token, sizeof(q.D)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        q.correct = toupper(token[0]);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.topic, token, sizeof(q.topic)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.difficulty, token, sizeof(q.difficulty)-1);

        // Filter
        if (topic && topic[0] && strcasecmp(q.topic, topic) != 0) continue;
        if (diff && diff[0] && strcasecmp(q.difficulty, diff) != 0) continue;

        questions[count++] = q;
    }
    fclose(fp);
    return count;
}