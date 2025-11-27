#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    int id;
    char text[256];
    char A[128];
    char B[128];
    char C[128];
    char D[128];
    char correct;
    char topic[64];
    char difficulty[32];
} QItem;

// Hàm
int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff);
void writeLog(const char *event);
int validate_user(const char *username, const char *password, char *role_out);
int register_user_with_role(const char *username, const char *password, const char *role);
void show_leaderboard(const char *output_file);  // GHI RA FILE

#endif  // ĐÚNG VỊ TRÍ