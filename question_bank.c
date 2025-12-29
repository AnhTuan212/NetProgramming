#define _GNU_SOURCE
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sqlite3.h>

// ===== GLOBAL ID TRACKING =====

static int max_question_id = 0;
static pthread_mutex_t id_lock = PTHREAD_MUTEX_INITIALIZER;
extern sqlite3 *db;

// ✅ Initialize max ID from database
void init_max_question_id() {
    sqlite3_stmt *stmt;
    const char *query = "SELECT MAX(id) FROM questions";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            max_question_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    printf("✓ Initialized question ID counter to: %d\n", max_question_id);
}

// ✅ Thread-safe ID generation
int get_next_question_id() {
    pthread_mutex_lock(&id_lock);
    int id = ++max_question_id;
    pthread_mutex_unlock(&id_lock);
    return id;
}

// ===== UTILITY FUNCTIONS =====

void to_lowercase(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

void to_uppercase(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

// ===== DATABASE QUESTION FUNCTIONS =====

// ✅ Load questions from DATABASE (not file!)
int loadQuestionsFromDB(QItem *questions, int maxQ,
                        const char *topic_filter, const char *diff_filter) {
    if (!questions || maxQ <= 0 || !db) return 0;
    
    sqlite3_stmt *stmt;
    const char *query;
    int count = 0;
    
    // ✅ FIXED: Use proper schema with FK joins to get topic/difficulty names
    if (topic_filter && diff_filter) {
        query = "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, q.correct_option, t.name, d.name "
                "FROM questions q "
                "JOIN topics t ON q.topic_id = t.id "
                "JOIN difficulties d ON q.difficulty_id = d.id "
                "WHERE LOWER(t.name) = LOWER(?) AND LOWER(d.name) = LOWER(?) ORDER BY RANDOM()";
    } else if (topic_filter) {
        query = "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, q.correct_option, t.name, d.name "
                "FROM questions q "
                "JOIN topics t ON q.topic_id = t.id "
                "JOIN difficulties d ON q.difficulty_id = d.id "
                "WHERE LOWER(t.name) = LOWER(?) ORDER BY RANDOM()";
    } else if (diff_filter) {
        query = "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, q.correct_option, t.name, d.name "
                "FROM questions q "
                "JOIN topics t ON q.topic_id = t.id "
                "JOIN difficulties d ON q.difficulty_id = d.id "
                "WHERE LOWER(d.name) = LOWER(?) ORDER BY RANDOM()";
    } else {
        query = "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, q.correct_option, t.name, d.name "
                "FROM questions q "
                "JOIN topics t ON q.topic_id = t.id "
                "JOIN difficulties d ON q.difficulty_id = d.id "
                "ORDER BY RANDOM() LIMIT ?";
    }
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    int param = 1;
    if (topic_filter && !diff_filter) {
        sqlite3_bind_text(stmt, param++, topic_filter, -1, SQLITE_STATIC);
    } else if (!topic_filter && diff_filter) {
        sqlite3_bind_text(stmt, param++, diff_filter, -1, SQLITE_STATIC);
    } else if (topic_filter && diff_filter) {
        sqlite3_bind_text(stmt, param++, topic_filter, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, param++, diff_filter, -1, SQLITE_STATIC);
    }
    
    // Only bind limit if no filters
    if (!topic_filter && !diff_filter) {
        sqlite3_bind_int(stmt, param++, maxQ);
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW && count < maxQ) {
        QItem *q = &questions[count];
        q->id = sqlite3_column_int(stmt, 0);
        strncpy(q->text, (const char*)sqlite3_column_text(stmt, 1), sizeof(q->text)-1);
        q->text[sizeof(q->text)-1] = '\0';
        strncpy(q->A, (const char*)sqlite3_column_text(stmt, 2), sizeof(q->A)-1);
        q->A[sizeof(q->A)-1] = '\0';
        strncpy(q->B, (const char*)sqlite3_column_text(stmt, 3), sizeof(q->B)-1);
        q->B[sizeof(q->B)-1] = '\0';
        strncpy(q->C, (const char*)sqlite3_column_text(stmt, 4), sizeof(q->C)-1);
        q->C[sizeof(q->C)-1] = '\0';
        strncpy(q->D, (const char*)sqlite3_column_text(stmt, 5), sizeof(q->D)-1);
        q->D[sizeof(q->D)-1] = '\0';
        q->correct = *((char*)sqlite3_column_text(stmt, 6));
        strncpy(q->topic, (const char*)sqlite3_column_text(stmt, 7), sizeof(q->topic)-1);
        q->topic[sizeof(q->topic)-1] = '\0';
        strncpy(q->difficulty, (const char*)sqlite3_column_text(stmt, 8), sizeof(q->difficulty)-1);
        q->difficulty[sizeof(q->difficulty)-1] = '\0';
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// ===== ENUMERATION FUNCTIONS =====

int get_all_topics_with_counts(char *output) {
    if (!output || !db) return -1;
    
    sqlite3_stmt *stmt;
    // Query topics with their question counts using proper schema (topic_id FK)
    const char *query = "SELECT t.name, COUNT(q.id) as count FROM topics t LEFT JOIN questions q ON t.id = q.topic_id GROUP BY t.id ORDER BY t.name";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    output[0] = '\0';
    int first = 1;
    int offset = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW && offset < 1023) {
        const char *topic_name = (const char*)sqlite3_column_text(stmt, 0);
        int count = sqlite3_column_int(stmt, 1);
        
        if (!first) {
            offset += snprintf(output + offset, 1024 - offset, "|%s:%d", topic_name, count);
        } else {
            offset += snprintf(output + offset, 1024 - offset, "%s:%d", topic_name, count);
            first = 0;
        }
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

int get_all_difficulties_with_counts(char *output) {
    if (!output || !db) return -1;
    
    sqlite3_stmt *stmt;
    // Query difficulties with their question counts using proper schema (difficulty_id FK)
    const char *query = "SELECT d.name, COUNT(q.id) as count FROM difficulties d LEFT JOIN questions q ON d.id = q.difficulty_id GROUP BY d.id ORDER BY d.level";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    int easy_count = 0, medium_count = 0, hard_count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *diff = (const char*)sqlite3_column_text(stmt, 0);
        int count = sqlite3_column_int(stmt, 1);
        
        if (strcasecmp(diff, "easy") == 0) easy_count = count;
        else if (strcasecmp(diff, "medium") == 0) medium_count = count;
        else if (strcasecmp(diff, "hard") == 0) hard_count = count;
    }
    
    sqlite3_finalize(stmt);
    
    snprintf(output, 256, "easy:%d|medium:%d|hard:%d", easy_count, medium_count, hard_count);
    return 0;
}

// ✅ NEW: Get difficulties with counts for SPECIFIC TOPICS ONLY
int get_difficulties_for_topics(const char *topic_filter, char *output) {
    if (!output || !topic_filter || !db) return -1;
    
    sqlite3_stmt *stmt;
    // Build IN clause from topic_filter (format: "cloud:1 database:1 ...")
    char topics_list[512] = "";
    char *filter_copy = strdup(topic_filter);
    char *token = strtok(filter_copy, " ");
    
    int first = 1;
    while (token) {
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';  // Extract topic name
            
            // Build this topic entry
            char entry[128];
            if (first) {
                snprintf(entry, sizeof(entry), "'%s'", token);
                first = 0;
            } else {
                snprintf(entry, sizeof(entry), ",'%s'", token);
            }
            
            // Check if we have enough space
            if (strlen(topics_list) + strlen(entry) < sizeof(topics_list)) {
                strcat(topics_list, entry);
            } else {
                fprintf(stderr, "Warning: topics_list buffer overflow\n");
                break;
            }
        }
        token = strtok(NULL, " ");
    }
    free(filter_copy);
    
    if (strlen(topics_list) == 0) {
        strcpy(output, "easy:0|medium:0|hard:0");
        return 0;
    }
    
    // Query: Get difficulty counts for questions in selected topics
    // Use ON clause (not WHERE) with LEFT JOIN to keep NULL rows
    char query[2048];
    int len = snprintf(query, sizeof(query), 
                       "SELECT d.name, COUNT(q.id) as count FROM difficulties d "
                       "LEFT JOIN questions q ON d.id = q.difficulty_id AND q.topic_id IN (SELECT id FROM topics WHERE name IN (%s)) "
                       "GROUP BY d.id ORDER BY d.level", topics_list);
    
    if (len < 0 || len >= (int)sizeof(query)) {
        fprintf(stderr, "Query too long\n");
        strcpy(output, "easy:0|medium:0|hard:0");
        return -1;
    }
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(db));
        strcpy(output, "easy:0|medium:0|hard:0");
        return -1;
    }
    
    int easy_count = 0, medium_count = 0, hard_count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *diff = (const char*)sqlite3_column_text(stmt, 0);
        int count = sqlite3_column_int(stmt, 1);
        
        fprintf(stderr, "[DEBUG get_difficulties_for_topics] Difficulty: %s, Count: %d\n", diff, count);
        
        if (strcasecmp(diff, "easy") == 0) easy_count = count;
        else if (strcasecmp(diff, "medium") == 0) medium_count = count;
        else if (strcasecmp(diff, "hard") == 0) hard_count = count;
    }
    
    sqlite3_finalize(stmt);
    
    snprintf(output, 256, "easy:%d|medium:%d|hard:%d", easy_count, medium_count, hard_count);
    fprintf(stderr, "[DEBUG get_difficulties_for_topics] Final output: %s\n", output);
    return 0;
}

