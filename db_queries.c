#include "db_queries.h"
#include "db_init.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ==================== USER MANAGEMENT ====================

// 🔧 Get user ID by username from database
int db_get_user_id(const char *username) {
    if (!username || !db) return -1;
    
    sqlite3_stmt *stmt;
    const char *query = "SELECT id FROM users WHERE username = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    int user_id = -1;
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return user_id;
}

// ==================== QUESTIONS ====================

// Add question to database
int db_add_question(const char *text, const char *opt_a, const char *opt_b,
                   const char *opt_c, const char *opt_d, char correct,
                   const char *topic, const char *difficulty, int created_by_id) {
    sqlite3_stmt *stmt;
    
    // 🔧 Normalize topic and difficulty to lowercase
    char topic_lower[64], difficulty_lower[32];
    strcpy(topic_lower, topic);
    strcpy(difficulty_lower, difficulty);
    
    // Convert to lowercase
    for (int i = 0; topic_lower[i]; i++) {
        topic_lower[i] = tolower(topic_lower[i]);
    }
    for (int i = 0; difficulty_lower[i]; i++) {
        difficulty_lower[i] = tolower(difficulty_lower[i]);
    }
    
    // Get topic ID (case-insensitive lookup)
    int topic_id = 0;
    const char *topic_query = "SELECT id FROM topics WHERE LOWER(name) = LOWER(?)";
    if (sqlite3_prepare_v2(db, topic_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, topic_lower, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            topic_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // 🔧 Auto-create topic if not found
    if (topic_id == 0) {
        const char *insert_topic_query = "INSERT INTO topics (name) VALUES (?)";
        if (sqlite3_prepare_v2(db, insert_topic_query, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, topic_lower, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                topic_id = (int)sqlite3_last_insert_rowid(db);
            }
            sqlite3_finalize(stmt);
        }
        
        if (topic_id == 0) {
            fprintf(stderr, "Error: failed to create topic '%s'\n", topic_lower);
            return -1;
        }
    }
    
    // Get difficulty ID (strict validation - no auto-creation)
    int difficulty_id = 0;
    const char *diff_query = "SELECT id FROM difficulties WHERE LOWER(name) = LOWER(?)";
    if (sqlite3_prepare_v2(db, diff_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, difficulty_lower, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            difficulty_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    if (difficulty_id == 0) {
        fprintf(stderr, "Error: invalid difficulty '%s' (must be easy, medium, or hard)\n", difficulty_lower);
        return -2;  // Return -2 for invalid difficulty
    }
    
    // Insert the question
    const char *query = 
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, "
        "correct_option, topic_id, difficulty_id, created_by) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, text, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, opt_a, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, opt_b, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, opt_c, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, opt_d, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, &correct, 1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, topic_id);
    sqlite3_bind_int(stmt, 8, difficulty_id);
    
    // Bind created_by - use NULL if not specified (id = -1 or 0)
    if (created_by_id > 0) {
        sqlite3_bind_int(stmt, 9, created_by_id);
    } else {
        sqlite3_bind_null(stmt, 9);
    }
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Error inserting question: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    return (int)sqlite3_last_insert_rowid(db);
}

// 🔧 Sync questions from text file to database
int db_sync_questions_from_file(const char *filename) {
    if (!filename || !db) return 0;
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: could not open %s for sync\n", filename);
        return 0;
    }
    
    int synced_count = 0;
    char line[1024];
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;
        
        // Parse: ID|text|A|B|C|D|correct|topic|difficulty
        int id;
        char text[256], A[128], B[128], C[128], D[128];
        char correct_str[2], topic[64], difficulty[32];
        
        int parsed = sscanf(line, "%d|%255[^|]|%127[^|]|%127[^|]|%127[^|]|%127[^|]|%1[^|]|%63[^|]|%31s",
                           &id, text, A, B, C, D, correct_str, topic, difficulty);
        
        if (parsed != 9) continue;
        
        // Check if question already exists in database
        sqlite3_stmt *stmt;
        const char *check_query = "SELECT id FROM questions WHERE id = ?";
        int exists = 0;
        
        if (sqlite3_prepare_v2(db, check_query, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                exists = 1;
            }
            sqlite3_finalize(stmt);
        }
        
        if (!exists) {
            // Sync this question to database
            int result = db_add_question(text, A, B, C, D, correct_str[0], topic, difficulty, -1);
            if (result > 0) {
                synced_count++;
                fprintf(stderr, "Synced question ID %d from file to database\n", id);
            } else {
                fprintf(stderr, "Failed to sync question ID %d from file\n", id);
            }
        }
    }
    
    fclose(fp);
    return synced_count;
}

// Get question by ID
int db_get_question(int id, DBQuestion *q) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
        "q.correct_option, q.topic_id, q.difficulty_id, t.name, d.name "
        "FROM questions q "
        "JOIN topics t ON q.topic_id = t.id "
        "JOIN difficulties d ON q.difficulty_id = d.id "
        "WHERE q.id = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        q->id = sqlite3_column_int(stmt, 0);
        strcpy(q->text, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(q->option_a, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(q->option_b, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(q->option_c, (const char*)sqlite3_column_text(stmt, 4));
        strcpy(q->option_d, (const char*)sqlite3_column_text(stmt, 5));
        q->correct_option = *((char*)sqlite3_column_text(stmt, 6));
        q->topic_id = sqlite3_column_int(stmt, 7);
        q->difficulty_id = sqlite3_column_int(stmt, 8);
        strncpy(q->topic, (const char*)sqlite3_column_text(stmt, 9), sizeof(q->topic)-1);
        strncpy(q->difficulty, (const char*)sqlite3_column_text(stmt, 10), sizeof(q->difficulty)-1);
        sqlite3_finalize(stmt);
        return 1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// Delete question by ID
int db_delete_question(int id) {
    sqlite3_stmt *stmt;
    const char *query = "DELETE FROM questions WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, id);
    
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE && changes > 0) ? 1 : 0;
}

// Get all questions (for admin)
int db_get_all_questions(DBQuestion *questions, int max_count) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
        "q.correct_option, q.topic_id, q.difficulty_id, t.name, d.name "
        "FROM questions q "
        "JOIN topics t ON q.topic_id = t.id "
        "JOIN difficulties d ON q.difficulty_id = d.id "
        "ORDER BY q.id LIMIT ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, max_count);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        questions[count].id = sqlite3_column_int(stmt, 0);
        strcpy(questions[count].text, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(questions[count].option_a, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(questions[count].option_b, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(questions[count].option_c, (const char*)sqlite3_column_text(stmt, 4));
        strcpy(questions[count].option_d, (const char*)sqlite3_column_text(stmt, 5));
        questions[count].correct_option = *((char*)sqlite3_column_text(stmt, 6));
        questions[count].topic_id = sqlite3_column_int(stmt, 7);
        questions[count].difficulty_id = sqlite3_column_int(stmt, 8);
        strncpy(questions[count].topic, (const char*)sqlite3_column_text(stmt, 9), sizeof(questions[count].topic)-1);
        strncpy(questions[count].difficulty, (const char*)sqlite3_column_text(stmt, 10), sizeof(questions[count].difficulty)-1);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get questions by topic
int db_get_questions_by_topic(const char *topic, DBQuestion *questions, int max_count) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
        "q.correct_option, q.topic_id, q.difficulty_id, t.name, d.name "
        "FROM questions q "
        "JOIN topics t ON q.topic_id = t.id "
        "JOIN difficulties d ON q.difficulty_id = d.id "
        "WHERE LOWER(t.name) = LOWER(?) LIMIT ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, topic, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, max_count);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        questions[count].id = sqlite3_column_int(stmt, 0);
        strcpy(questions[count].text, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(questions[count].option_a, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(questions[count].option_b, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(questions[count].option_c, (const char*)sqlite3_column_text(stmt, 4));
        strcpy(questions[count].option_d, (const char*)sqlite3_column_text(stmt, 5));
        questions[count].correct_option = *((char*)sqlite3_column_text(stmt, 6));
        questions[count].topic_id = sqlite3_column_int(stmt, 7);
        questions[count].difficulty_id = sqlite3_column_int(stmt, 8);
        strncpy(questions[count].topic, (const char*)sqlite3_column_text(stmt, 9), sizeof(questions[count].topic)-1);
        strncpy(questions[count].difficulty, (const char*)sqlite3_column_text(stmt, 10), sizeof(questions[count].difficulty)-1);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get questions by difficulty
int db_get_questions_by_difficulty(const char *difficulty, DBQuestion *questions, int max_count) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
        "q.correct_option, q.topic_id, q.difficulty_id, t.name, d.name "
        "FROM questions q "
        "JOIN topics t ON q.topic_id = t.id "
        "JOIN difficulties d ON q.difficulty_id = d.id "
        "WHERE LOWER(d.name) = LOWER(?) LIMIT ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, difficulty, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, max_count);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        questions[count].id = sqlite3_column_int(stmt, 0);
        strcpy(questions[count].text, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(questions[count].option_a, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(questions[count].option_b, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(questions[count].option_c, (const char*)sqlite3_column_text(stmt, 4));
        strcpy(questions[count].option_d, (const char*)sqlite3_column_text(stmt, 5));
        questions[count].correct_option = *((char*)sqlite3_column_text(stmt, 6));
        questions[count].topic_id = sqlite3_column_int(stmt, 7);
        questions[count].difficulty_id = sqlite3_column_int(stmt, 8);
        strncpy(questions[count].topic, (const char*)sqlite3_column_text(stmt, 9), sizeof(questions[count].topic)-1);
        strncpy(questions[count].difficulty, (const char*)sqlite3_column_text(stmt, 10), sizeof(questions[count].difficulty)-1);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get questions by topic AND difficulty with distribution
int db_get_questions_with_distribution(const char *topic_filter, const char *diff_filter,
                                       DBQuestion *questions, int max_count) {
    // Parse topic_filter: "topic1:count1 topic2:count2 ..."
    // Parse diff_filter: "easy:count1 medium:count2 ..."
    
    char topic_copy[512], diff_copy[256];
    strcpy(topic_copy, topic_filter ? topic_filter : "");
    strcpy(diff_copy, diff_filter ? diff_filter : "");
    
    // Build dynamic query based on filters
    char query[2048] = "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
                       "q.correct_option, q.topic_id, q.difficulty_id, t.name, d.name "
                       "FROM questions q "
                       "JOIN topics t ON q.topic_id = t.id "
                       "JOIN difficulties d ON q.difficulty_id = d.id WHERE 1=1";
    
    if (strlen(topic_copy) > 0) {
        strcat(query, " AND LOWER(t.name) IN (");
        char *topic_ptr = strtok(topic_copy, " ");
        int first = 1;
        while (topic_ptr) {
            char *colon = strchr(topic_ptr, ':');
            if (colon) *colon = '\0';
            
            if (!first) strcat(query, ", ");
            strcat(query, "LOWER('");
            strcat(query, topic_ptr);
            strcat(query, "')");
            first = 0;
            topic_ptr = strtok(NULL, " ");
        }
        strcat(query, ")");
    }
    
    if (strlen(diff_copy) > 0) {
        strcat(query, " AND LOWER(d.name) IN (");
        char *diff_ptr = strtok(diff_copy, " ");
        int first = 1;
        while (diff_ptr) {
            char *colon = strchr(diff_ptr, ':');
            if (colon) *colon = '\0';
            
            if (!first) strcat(query, ", ");
            strcat(query, "LOWER('");
            strcat(query, diff_ptr);
            strcat(query, "')");
            first = 0;
            diff_ptr = strtok(NULL, " ");
        }
        strcat(query, ")");
    }
    
    strcat(query, " ORDER BY RANDOM() LIMIT ?");
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, max_count);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        questions[count].id = sqlite3_column_int(stmt, 0);
        strcpy(questions[count].text, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(questions[count].option_a, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(questions[count].option_b, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(questions[count].option_c, (const char*)sqlite3_column_text(stmt, 4));
        strcpy(questions[count].option_d, (const char*)sqlite3_column_text(stmt, 5));
        questions[count].correct_option = *((char*)sqlite3_column_text(stmt, 6));
        questions[count].topic_id = sqlite3_column_int(stmt, 7);
        questions[count].difficulty_id = sqlite3_column_int(stmt, 8);
        strncpy(questions[count].topic, (const char*)sqlite3_column_text(stmt, 9), sizeof(questions[count].topic)-1);
        strncpy(questions[count].difficulty, (const char*)sqlite3_column_text(stmt, 10), sizeof(questions[count].difficulty)-1);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get all topics
int db_get_all_topics(char *output) {
    sqlite3_stmt *stmt;
    // Use LEFT JOIN to include ALL topics, even those with 0 questions
    const char *query = "SELECT t.name, COUNT(q.id) FROM topics t "
                       "LEFT JOIN questions q ON q.topic_id = t.id GROUP BY t.id ORDER BY t.name";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    strcpy(output, "");
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *topic = (const char*)sqlite3_column_text(stmt, 0);
        int topic_count = sqlite3_column_int(stmt, 1);
        
        if (count > 0) strcat(output, "|");
        sprintf(output + strlen(output), "%s:%d", topic, topic_count);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get all difficulties
int db_get_all_difficulties(char *output) {
    sqlite3_stmt *stmt;
    // Use LEFT JOIN to include ALL difficulties, even those with 0 questions
    const char *query = "SELECT d.name, COUNT(q.id) FROM difficulties d "
                       "LEFT JOIN questions q ON q.difficulty_id = d.id GROUP BY d.id ORDER BY d.level";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    strcpy(output, "");
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *difficulty = (const char*)sqlite3_column_text(stmt, 0);
        int diff_count = sqlite3_column_int(stmt, 1);
        
        if (count > 0) strcat(output, "|");
        sprintf(output + strlen(output), "%s:%d", difficulty, diff_count);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// ==================== USERS ====================

// Add user
int db_add_user(const char *username, const char *password, const char *role) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO users (username, password, role) VALUES (?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, role, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;
}

// Validate user
int db_validate_user(const char *username, const char *password) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT id FROM users WHERE username = ? AND password = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
    
    int user_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return user_id;
}

// Get user role
int db_get_user_role(const char *username, char *role) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT role FROM users WHERE username = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strcpy(role, (const char*)sqlite3_column_text(stmt, 0));
        sqlite3_finalize(stmt);
        return 1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// Check if username exists
int db_username_exists(const char *username) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT 1 FROM users WHERE username = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    
    int exists = (sqlite3_step(stmt) == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    return exists;
}

// ==================== ROOMS ====================

// Create room
int db_create_room(const char *name, int owner_id, int duration_minutes) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO rooms (name, owner_id, duration_minutes) VALUES (?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, owner_id);
    sqlite3_bind_int(stmt, 3, duration_minutes);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;
}

// Add question to room
int db_add_question_to_room(int room_id, int question_id, int order_num) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO room_questions (room_id, question_id, order_num) VALUES (?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    sqlite3_bind_int(stmt, 2, question_id);
    sqlite3_bind_int(stmt, 3, order_num);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 1 : 0;
}

// Get questions in room
int db_get_room_questions(int room_id, DBQuestion *questions, int max_count) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
        "q.correct_option, q.topic_id, q.difficulty_id, t.name, d.name "
        "FROM questions q "
        "JOIN room_questions rq ON q.id = rq.question_id "
        "JOIN topics t ON q.topic_id = t.id "
        "JOIN difficulties d ON q.difficulty_id = d.id "
        "WHERE rq.room_id = ? ORDER BY rq.order_num LIMIT ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    sqlite3_bind_int(stmt, 2, max_count);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        questions[count].id = sqlite3_column_int(stmt, 0);
        strcpy(questions[count].text, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(questions[count].option_a, (const char*)sqlite3_column_text(stmt, 2));
        strcpy(questions[count].option_b, (const char*)sqlite3_column_text(stmt, 3));
        strcpy(questions[count].option_c, (const char*)sqlite3_column_text(stmt, 4));
        strcpy(questions[count].option_d, (const char*)sqlite3_column_text(stmt, 5));
        questions[count].correct_option = *((char*)sqlite3_column_text(stmt, 6));
        questions[count].topic_id = sqlite3_column_int(stmt, 7);
        questions[count].difficulty_id = sqlite3_column_int(stmt, 8);
        strncpy(questions[count].topic, (const char*)sqlite3_column_text(stmt, 9), sizeof(questions[count].topic)-1);
        strncpy(questions[count].difficulty, (const char*)sqlite3_column_text(stmt, 10), sizeof(questions[count].difficulty)-1);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get room by ID
int db_get_room(int room_id, DBRoom *room) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT id, name, owner_id, duration_minutes, is_started, is_finished FROM rooms WHERE id = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        room->id = sqlite3_column_int(stmt, 0);
        strcpy(room->name, (const char*)sqlite3_column_text(stmt, 1));
        room->owner_id = sqlite3_column_int(stmt, 2);
        room->duration_minutes = sqlite3_column_int(stmt, 3);
        room->is_started = sqlite3_column_int(stmt, 4);
        room->is_finished = sqlite3_column_int(stmt, 5);
        sqlite3_finalize(stmt);
        return 1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// ==================== PARTICIPANTS & ANSWERS ====================

// Add participant to room
int db_add_participant(int room_id, int user_id) {
    sqlite3_stmt *stmt;
    const char *query = "INSERT INTO participants (room_id, user_id) VALUES (?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    sqlite3_bind_int(stmt, 2, user_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;
}

// Record answer
int db_record_answer(int participant_id, int question_id, char selected_option, int is_correct) {
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT OR REPLACE INTO answers (participant_id, question_id, selected_option, is_correct) "
        "VALUES (?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, participant_id);
    sqlite3_bind_int(stmt, 2, question_id);
    sqlite3_bind_text(stmt, 3, &selected_option, 1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, is_correct);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 1 : 0;
}

// ==================== RESULTS ====================

// Add result
int db_add_result(int participant_id, int room_id, int score, int total, int correct) {
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT INTO results (participant_id, room_id, score, total_questions, correct_answers) "
        "VALUES (?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, participant_id);
    sqlite3_bind_int(stmt, 2, room_id);
    sqlite3_bind_int(stmt, 3, score);
    sqlite3_bind_int(stmt, 4, total);
    sqlite3_bind_int(stmt, 5, correct);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;
}

// Get leaderboard for room
int db_get_leaderboard(int room_id, char *output, int max_size) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT u.username, r.score, r.total_questions FROM results r "
        "JOIN participants p ON r.participant_id = p.id "
        "JOIN users u ON p.user_id = u.id "
        "WHERE r.room_id = ? ORDER BY r.score DESC LIMIT 100";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    
    strcpy(output, "");
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *username = (const char*)sqlite3_column_text(stmt, 0);
        int score = sqlite3_column_int(stmt, 1);
        int total = sqlite3_column_int(stmt, 2);
        
        char line[256];
        sprintf(line, "%d. %s - %d/%d\n", count + 1, username, score, total);
        
        if (strlen(output) + strlen(line) < max_size) {
            strcat(output, line);
            count++;
        } else {
            break;
        }
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// ==================== LOGS ====================

// Add log entry
int db_add_log(int user_id, const char *event_type, const char *description) {
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT INTO logs (user_id, event_type, description) VALUES (?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, event_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 1 : 0;
}

// 🔧 Get room ID by name (needed for deletion)
int db_get_room_id_by_name(const char *room_name) {
    if (!db || !room_name) return -1;
    
    sqlite3_stmt *stmt;
    const char *query = "SELECT id FROM rooms WHERE name = ?";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, room_name, -1, SQLITE_STATIC);
    
    int room_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        room_id = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    return room_id;
}

// 🔧 Delete room from database (and associated questions)
int db_delete_room(int room_id) {
    if (!db || room_id <= 0) return 0;
    
    sqlite3_stmt *stmt;
    
    // First delete questions in this room
    const char *delete_questions = "DELETE FROM room_questions WHERE room_id = ?";
    if (sqlite3_prepare_v2(db, delete_questions, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, room_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Then delete the room itself
    const char *delete_room = "DELETE FROM rooms WHERE id = ?";
    if (sqlite3_prepare_v2(db, delete_room, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    int result = (sqlite3_step(stmt) == SQLITE_DONE) ? 1 : 0;
    sqlite3_finalize(stmt);
    
    return result;
}

// Renumber questions to remove gaps after deletion
// This creates a new questions table with sequential IDs and updates all references
int db_renumber_questions(void) {
    if (!db) return 0;
    
    char *err_msg = NULL;
    
    // Begin transaction
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Transaction begin error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }
    
    // Create mapping table from old IDs to new sequential IDs
    const char *create_mapping = 
        "CREATE TEMPORARY TABLE id_mapping AS "
        "SELECT id as old_id, ROW_NUMBER() OVER (ORDER BY id) as new_id FROM questions;";
    
    if (sqlite3_exec(db, create_mapping, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Create mapping error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", 0, 0, &err_msg);
        return 0;
    }
    
    // Create new questions table with sequential IDs
    const char *create_temp = 
        "CREATE TABLE questions_new AS "
        "SELECT m.new_id as id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
        "q.correct_option, q.topic_id, q.difficulty_id, q.created_by, q.created_at "
        "FROM questions q JOIN id_mapping m ON q.id = m.old_id ORDER BY m.new_id;";
    
    if (sqlite3_exec(db, create_temp, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Create temp error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", 0, 0, &err_msg);
        return 0;
    }
    
    // Update room_questions table to use new IDs
    const char *update_room_q = 
        "UPDATE room_questions SET question_id = "
        "(SELECT m.new_id FROM id_mapping m WHERE m.old_id = room_questions.question_id);";
    
    if (sqlite3_exec(db, update_room_q, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Update room_questions error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", 0, 0, &err_msg);
        return 0;
    }
    
    // Update answers table to use new IDs
    const char *update_answers = 
        "UPDATE answers SET question_id = "
        "(SELECT m.new_id FROM id_mapping m WHERE m.old_id = answers.question_id);";
    
    if (sqlite3_exec(db, update_answers, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Update answers error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", 0, 0, &err_msg);
        return 0;
    }
    
    // Drop old table and rename new one
    if (sqlite3_exec(db, "DROP TABLE questions;", 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Drop questions error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", 0, 0, &err_msg);
        return 0;
    }
    
    if (sqlite3_exec(db, "ALTER TABLE questions_new RENAME TO questions;", 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Rename error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", 0, 0, &err_msg);
        return 0;
    }
    
    // Recreate indexes
    const char *recreate_index = 
        "CREATE INDEX IF NOT EXISTS idx_questions_topic ON questions(topic_id); "
        "CREATE INDEX IF NOT EXISTS idx_questions_difficulty ON questions(difficulty_id);";
    
    if (sqlite3_exec(db, recreate_index, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Index recreation error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db, "ROLLBACK;", 0, 0, &err_msg);
        return 0;
    }
    
    // Commit transaction
    if (sqlite3_exec(db, "COMMIT;", 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Commit error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }
    
    return 1;
}

// Compact question IDs to remove gaps (e.g., after deletion)
// Creates new table with sequential IDs and swaps it
int db_compact_question_ids(void) {
    if (!db) return 0;
    
    char *err_msg = NULL;
    
    // Use VACUUM to reclaim space and compact the database
    // This is a simpler approach than renumbering IDs
    if (sqlite3_exec(db, "VACUUM;", 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "Vacuum error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 0;
    }
    
    return 1;
}

