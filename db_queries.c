#include "db_queries.h"
#include "db_init.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ==================== VALIDATION HELPERS ====================

// Validate topic_ids format: should only contain digits and commas
// Valid formats: "1", "1,2,3", "5"
// Invalid: "1a", "1,2,", ",1,2", " 1 "
static int validate_topic_ids_format(const char *topic_ids) {
    if (!topic_ids || strlen(topic_ids) == 0) return 0;
    
    // Check first character is digit
    if (!isdigit((unsigned char)topic_ids[0])) return 0;
    
    for (int i = 0; topic_ids[i]; i++) {
        char c = topic_ids[i];
        // Allow digits and commas only
        if (!isdigit((unsigned char)c) && c != ',') {
            fprintf(stderr, "[VALIDATION] Invalid character '%c' in topic_ids: %s\n", c, topic_ids);
            return 0;
        }
        // Don't allow comma at end
        if (c == ',' && topic_ids[i+1] == '\0') {
            fprintf(stderr, "[VALIDATION] Trailing comma in topic_ids: %s\n", topic_ids);
            return 0;
        }
        // Don't allow double commas
        if (c == ',' && topic_ids[i+1] == ',') {
            fprintf(stderr, "[VALIDATION] Double comma in topic_ids: %s\n", topic_ids);
            return 0;
        }
    }
    
    fprintf(stderr, "[VALIDATION] ✓ topic_ids format valid: %s\n", topic_ids);
    return 1;
}

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
// Topic filter: "topic1:count1 topic2:count2 ..."
// Difficulty filter: "easy:count1 medium:count2 ..."
// This function properly distributes questions from each topic/difficulty combo
int db_get_questions_with_distribution(const char *topic_filter, const char *diff_filter,
                                       DBQuestion *questions, int max_count) {
    
    // Parse topic filter: extract topic names and counts
    typedef struct {
        char name[64];
        int count;
        int id;
    } TopicFilter;
    
    typedef struct {
        char name[32];
        int count;
        int id;
    } DifficultyFilter;
    
    TopicFilter topics[32];
    DifficultyFilter difficulties[3];
    int topic_count = 0;
    int difficulty_count = 0;
    int total_wanted = 0;
    
    // Parse topics: "topic1:count1 topic2:count2 ..."
    // If topic_filter is NULL/empty, load ALL topics
    if (topic_filter && strlen(topic_filter) > 0) {
        char topic_copy[512];
        strncpy(topic_copy, topic_filter, sizeof(topic_copy) - 1);
        topic_copy[sizeof(topic_copy) - 1] = '\0';
        
        char *saveptr;
        char *token = strtok_r(topic_copy, " ", &saveptr);
        
        while (token && topic_count < 32) {
            char *colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                strncpy(topics[topic_count].name, token, sizeof(topics[topic_count].name) - 1);
                topics[topic_count].count = atoi(colon + 1);
                total_wanted += topics[topic_count].count;
                
                // Get topic ID from database
                sqlite3_stmt *stmt;
                const char *query = "SELECT id FROM topics WHERE LOWER(name) = LOWER(?)";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, topics[topic_count].name, -1, SQLITE_STATIC);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        topics[topic_count].id = sqlite3_column_int(stmt, 0);
                    } else {
                        topics[topic_count].id = -1;  // Invalid topic
                    }
                    sqlite3_finalize(stmt);
                }
                
                topic_count++;
            }
            token = strtok_r(NULL, " ", &saveptr);
        }
    } else {
        // No topic filter - load ALL topics from database
        // Distribute max_count questions evenly across all topics
        sqlite3_stmt *stmt;
        const char *query = "SELECT id, name FROM topics ORDER BY id";
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW && topic_count < 32) {
                topics[topic_count].id = sqlite3_column_int(stmt, 0);
                strncpy(topics[topic_count].name, (const char*)sqlite3_column_text(stmt, 1),
                        sizeof(topics[topic_count].name) - 1);
                topics[topic_count].count = 0;  // Will be calculated below
                topic_count++;
            }
            sqlite3_finalize(stmt);
        }
        
        // Distribute max_count evenly across all topics
        if (topic_count > 0) {
            int per_topic = max_count / topic_count;
            for (int i = 0; i < topic_count; i++) {
                topics[i].count = per_topic;
                total_wanted += per_topic;
            }
            // Add remainder to first topic
            int remainder = max_count % topic_count;
            if (remainder > 0) {
                topics[0].count += remainder;
                total_wanted += remainder;
            }
        }
    }
    
    // Parse difficulties: "easy:count1 medium:count2 ..."
    // If diff_filter is empty or "#", load ALL difficulties (no filter)
    if (diff_filter && strlen(diff_filter) > 0 && strcmp(diff_filter, "#") != 0) {
        char diff_copy[256];
        strncpy(diff_copy, diff_filter, sizeof(diff_copy) - 1);
        diff_copy[sizeof(diff_copy) - 1] = '\0';
        
        char *saveptr;
        char *token = strtok_r(diff_copy, " ", &saveptr);
        
        while (token && difficulty_count < 3) {
            char *colon = strchr(token, ':');
            if (colon) {
                *colon = '\0';
                strncpy(difficulties[difficulty_count].name, token, sizeof(difficulties[difficulty_count].name) - 1);
                difficulties[difficulty_count].count = atoi(colon + 1);
                
                // Get difficulty ID from database
                sqlite3_stmt *stmt;
                const char *query = "SELECT id FROM difficulties WHERE LOWER(name) = LOWER(?)";
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, difficulties[difficulty_count].name, -1, SQLITE_STATIC);
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        difficulties[difficulty_count].id = sqlite3_column_int(stmt, 0);
                    } else {
                        difficulties[difficulty_count].id = -1;  // Invalid difficulty
                    }
                    sqlite3_finalize(stmt);
                }
                
                difficulty_count++;
            }
            token = strtok_r(NULL, " ", &saveptr);
        }
    } else {
        // No difficulty filter - load ALL difficulties (easy, medium, hard)
        // Query all difficulties from database
        sqlite3_stmt *stmt;
        const char *query = "SELECT id, name FROM difficulties ORDER BY id";
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW && difficulty_count < 3) {
                difficulties[difficulty_count].id = sqlite3_column_int(stmt, 0);
                strncpy(difficulties[difficulty_count].name, (const char*)sqlite3_column_text(stmt, 1), 
                        sizeof(difficulties[difficulty_count].name) - 1);
                difficulties[difficulty_count].count = 0;  // Will fetch all available
                difficulty_count++;
            }
            sqlite3_finalize(stmt);
        }
    }
    
    // Validate: at least one valid topic
    int valid_topics = 0;
    for (int i = 0; i < topic_count; i++) {
        if (topics[i].id != -1) valid_topics++;
    }
    
    int valid_difficulties = 0;
    for (int i = 0; i < difficulty_count; i++) {
        if (difficulties[i].id != -1) valid_difficulties++;
    }
    
    if (valid_topics == 0 || valid_difficulties == 0 || total_wanted == 0 || total_wanted > max_count) {
        fprintf(stderr, "[DEBUG] Invalid filters: valid_topics=%d, valid_difficulties=%d, total_wanted=%d, max_count=%d\n", 
                valid_topics, valid_difficulties, total_wanted, max_count);
        return 0;
    }
    
    // Now fetch questions for each topic separately, then distribute across difficulties
    int result_count = 0;
    
    for (int t = 0; t < topic_count && result_count < max_count; t++) {
        if (topics[t].id == -1) continue;  // Skip invalid topics
        
        int questions_for_this_topic = topics[t].count;  // How many questions needed from this topic
        int fetched_for_topic = 0;
        
        // For each difficulty level, get required number of questions from THIS TOPIC ONLY
        for (int d = 0; d < difficulty_count && fetched_for_topic < questions_for_this_topic; d++) {
            if (difficulties[d].id == -1) continue;  // Skip invalid difficulties
            
            // Determine how many questions to fetch for this (topic, difficulty) combo
            int limit;
            if (difficulties[d].count > 0) {
                // Explicit count specified for this difficulty
                limit = difficulties[d].count;
            } else {
                // No difficulty filter - distribute topic count evenly across difficulties
                limit = (questions_for_this_topic / difficulty_count);
                if (d == 0) {
                    // Add remainder to first difficulty
                    limit += (questions_for_this_topic % difficulty_count);
                }
            }
            
            // Cap limit to remaining questions needed for this topic
            if (limit > questions_for_this_topic - fetched_for_topic) {
                limit = questions_for_this_topic - fetched_for_topic;
            }
            
            if (limit <= 0) continue;
            
            // Get questions ONLY from this topic AND this difficulty
            const char *query = 
                "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
                "q.correct_option, q.topic_id, q.difficulty_id, t.name, d.name "
                "FROM questions q "
                "JOIN topics t ON q.topic_id = t.id "
                "JOIN difficulties d ON q.difficulty_id = d.id "
                "WHERE q.topic_id = ? AND q.difficulty_id = ? "
                "ORDER BY RANDOM() LIMIT ?";
            
            sqlite3_stmt *stmt;
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
                fprintf(stderr, "[DEBUG] Prepare error: %s\n", sqlite3_errmsg(db));
                continue;
            }
            
            sqlite3_bind_int(stmt, 1, topics[t].id);
            sqlite3_bind_int(stmt, 2, difficulties[d].id);
            sqlite3_bind_int(stmt, 3, limit);
            
            int fetched = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW && result_count < max_count && fetched < limit) {
                questions[result_count].id = sqlite3_column_int(stmt, 0);
                strncpy(questions[result_count].text, (const char*)sqlite3_column_text(stmt, 1), 
                        sizeof(questions[result_count].text) - 1);
                strncpy(questions[result_count].option_a, (const char*)sqlite3_column_text(stmt, 2), 
                        sizeof(questions[result_count].option_a) - 1);
                strncpy(questions[result_count].option_b, (const char*)sqlite3_column_text(stmt, 3), 
                        sizeof(questions[result_count].option_b) - 1);
                strncpy(questions[result_count].option_c, (const char*)sqlite3_column_text(stmt, 4), 
                        sizeof(questions[result_count].option_c) - 1);
                strncpy(questions[result_count].option_d, (const char*)sqlite3_column_text(stmt, 5), 
                        sizeof(questions[result_count].option_d) - 1);
                questions[result_count].correct_option = *((char*)sqlite3_column_text(stmt, 6));
                questions[result_count].topic_id = sqlite3_column_int(stmt, 7);
                questions[result_count].difficulty_id = sqlite3_column_int(stmt, 8);
                strncpy(questions[result_count].topic, (const char*)sqlite3_column_text(stmt, 9), 
                        sizeof(questions[result_count].topic) - 1);
                strncpy(questions[result_count].difficulty, (const char*)sqlite3_column_text(stmt, 10), 
                        sizeof(questions[result_count].difficulty) - 1);
                
                result_count++;
                fetched_for_topic++;
                fetched++;
            }
            
            sqlite3_finalize(stmt);
        }
    }
    
    fprintf(stderr, "[DEBUG] db_get_questions_with_distribution: fetched %d questions (wanted %d)\n", 
            result_count, total_wanted);
    
    return result_count;
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