// ===== QUESTION VALIDATION =====

int validate_question_input(const QItem *q, char *error_msg) {
    if (!q || !error_msg) return 0;

    if (strlen(q->text) < 10) {
        strcpy(error_msg, "ERROR: Question text too short (min 10 chars)");
        return 0;
    }

    if (strlen(q->A) == 0) {
        strcpy(error_msg, "ERROR: Option A is empty");
        return 0;
    }
    if (strlen(q->B) == 0) {
        strcpy(error_msg, "ERROR: Option B is empty");
        return 0;
    }
    if (strlen(q->C) == 0) {
        strcpy(error_msg, "ERROR: Option C is empty");
        return 0;
    }
    if (strlen(q->D) == 0) {
        strcpy(error_msg, "ERROR: Option D is empty");
        return 0;
    }

    char correct_upper = toupper(q->correct);
    if (correct_upper != 'A' && correct_upper != 'B' && correct_upper != 'C' && correct_upper != 'D') {
        strcpy(error_msg, "ERROR: Correct answer must be A, B, C, or D");
        return 0;
    }

    if (strlen(q->topic) == 0) {
        strcpy(error_msg, "ERROR: Topic cannot be empty");
        return 0;
    }

    char diff_lower[32];
    strcpy(diff_lower, q->difficulty);
    to_lowercase(diff_lower);

    if (strcmp(diff_lower, "easy") != 0 && strcmp(diff_lower, "medium") != 0 && strcmp(diff_lower, "hard") != 0) {
        strcpy(error_msg, "ERROR: Difficulty must be: easy, medium, or hard");
        return 0;
    }

    return 1;
}

