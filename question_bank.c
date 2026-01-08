#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ===== UTILITY FUNCTIONS =====

void to_lowercase(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void to_uppercase(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

// ===== ENUMERATION FUNCTIONS =====

int get_all_topics_with_counts(char *output) {
    if (!output) return -1;
    
    // Use database function db_get_all_topics instead of file I/O
    return db_get_all_topics(output);
}

int get_all_difficulties_with_counts(char *output) {
    if (!output) return -1;

    // Use database function db_get_all_difficulties instead of file I/O
    return db_get_all_difficulties(output);
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

// ===== FILE OPERATIONS =====

int add_question_to_file(const QItem *q) {
    if (!q) return -1;

    // Normalize question
    QItem new_q = *q;
    to_lowercase(new_q.topic);
    to_lowercase(new_q.difficulty);
    new_q.correct = toupper(new_q.correct);

    // Use database function instead of file I/O
    // Note: created_by should be passed as parameter (currently 0, can be updated by caller)
    int result = db_add_question(new_q.text, new_q.A, new_q.B, new_q.C, new_q.D,
                                 new_q.correct, new_q.topic, new_q.difficulty, 0);
    
    return result;  // Returns question ID on success, or negative on error
}

// ===== DEDUPLICATION & RANDOMIZATION =====

int remove_duplicate_questions(QItem *questions, int *count) {
    if (!questions || !count || *count <= 0) return 0;

    int seen_ids[200];
    memset(seen_ids, -1, sizeof(seen_ids));

    int write_idx = 0;
    int original_count = *count;

    for (int i = 0; i < original_count; i++) {
        // Check if ID already seen
        int found = 0;
        for (int j = 0; j < write_idx; j++) {
            if (seen_ids[j] == questions[i].id) {
                found = 1;
                break;
            }
        }

        if (!found) {
            seen_ids[write_idx] = questions[i].id;
            questions[write_idx] = questions[i];
            write_idx++;
        }
    }

    int removed = original_count - write_idx;
    *count = write_idx;
    return removed;
}

int shuffle_questions(QItem *questions, int count) {
    if (!questions || count <= 1) return 0;

    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        QItem temp = questions[i];
        questions[i] = questions[j];
        questions[j] = temp;
    }
    return 0;
}

// ===== MAIN LOADING FUNCTION =====
// NOTE: These functions are kept for backward compatibility but are now UNUSED
// Modern version uses database directly via db_get_questions_with_distribution()
// They are commented out to avoid compilation warnings about unused parameters

/*
int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff) {
    // Note: filename parameter ignored - we now use database as single source of truth
    if (!questions || maxQ <= 0) return 0;

    DBQuestion db_questions[maxQ];
    int count = db_get_questions_with_distribution(topic, diff, db_questions, maxQ);
    
    if (count <= 0) return 0;

    // Convert DBQuestion to QItem format
    for (int i = 0; i < count; i++) {
        questions[i].id = db_questions[i].id;
        strncpy(questions[i].text, db_questions[i].text, sizeof(questions[i].text)-1);
        strncpy(questions[i].A, db_questions[i].option_a, sizeof(questions[i].A)-1);
        strncpy(questions[i].B, db_questions[i].option_b, sizeof(questions[i].B)-1);
        strncpy(questions[i].C, db_questions[i].option_c, sizeof(questions[i].C)-1);
        strncpy(questions[i].D, db_questions[i].option_d, sizeof(questions[i].D)-1);
        questions[i].correct = db_questions[i].correct_option;
        strncpy(questions[i].topic, db_questions[i].topic, sizeof(questions[i].topic)-1);
        strncpy(questions[i].difficulty, db_questions[i].difficulty, sizeof(questions[i].difficulty)-1);
    }

    // Randomize question order
    shuffle_questions(questions, count);

    return count;
}

// Load questions with topic and difficulty distribution filters
// topic_filter format: "topic1:count1 topic2:count2 ..."
// diff_filter format: "difficulty1:count1 difficulty2:count2 ..."
int loadQuestionsWithFilters(const char *filename, QItem *questions, int maxQ,
                             const char *topic_filter, const char *diff_filter) {
    // Note: filename parameter ignored - we now use database as single source of truth
    // This function uses db_get_questions_with_distribution which handles filtering
    
    if (!questions || maxQ <= 0) return 0;

    DBQuestion db_questions[maxQ];
    int count = db_get_questions_with_distribution(topic_filter, diff_filter, db_questions, maxQ);
    
    if (count <= 0) return 0;

    // Convert DBQuestion to QItem format
    for (int i = 0; i < count; i++) {
        questions[i].id = db_questions[i].id;
        strncpy(questions[i].text, db_questions[i].text, sizeof(questions[i].text)-1);
        strncpy(questions[i].A, db_questions[i].option_a, sizeof(questions[i].A)-1);
        strncpy(questions[i].B, db_questions[i].option_b, sizeof(questions[i].B)-1);
        strncpy(questions[i].C, db_questions[i].option_c, sizeof(questions[i].C)-1);
        strncpy(questions[i].D, db_questions[i].option_d, sizeof(questions[i].D)-1);
        questions[i].correct = db_questions[i].correct_option;
        strncpy(questions[i].topic, db_questions[i].topic, sizeof(questions[i].topic)-1);
        strncpy(questions[i].difficulty, db_questions[i].difficulty, sizeof(questions[i].difficulty)-1);
    }

    // Randomize question order
    shuffle_questions(questions, count);

    return count;
}
*/

// Search questions by ID
int search_questions_by_id(int id, QItem *result) {
    // Use database function instead of file I/O
    DBQuestion db_question;
    if (!db_get_question(id, &db_question)) {
        return 0;
    }

    // Convert DBQuestion to QItem
    result->id = db_question.id;
    strncpy(result->text, db_question.text, sizeof(result->text)-1);
    strncpy(result->A, db_question.option_a, sizeof(result->A)-1);
    strncpy(result->B, db_question.option_b, sizeof(result->B)-1);
    strncpy(result->C, db_question.option_c, sizeof(result->C)-1);
    strncpy(result->D, db_question.option_d, sizeof(result->D)-1);
    result->correct = db_question.correct_option;
    strncpy(result->topic, db_question.topic, sizeof(result->topic)-1);
    strncpy(result->difficulty, db_question.difficulty, sizeof(result->difficulty)-1);
    
    return 1;
}

// Search questions by topic (returns formatted string)
int search_questions_by_topic(const char *topic, char *output) {
    // Use database function instead of file I/O
    DBQuestion questions[100];
    int count = db_get_questions_by_topic(topic, questions, 100);
    
    if (count <= 0) {
        output[0] = '\0';
        return 0;
    }

    // Format output as id|text\nid|text\n...
    output[0] = '\0';
    for (int i = 0; i < count; i++) {
        char entry[512];
        snprintf(entry, sizeof(entry), "%d|%s\n", questions[i].id, questions[i].text);
        if (strlen(output) + strlen(entry) < 8000) {
            strcat(output, entry);
        }
    }
    
    return count;
}

// Search questions by difficulty (returns formatted string)
int search_questions_by_difficulty(const char *difficulty, char *output) {
    // Use database function instead of file I/O
    DBQuestion questions[100];
    int count = db_get_questions_by_difficulty(difficulty, questions, 100);
    
    if (count <= 0) {
        output[0] = '\0';
        return 0;
    }

    // Format output as id|text\nid|text\n...
    output[0] = '\0';
    for (int i = 0; i < count; i++) {
        char entry[512];
        snprintf(entry, sizeof(entry), "%d|%s\n", questions[i].id, questions[i].text);
        if (strlen(output) + strlen(entry) < 8000) {
            strcat(output, entry);
        }
    }
    
    return count;
}

// Delete question by ID
int delete_question_by_id(int id) {
    // Use database function instead of file I/O
    return db_delete_question(id);
}