// 🔧 Get a random question from a specific topic
int db_get_random_question_by_topic(const char *topic_name, DBQuestion *question) {
    if (!db || !topic_name || strlen(topic_name) == 0 || !question) {
        return 0;
    }
    
    sqlite3_stmt *stmt;
    
    // First, get topic ID by name (case-insensitive)
    const char *topic_id_query = "SELECT id FROM topics WHERE LOWER(name) = LOWER(?)";
    if (sqlite3_prepare_v2(db, topic_id_query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error getting topic ID: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, topic_name, -1, SQLITE_STATIC);
    
    int topic_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        topic_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    if (topic_id <= 0) {
        fprintf(stderr, "[DB] Topic '%s' not found\n", topic_name);
        return 0;
    }
    
    // Now get a random question from this topic
    const char *question_query = 
        "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, q.correct_option, "
        "q.topic_id, q.difficulty_id, t.name, d.name "
        "FROM questions q "
        "JOIN topics t ON q.topic_id = t.id "
        "JOIN difficulties d ON q.difficulty_id = d.id "
        "WHERE q.topic_id = ? "
        "ORDER BY RANDOM() LIMIT 1";
    
    if (sqlite3_prepare_v2(db, question_query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error getting random question: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, topic_id);
    
    int success = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        question->id = sqlite3_column_int(stmt, 0);
        strncpy(question->text, (const char *)sqlite3_column_text(stmt, 1), 255);
        question->text[255] = '\0';
        strncpy(question->option_a, (const char *)sqlite3_column_text(stmt, 2), 127);
        question->option_a[127] = '\0';
        strncpy(question->option_b, (const char *)sqlite3_column_text(stmt, 3), 127);
        question->option_b[127] = '\0';
        strncpy(question->option_c, (const char *)sqlite3_column_text(stmt, 4), 127);
        question->option_c[127] = '\0';
        strncpy(question->option_d, (const char *)sqlite3_column_text(stmt, 5), 127);
        question->option_d[127] = '\0';
        question->correct_option = ((const char *)sqlite3_column_text(stmt, 6))[0];
        question->topic_id = sqlite3_column_int(stmt, 7);
        question->difficulty_id = sqlite3_column_int(stmt, 8);
        strncpy(question->topic, (const char *)sqlite3_column_text(stmt, 9), 63);
        question->topic[63] = '\0';
        strncpy(question->difficulty, (const char *)sqlite3_column_text(stmt, 10), 31);
        question->difficulty[31] = '\0';
        
        success = 1;
        fprintf(stderr, "[DB] ✓ Random question fetched for topic '%s': Q%d\n", topic_name, question->id);
    } else {
        fprintf(stderr, "[DB] No questions found for topic '%s'\n", topic_name);
    }
    
    sqlite3_finalize(stmt);
    return success;
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
    if (!db || room_id <= 0 || !output || max_size <= 0) {
        return 0;
    }
    
    sqlite3_stmt *stmt;
    
    // Query: Get top 10 highest scores in this room
    // Order by: score DESC (highest first), then by submitted_at ASC (earliest submission wins ties)
    const char *query = 
        "SELECT u.username, r.score, r.total_questions, r.submitted_at "
        "FROM results r "
        "JOIN participants p ON r.participant_id = p.id "
        "JOIN users u ON p.user_id = u.id "
        "WHERE r.room_id = ? "
        "ORDER BY r.score DESC, r.submitted_at ASC "
        "LIMIT 10";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error preparing leaderboard query: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    
    // Initialize output
    output[0] = '\0';
    int pos = 0;
    int rank = 1;
    
    // Format header with box drawing characters - cleaner design
    pos += snprintf(output + pos, max_size - pos, 
                   "┌─────┬──────────────────────┬─────────┐\n");
    pos += snprintf(output + pos, max_size - pos, 
                   "│ #   │ Username             │  Score  │\n");
    pos += snprintf(output + pos, max_size - pos, 
                   "├─────┼──────────────────────┼─────────┤\n");
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < 10 && pos < max_size - 200) {
        const char *username = (const char*)sqlite3_column_text(stmt, 0);
        int score = sqlite3_column_int(stmt, 1);
        int total = sqlite3_column_int(stmt, 2);
        
        // Truncate long usernames to 20 chars
        char display_name[22];
        if (strlen(username) > 20) {
            strncpy(display_name, username, 17);
            display_name[17] = '\0';
            strcat(display_name, "..");
        } else {
            strcpy(display_name, username);
        }
        
        // Format: "│ 1   │ user                 │  5/5    │\n"
        pos += snprintf(output + pos, max_size - pos,
                       "│ %-3d │ %-20s │ %3d/%-3d │\n",
                       rank, display_name, score, total);
        
        rank++;
        count++;
    }
    
    // Format footer
    if (pos < max_size - 50) {
        pos += snprintf(output + pos, max_size - pos,
                       "└─────┴──────────────────────┴─────────┘\n");
    }
    
    sqlite3_finalize(stmt);
    
    if (count == 0) {
        // No results yet
        snprintf(output, max_size, "No results yet for this room.\n");
        return 0;
    }
    
    fprintf(stderr, "[DB] ✓ Leaderboard retrieved for room %d (showing %d of top 10)\n", 
            room_id, count);
    
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
    
    fprintf(stderr, "[DB] Starting cascade delete for room %d\n", room_id);
    
    // Get room name for logging
    char room_name[128] = "";
    const char *get_room_query = "SELECT name FROM rooms WHERE id = ?";
    if (sqlite3_prepare_v2(db, get_room_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, room_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            strncpy(room_name, (const char*)sqlite3_column_text(stmt, 0), 127);
            room_name[127] = '\0';
        }
        sqlite3_finalize(stmt);
    }
    
    // Count participants before deletion (for logging)
    int participant_count = 0;
    const char *count_participants = "SELECT COUNT(*) FROM participants WHERE room_id = ?";
    if (sqlite3_prepare_v2(db, count_participants, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, room_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            participant_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // Count answers before deletion (for logging)
    int answer_count = 0;
    const char *count_answers = 
        "SELECT COUNT(*) FROM answers WHERE participant_id IN ("
        "  SELECT id FROM participants WHERE room_id = ?"
        ")";
    if (sqlite3_prepare_v2(db, count_answers, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, room_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            answer_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // Count results before deletion (for logging)
    int result_count = 0;
    const char *count_results = "SELECT COUNT(*) FROM results WHERE room_id = ?";
    if (sqlite3_prepare_v2(db, count_results, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, room_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            result_count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // First delete room_questions (explicit, though CASCADE would handle it)
    const char *delete_questions = "DELETE FROM room_questions WHERE room_id = ?";
    if (sqlite3_prepare_v2(db, delete_questions, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, room_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        fprintf(stderr, "[DB]   - Deleted room_questions for room %d\n", room_id);
    }
    
    // CASCADE will handle: participants -> answers, results
    // But let's delete room_questions explicitly and let CASCADE handle the rest
    
    // Finally delete the room itself (CASCADE will delete participants, answers, results)
    const char *delete_room = "DELETE FROM rooms WHERE id = ?";
    if (sqlite3_prepare_v2(db, delete_room, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] ✗ Error preparing delete room query: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE && changes > 0) {
        fprintf(stderr, "[DB] ✓ Room deleted: %s (ID: %d)\n", room_name, room_id);
        fprintf(stderr, "[DB]   - Cascade deleted %d participants\n", participant_count);
        fprintf(stderr, "[DB]   - Cascade deleted %d answers\n", answer_count);
        fprintf(stderr, "[DB]   - Cascade deleted %d results\n", result_count);
        return 1;
    }
    
    fprintf(stderr, "[DB] ✗ Failed to delete room %d\n", room_id);
    return 0;
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

// ==================== ROOM PERSISTENCE ====================
// Load all non-finished rooms from database for server persistence
int db_load_all_rooms(DBRoom *rooms, int max_count) {
    if (!db || !rooms || max_count <= 0) return 0;
    
    sqlite3_stmt *stmt;
    const char *query = "SELECT id, name, owner_id, duration_minutes, is_started, is_finished "
                       "FROM rooms WHERE is_finished = 0 ORDER BY created_at DESC;";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        rooms[count].id = sqlite3_column_int(stmt, 0);
        strncpy(rooms[count].name, (const char *)sqlite3_column_text(stmt, 1), 127);
        rooms[count].name[127] = '\0';
        rooms[count].owner_id = sqlite3_column_int(stmt, 2);
        rooms[count].duration_minutes = sqlite3_column_int(stmt, 3);
        rooms[count].is_started = sqlite3_column_int(stmt, 4);
        rooms[count].is_finished = sqlite3_column_int(stmt, 5);
        
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get username by user_id (helper for loading rooms)
int db_get_username_by_id(int user_id, char *username, int max_len) {
    if (!db || !username || max_len <= 0) return 0;
    
    sqlite3_stmt *stmt;
    const char *query = "SELECT username FROM users WHERE id = ?;";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(username, (const char *)sqlite3_column_text(stmt, 0), max_len - 1);
        username[max_len - 1] = '\0';
        sqlite3_finalize(stmt);
        return 1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// Get random questions filtered by topic IDs and difficulty
// topic_ids: comma-separated list of topic IDs (e.g., "1,2,3"), or NULL to get all topics
// difficulty_id: difficulty ID (1=easy, 2=medium, 3=hard), or 0 to get all difficulties
// limit: max number of questions to return
int db_get_random_filtered_questions(const char *topic_ids, int difficulty_id, 
                                     int limit, DBQuestion *questions) {
    if (!db || !questions || limit <= 0) return 0;
    
    // CRITICAL: Validate topic_ids format before using in SQL query
    if (topic_ids && strlen(topic_ids) > 0) {
        if (!validate_topic_ids_format(topic_ids)) {
            fprintf(stderr, "[ERROR] Invalid topic_ids format: %s\n", topic_ids);
            return 0;
        }
    }
    
    fprintf(stderr, "[DEBUG db_get_random_filtered_questions] START\n");
    fprintf(stderr, "[DEBUG] topic_ids='%s', difficulty_id=%d, limit=%d\n", 
            topic_ids ? topic_ids : "(NULL)", difficulty_id, limit);
    fflush(stderr);
    
    sqlite3_stmt *stmt;
    
    // Build query with optional topic_ids and difficulty_id filters
    char query[1024];
    
    // Build WHERE clause based on what's provided
    char where_clause[256];
    where_clause[0] = '\0';
    
    if (topic_ids && strlen(topic_ids) > 0) {
        snprintf(where_clause, sizeof(where_clause), "WHERE q.topic_id IN (%s)", topic_ids);
        fprintf(stderr, "[DEBUG] WHERE clause (with topic_ids): %s\n", where_clause);
    }
    
    if (difficulty_id > 0) {
        if (strlen(where_clause) > 0) {
            strcat(where_clause, " AND q.difficulty_id = ?");
        } else {
            snprintf(where_clause, sizeof(where_clause), "WHERE q.difficulty_id = ?");
        }
        fprintf(stderr, "[DEBUG] WHERE clause (with difficulty): %s\n", where_clause);
    }
    
    snprintf(query, sizeof(query), 
             "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, q.correct_option, "
             "q.topic_id, q.difficulty_id, t.name, d.name FROM questions q "
             "JOIN topics t ON q.topic_id = t.id "
             "JOIN difficulties d ON q.difficulty_id = d.id "
             "%s ORDER BY RANDOM() LIMIT ?", where_clause);
    
    fprintf(stderr, "[DEBUG] Full Query:\n%s\n", query);
    fflush(stderr);
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Prepare error: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    // Bind parameters
    int bind_idx = 1;
    if (difficulty_id > 0) {
        fprintf(stderr, "[DEBUG] Binding difficulty_id=%d at position %d\n", difficulty_id, bind_idx);
        sqlite3_bind_int(stmt, bind_idx++, difficulty_id);
    }
    fprintf(stderr, "[DEBUG] Binding limit=%d at position %d\n", limit, bind_idx);
    sqlite3_bind_int(stmt, bind_idx++, limit);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < limit) {
        questions[count].id = sqlite3_column_int(stmt, 0);
        
        strncpy(questions[count].text, (const char *)sqlite3_column_text(stmt, 1), 255);
        questions[count].text[255] = '\0';
        
        strncpy(questions[count].option_a, (const char *)sqlite3_column_text(stmt, 2), 127);
        questions[count].option_a[127] = '\0';
        
        strncpy(questions[count].option_b, (const char *)sqlite3_column_text(stmt, 3), 127);
        questions[count].option_b[127] = '\0';
        
        strncpy(questions[count].option_c, (const char *)sqlite3_column_text(stmt, 4), 127);
        questions[count].option_c[127] = '\0';
        
        strncpy(questions[count].option_d, (const char *)sqlite3_column_text(stmt, 5), 127);
        questions[count].option_d[127] = '\0';
        
        questions[count].correct_option = ((const char *)sqlite3_column_text(stmt, 6))[0];
        questions[count].topic_id = sqlite3_column_int(stmt, 7);
        questions[count].difficulty_id = sqlite3_column_int(stmt, 8);
        
        strncpy(questions[count].topic, (const char *)sqlite3_column_text(stmt, 9), 63);
        questions[count].topic[63] = '\0';
        
        strncpy(questions[count].difficulty, (const char *)sqlite3_column_text(stmt, 10), 31);
        questions[count].difficulty[31] = '\0';
        
        fprintf(stderr, "[DEBUG] Fetched question %d: id=%d, topic_id=%d (%s), difficulty_id=%d (%s)\n", 
                count, questions[count].id, questions[count].topic_id, questions[count].topic,
                questions[count].difficulty_id, questions[count].difficulty);
        
        count++;
    }
    
    sqlite3_finalize(stmt);
    fprintf(stderr, "[DEBUG db_get_random_filtered_questions] SUCCESS: Fetched %d questions\n", count);
    fflush(stderr);
    
    return count;
}

// Parse topic filter string (e.g., "database:3 oop:2") and build topic IDs string
// Returns: "2,3" format (topic IDs from database)
// Also fills topic_counts array with counts wanted
char* db_parse_topic_filter(const char *filter_str, int *topic_counts, int max_topics) {
    static char topic_ids[256];
    topic_ids[0] = '\0';
    
    if (!filter_str || strlen(filter_str) == 0) {
        return NULL;
    }
    
    printf("[DEBUG db_parse_topic_filter] Input filter_str: '%s'\n", filter_str);
    fflush(stdout);
    
    char filter_copy[256];
    strncpy(filter_copy, filter_str, sizeof(filter_copy) - 1);
    filter_copy[sizeof(filter_copy) - 1] = '\0';
    
    int topic_id_count = 0;
    char *saveptr;
    char *token = strtok_r(filter_copy, " ", &saveptr);
    
    while (token && topic_id_count < max_topics) {
        printf("[DEBUG db_parse_topic_filter] Processing token: '%s'\n", token);
        fflush(stdout);
        
        // Parse "topic_name:count"
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            const char *topic_name = token;
            int wanted_count = atoi(colon + 1);
            
            printf("[DEBUG db_parse_topic_filter] topic_name='%s', wanted_count=%d\n", topic_name, wanted_count);
            fflush(stdout);
            
            // Get topic ID from database
            char query[256];
            sqlite3_stmt *stmt;
            snprintf(query, sizeof(query), "SELECT id FROM topics WHERE LOWER(name) = LOWER(?);");
            
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, topic_name, -1, SQLITE_STATIC);
                
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    int topic_id = sqlite3_column_int(stmt, 0);
                    printf("[DEBUG db_parse_topic_filter] Found topic_id=%d for topic_name='%s'\n", topic_id, topic_name);
                    fflush(stdout);
                    
                    // Add to topic_ids string
                    if (strlen(topic_ids) > 0) {
                        strcat(topic_ids, ",");
                    }
                    char id_str[32];
                    snprintf(id_str, sizeof(id_str), "%d", topic_id);
                    strcat(topic_ids, id_str);
                    
                    // Store count
                    topic_counts[topic_id_count] = wanted_count;
                    topic_id_count++;
                } else {
                    printf("[DEBUG db_parse_topic_filter] Topic not found: '%s'\n", topic_name);
                    fflush(stdout);
                }
                sqlite3_finalize(stmt);
            }
        }
        token = strtok_r(NULL, " ", &saveptr);
    }
    
    printf("[DEBUG db_parse_topic_filter] Final topic_ids: '%s'\n", topic_ids);
    fflush(stdout);
    
    return (strlen(topic_ids) > 0) ? topic_ids : NULL;
}

// Parse difficulty filter string (e.g., "easy:3 medium:1 hard:1")
// Fills difficulty_counts array: [0]=easy, [1]=medium, [2]=hard
void db_parse_difficulty_filter(const char *filter_str, int *difficulty_counts) {
    difficulty_counts[0] = 0;  // easy
    difficulty_counts[1] = 0;  // medium
    difficulty_counts[2] = 0;  // hard
    
    if (!filter_str || strlen(filter_str) == 0) {
        return;
    }
    
    char filter_copy[256];
    strncpy(filter_copy, filter_str, sizeof(filter_copy) - 1);
    filter_copy[sizeof(filter_copy) - 1] = '\0';
    
    char *saveptr;
    char *token = strtok_r(filter_copy, " ", &saveptr);
    
    while (token) {
        // Parse "difficulty_name:count"
        char *colon = strchr(token, ':');
        if (colon) {
            *colon = '\0';
            const char *diff_name = token;
            int count = atoi(colon + 1);
            
            if (strcasecmp(diff_name, "easy") == 0) {
                difficulty_counts[0] = count;
            } else if (strcasecmp(diff_name, "medium") == 0) {
                difficulty_counts[1] = count;
            } else if (strcasecmp(diff_name, "hard") == 0) {
                difficulty_counts[2] = count;
            }
        }
        token = strtok_r(NULL, " ", &saveptr);
    }
}

// Count questions by difficulty for specific topics
// topic_ids: comma-separated topic IDs (e.g., "1,2,5")
// difficulty_counts: array to fill [0]=easy, [1]=medium, [2]=hard
void db_count_difficulties_for_topics(const char *topic_ids, int *difficulty_counts) {
    difficulty_counts[0] = 0;  // easy
    difficulty_counts[1] = 0;  // medium
    difficulty_counts[2] = 0;  // hard
    
    if (!db || !topic_ids || strlen(topic_ids) == 0) {
        return;
    }
    
    sqlite3_stmt *stmt;
    char query[512];
    
    printf("[DEBUG db_count_difficulties_for_topics] topic_ids='%s'\n", topic_ids);
    fflush(stdout);
    
    // Build query to count questions by difficulty for given topics
    snprintf(query, sizeof(query),
             "SELECT difficulty_id, COUNT(*) FROM questions "
             "WHERE topic_id IN (%s) GROUP BY difficulty_id;", topic_ids);
    
    printf("[DEBUG db_count_difficulties_for_topics] query='%s'\n", query);
    fflush(stdout);
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(db));
        return;
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int difficulty_id = sqlite3_column_int(stmt, 0);
        int count = sqlite3_column_int(stmt, 1);
        
        printf("[DEBUG db_count_difficulties_for_topics] difficulty_id=%d, count=%d\n", difficulty_id, count);
        fflush(stdout);
        
        if (difficulty_id == 1) {
            difficulty_counts[0] = count;  // easy
        } else if (difficulty_id == 2) {
            difficulty_counts[1] = count;  // medium
        } else if (difficulty_id == 3) {
            difficulty_counts[2] = count;  // hard
        }
    }
    
    printf("[DEBUG db_count_difficulties_for_topics] Final: easy=%d, medium=%d, hard=%d\n",
           difficulty_counts[0], difficulty_counts[1], difficulty_counts[2]);
    fflush(stdout);
    
    sqlite3_finalize(stmt);
}

// ==================== RESULTS PERSISTENCE ====================

// Save participant to database when user joins room
int db_save_participant(int room_id, int user_id) {
    if (!db || room_id <= 0 || user_id <= 0) return -1;
    
    sqlite3_stmt *stmt;
    const char *query = "INSERT OR IGNORE INTO participants (room_id, user_id) VALUES (?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error saving participant: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    sqlite3_bind_int(stmt, 2, user_id);
    
    int rc = sqlite3_step(stmt);
    int participant_id = (rc == SQLITE_DONE) ? (int)sqlite3_last_insert_rowid(db) : -1;
    sqlite3_finalize(stmt);
    
    if (participant_id <= 0) {
        // User already participant - get their ID
        const char *get_query = "SELECT id FROM participants WHERE room_id = ? AND user_id = ?";
        if (sqlite3_prepare_v2(db, get_query, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, room_id);
            sqlite3_bind_int(stmt, 2, user_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                participant_id = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }
    
    fprintf(stderr, "[DB] Saved/Retrieved participant: id=%d, room_id=%d, user_id=%d\n", 
            participant_id, room_id, user_id);
    
    return participant_id;
}

// Save answer to database when user submits an answer
int db_save_answer(int participant_id, int question_id, char selected_option, int is_correct) {
    if (!db || participant_id <= 0 || question_id <= 0) return 0;
    
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT OR REPLACE INTO answers (participant_id, question_id, selected_option, is_correct) "
        "VALUES (?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error saving answer: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, participant_id);
    sqlite3_bind_int(stmt, 2, question_id);
    sqlite3_bind_text(stmt, 3, &selected_option, 1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, is_correct);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[DB] Failed to save answer for participant %d, question %d\n", participant_id, question_id);
        return 0;
    }
    
    fprintf(stderr, "[DB] Saved answer: participant_id=%d, question_id=%d, selected='%c', correct=%d\n",
            participant_id, question_id, selected_option, is_correct);
    
    return 1;
}

// Save result to database when user finishes test
int db_save_result(int participant_id, int room_id, int score, int total_questions, int correct_answers) {
    if (!db || participant_id <= 0 || room_id <= 0) return -1;
    
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT OR REPLACE INTO results (participant_id, room_id, score, total_questions, correct_answers) "
        "VALUES (?, ?, ?, ?, ?)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error saving result: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, participant_id);
    sqlite3_bind_int(stmt, 2, room_id);
    sqlite3_bind_int(stmt, 3, score);
    sqlite3_bind_int(stmt, 4, total_questions);
    sqlite3_bind_int(stmt, 5, correct_answers);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[DB] Failed to save result for participant %d\n", participant_id);
        return -1;
    }
    
    fprintf(stderr, "[DB] Saved result: participant_id=%d, room_id=%d, score=%d/%d, correct=%d\n",
            participant_id, room_id, score, total_questions, correct_answers);
    
    return 1;
}

// ==================== LOAD PARTICIPANTS FOR SERVER RESTART ====================

// Load answers for a participant from database and reconstruct answer string
// Returns 1 on success, 0 on failure
int db_load_participant_answers(int participant_id, char *answers, int max_len) {
    if (!db || participant_id <= 0 || !answers || max_len <= 0) {
        return 0;
    }
    
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT question_id, selected_option FROM answers "
        "WHERE participant_id = ? "
        "ORDER BY question_id ASC";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error loading participant answers: %s\n", sqlite3_errmsg(db));
        memset(answers, '.', max_len);
        answers[max_len - 1] = '\0';
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, participant_id);
    
    // Initialize all answers to '.' (not answered)
    memset(answers, '.', max_len);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int question_id = sqlite3_column_int(stmt, 0);
        const char *selected = (const char*)sqlite3_column_text(stmt, 1);
        
        // Use safe indexing (question_id should be sequential)
        if (question_id > 0 && question_id <= max_len) {
            answers[question_id - 1] = selected[0];
        }
    }
    
    sqlite3_finalize(stmt);
    answers[max_len - 1] = '\0';
    
    fprintf(stderr, "[DB] Loaded answers for participant %d: %s\n", participant_id, answers);
    return 1;
}

// Load all participants and their results for a room from database
// Returns number of participants loaded
int db_load_room_participants(int room_id, DBParticipantInfo *participants, int max_count) {
    if (!db || room_id <= 0 || !participants || max_count <= 0) {
        return 0;
    }
    
    sqlite3_stmt *stmt;
    
    // Get all participants who have joined this room, including their results
    const char *query = 
        "SELECT p.id, u.username, r.score "
        "FROM participants p "
        "JOIN users u ON p.user_id = u.id "
        "LEFT JOIN results r ON p.id = r.participant_id AND r.room_id = ? "
        "WHERE p.room_id = ? "
        "ORDER BY p.joined_at DESC";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error loading participants: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    sqlite3_bind_int(stmt, 2, room_id);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        int participant_id = sqlite3_column_int(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        
        // Get score (NULL = still in progress = -1)
        int score = -1;
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            score = sqlite3_column_int(stmt, 2);
        }
        
        // Populate structure
        participants[count].db_id = participant_id;
        strncpy(participants[count].username, username, 63);
        participants[count].username[63] = '\0';
        participants[count].score = score;
        
        // Load answers from answers table
        memset(participants[count].answers, '.', 50);
        participants[count].answers[49] = '\0';
        db_load_participant_answers(participant_id, participants[count].answers, 50);
        
        fprintf(stderr, "[DB] ✓ Loaded participant %d: %s (score=%d, answers=%s)\n", 
                participant_id, username, score, participants[count].answers);
        
        count++;
    }
    
    sqlite3_finalize(stmt);
    fprintf(stderr, "[DB] ✓ Loaded %d participants for room %d\n", count, room_id);
    return count;
}

// Get all results for a specific room
int db_get_room_results(int room_id, char *output, int max_size) {
    if (!db || room_id <= 0 || !output || max_size <= 0) return 0;
    
    sqlite3_stmt *stmt;
    const char *query =
        "SELECT u.username, r.score, r.total_questions, r.correct_answers, r.submitted_at "
        "FROM results r "
        "JOIN participants p ON r.participant_id = p.id "
        "JOIN users u ON p.user_id = u.id "
        "WHERE r.room_id = ? "
        "ORDER BY r.score DESC, r.submitted_at ASC";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Error getting room results: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    
    strcpy(output, "");
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *username = (const char*)sqlite3_column_text(stmt, 0);
        int score = sqlite3_column_int(stmt, 1);
        int total = sqlite3_column_int(stmt, 2);
        int correct = sqlite3_column_int(stmt, 3);
        const char *submitted_at = (const char*)sqlite3_column_text(stmt, 4);
        
        char line[512];
        snprintf(line, sizeof(line), "%d|%s|%d|%d|%d|%s\n", 
                 count + 1, username, score, correct, total, submitted_at);
        
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