// ===== FILE OPERATIONS (for backward compatibility) =====

// ✅ Keep for migration only
int add_question_to_file(const QItem *q) {
    if (!q || !db) return -1;
    
    char topic_lower[64], diff_lower[32];
    strcpy(topic_lower, q->topic);
    strcpy(diff_lower, q->difficulty);
    to_lowercase(topic_lower);
    to_lowercase(diff_lower);

    return db_add_question(q->text, q->A, q->B, q->C, q->D, 
                          q->correct, topic_lower, diff_lower, -1);
}

int delete_question_by_id(int id) {
    if (!db) return 0;
    
    sqlite3_stmt *stmt;
    const char *query = "DELETE FROM questions WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 1 : 0;
}

// ✅ Keep for backward compatibility but read from DB
int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff) {
    (void)filename; // Not used - read from DB instead
    return loadQuestionsFromDB(questions, maxQ, topic, diff);
}

int loadQuestionsWithFilters(const char *filename, QItem *questions, int maxQ,
                             const char *topic_filter, const char *diff_filter) {
    (void)filename; // Not used - read from DB instead
    return loadQuestionsFromDB(questions, maxQ, topic_filter, diff_filter);
}

// ===== Load questions with multiple topic/difficulty filters =====
// Handles format like: "database:2 networking:2" and "easy:3 medium:1"
int loadQuestionsWithMultipleFilters(QItem *questions, int maxQ,
                                      const char *topic_filter, const char *diff_filter) {
    if (!questions || maxQ <= 0 || !db) return 0;
    
    sqlite3_stmt *stmt;
    int count = 0;
    
    // Build topic list
    char topic_in[512] = "";
    if (topic_filter && strlen(topic_filter) > 0) {
        char topic_copy[512];
        strcpy(topic_copy, topic_filter);
        
        char *saveptr = NULL;
        char *token = strtok_r(topic_copy, " ", &saveptr);
        int first = 1;
        
        while (token && strlen(topic_in) < sizeof(topic_in) - 10) {
            char *colon = strchr(token, ':');
            if (colon) {
                int len = colon - token;
                char topic_name[64];
                strncpy(topic_name, token, len);
                topic_name[len] = '\0';
                
                if (!first) {
                    strncat(topic_in, ",'", sizeof(topic_in) - strlen(topic_in) - 1);
                } else {
                    strncat(topic_in, "'", sizeof(topic_in) - strlen(topic_in) - 1);
                }
                strncat(topic_in, topic_name, sizeof(topic_in) - strlen(topic_in) - 1);
                strncat(topic_in, "'", sizeof(topic_in) - strlen(topic_in) - 1);
                first = 0;
            }
            token = strtok_r(NULL, " ", &saveptr);
        }
    }
    
    // Build difficulty list
    char diff_in[512] = "";
    if (diff_filter && strlen(diff_filter) > 0) {
        char diff_copy[512];
        strcpy(diff_copy, diff_filter);
        
        char *saveptr = NULL;
        char *token = strtok_r(diff_copy, " ", &saveptr);
        int first = 1;
        
        while (token && strlen(diff_in) < sizeof(diff_in) - 10) {
            char *colon = strchr(token, ':');
            if (colon) {
                int len = colon - token;
                char diff_name[64];
                strncpy(diff_name, token, len);
                diff_name[len] = '\0';
                
                if (!first) {
                    strncat(diff_in, ",'", sizeof(diff_in) - strlen(diff_in) - 1);
                } else {
                    strncat(diff_in, "'", sizeof(diff_in) - strlen(diff_in) - 1);
                }
                strncat(diff_in, diff_name, sizeof(diff_in) - strlen(diff_in) - 1);
                strncat(diff_in, "'", sizeof(diff_in) - strlen(diff_in) - 1);
                first = 0;
            }
            token = strtok_r(NULL, " ", &saveptr);
        }
    }
    
    // Build query safely using snprintf
    char query[2048];
    int qlen = snprintf(query, sizeof(query), 
                       "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, q.correct_option, t.name, d.name "
                       "FROM questions q "
                       "JOIN topics t ON q.topic_id = t.id "
                       "JOIN difficulties d ON q.difficulty_id = d.id WHERE 1=1");
    
    if (strlen(topic_in) > 0) {
        qlen += snprintf(query + qlen, sizeof(query) - qlen,
                        " AND LOWER(t.name) IN (%s)", topic_in);
    }
    
    if (strlen(diff_in) > 0) {
        qlen += snprintf(query + qlen, sizeof(query) - qlen,
                        " AND LOWER(d.name) IN (%s)", diff_in);
    }
    
    snprintf(query + qlen, sizeof(query) - qlen, " ORDER BY RANDOM()");
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Query error: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW && count < maxQ) {
        QItem *q = &questions[count];
        q->id = sqlite3_column_int(stmt, 0);
        strncpy(q->text, (const char*)sqlite3_column_text(stmt, 1), sizeof(q->text)-1);
        q->text[sizeof(q->text)-1] = '\0';
        strncpy(q->A, (const char*)sqlite3_column_text(stmt, 2), sizeof(q->A)-1);
        q->A[sizeof(q->A)-1] = '\0';
        strncpy(q->B, (const char*)sqlite3_column_text(stmt, 3), sizeof(q->B)-1);
        q->B[sizeof(q->B)-1] = '\0';
        strncpy(q->C, (const char*)sqlite3_column_text(stmt, 4), sizeof(q->C)-1);
        q->C[sizeof(q->C)-1] = '\0';
        strncpy(q->D, (const char*)sqlite3_column_text(stmt, 5), sizeof(q->D)-1);
        q->D[sizeof(q->D)-1] = '\0';
        q->correct = *((char*)sqlite3_column_text(stmt, 6));
        strncpy(q->topic, (const char*)sqlite3_column_text(stmt, 7), sizeof(q->topic)-1);
        q->topic[sizeof(q->topic)-1] = '\0';
        strncpy(q->difficulty, (const char*)sqlite3_column_text(stmt, 8), sizeof(q->difficulty)-1);
        q->difficulty[sizeof(q->difficulty)-1] = '\0';
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// ===== Shuffle function =====
int shuffle_questions(QItem *questions, int count) {
    if (!questions || count <= 1) return count;
    
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        QItem temp = questions[i];
        questions[i] = questions[j];
        questions[j] = temp;
    }
    return count;
}

// ===== Deduplication =====
int remove_duplicate_questions(QItem *questions, int *count) {
    if (!questions || !count || *count <= 1) return *count;
    
    int new_count = 0;
    for (int i = 0; i < *count; i++) {
        int found = 0;
        for (int j = 0; j < new_count; j++) {
            if (questions[i].id == questions[j].id) {
                found = 1;
                break;
            }
        }
        if (!found) {
            questions[new_count++] = questions[i];
        }
    }
    *count = new_count;
    return new_count;
}