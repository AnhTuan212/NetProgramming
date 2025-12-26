#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sqlite3.h>
#include "db_init.h"
#include "db_queries.h"
#include "db_migration.h"

#define MAX_QUESTIONS_PER_ROOM 50
#define DB_PATH "test_system.db"
#define DATA_DIR "data"

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

// Main question loading function
int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff);

// Load questions with topic and difficulty distribution filters
int loadQuestionsWithFilters(const char *filename, QItem *questions, int maxQ,
                             const char *topic_filter, const char *diff_filter);

// Enumeration functions
int get_all_topics_with_counts(char *output);
int get_all_difficulties_with_counts(char *output);

// Question validation
int validate_question_input(const QItem *q, char *error_msg);

// Question file operations
int add_question_to_file(const QItem *q);

// Question search and delete operations
int search_questions_by_id(int id, QItem *result);
int search_questions_by_topic(const char *topic, char *output);
int search_questions_by_difficulty(const char *difficulty, char *output);
int delete_question_by_id(int id);

// Deduplication & randomization
int remove_duplicate_questions(QItem *questions, int *count);
int shuffle_questions(QItem *questions, int count);

// String utilities
void to_lowercase(char *str);
void to_uppercase(char *str);

// User management
void writeLog(const char *event);
int validate_user(const char *username, const char *password, char *role_out);
int register_user_with_role(const char *username, const char *password, const char *role);

// Leaderboard
void show_leaderboard(const char *output_file);

#endif  // COMMON_H