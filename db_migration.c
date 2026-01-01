#include "db_migration.h"
#include "db_init.h"
#include "db_queries.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sqlite3.h>

// Migrate topics from questions.txt to database
int migrate_topics(const char *questions_file) {
    FILE *f = fopen(questions_file, "r");
    if (!f) {
        fprintf(stderr, "Error opening questions file for migration\n");
        return 0;
    }
    
    char line[1024];
    int topics_added = 0;
    
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        
        // Format: id|text|A|B|C|D|correct|topic|difficulty
        char *parts[9];
        char *ptr = strtok(line, "|");
        int idx = 0;
        
        while (ptr && idx < 9) {
            parts[idx++] = ptr;
            ptr = strtok(NULL, "|");
        }
        
        if (idx < 8) continue;
        
        const char *topic_raw = parts[7];
        
        // Convert topic to lowercase
        char topic[64];
        strcpy(topic, topic_raw);
        for (int i = 0; topic[i]; i++) {
            topic[i] = tolower((unsigned char)topic[i]);
        }
        
        // Check if topic already exists
        sqlite3_stmt *stmt;
        const char *check_query = "SELECT 1 FROM topics WHERE name = ?";
        
        if (sqlite3_prepare_v2(db, check_query, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, topic, -1, SQLITE_STATIC);
            
            if (sqlite3_step(stmt) != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                
                // Add topic
                const char *insert_query = "INSERT INTO topics (name) VALUES (?)";
                if (sqlite3_prepare_v2(db, insert_query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, topic, -1, SQLITE_STATIC);
                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        topics_added++;
                    }
                    sqlite3_finalize(stmt);
                }
            } else {
                sqlite3_finalize(stmt);
            }
        }
    }
    
    fclose(f);
    printf("✓ Migrated %d topics\n", topics_added);
    return 1;
}

// Migrate questions from questions.txt to database
int migrate_questions(const char *questions_file) {
    FILE *f = fopen(questions_file, "r");
    if (!f) {
        fprintf(stderr, "Error opening questions file for migration\n");
        return 0;
    }
    
    char line[1024];
    int questions_added = 0;
    int questions_failed = 0;
    
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        
        // Format: id|text|A|B|C|D|correct|topic|difficulty
        char *parts[9];
        char line_copy[1024];
        strcpy(line_copy, line);
        
        char *ptr = strtok(line_copy, "|");
        int idx = 0;
        
        while (ptr && idx < 9) {
            parts[idx++] = ptr;
            ptr = strtok(NULL, "|");
        }
        
        if (idx < 9) continue;
        
        // Extract fields
        const char *text = parts[1];
        const char *opt_a = parts[2];
        const char *opt_b = parts[3];
        const char *opt_c = parts[4];
        const char *opt_d = parts[5];
        char correct = parts[6][0];
        const char *topic_raw = parts[7];
        const char *difficulty_raw = parts[8];
        
        // Convert topic to lowercase
        char topic[64];
        strcpy(topic, topic_raw);
        for (int i = 0; topic[i]; i++) {
            topic[i] = tolower((unsigned char)topic[i]);
        }
        
        // Convert difficulty to lowercase
        char difficulty[32];
        strcpy(difficulty, difficulty_raw);
        for (int i = 0; difficulty[i]; i++) {
            difficulty[i] = tolower((unsigned char)difficulty[i]);
        }
        
        // Insert question with no creator (NULL)
        int q_id = db_add_question(text, opt_a, opt_b, opt_c, opt_d, correct, topic, difficulty, 0);
        
        if (q_id > 0) {
            questions_added++;
        } else {
            questions_failed++;
        }
    }
    
    fclose(f);
    printf("✓ Migrated %d questions (%d failed)\n", questions_added, questions_failed);
    return 1;
}

// Migrate users from users.txt to database
int migrate_users(const char *users_file) {
    FILE *f = fopen(users_file, "r");
    if (!f) {
        fprintf(stderr, "Error opening users file for migration\n");
        return 0;
    }
    
    char line[512];
    int users_added = 0;
    
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        
        // Format: username|password|role
        char *parts[3];
        char line_copy[512];
        strcpy(line_copy, line);
        
        char *ptr = strtok(line_copy, "|");
        int idx = 0;
        
        while (ptr && idx < 3) {
            parts[idx++] = ptr;
            ptr = strtok(NULL, "|");
        }
        
        if (idx < 3) continue;
        
        const char *username = parts[0];
        const char *password = parts[1];
        const char *role = parts[2];
        
        if (db_add_user(username, password, role) > 0) {
            users_added++;
        }
    }
    
    fclose(f);
    printf("✓ Migrated %d users\n", users_added);
    return 1;
}

// Migrate: Add topic_filter and difficulty_filter columns to rooms table if they don't exist
int migrate_rooms_add_filters(void) {
    // Check if columns exist by trying to select from them
    sqlite3_stmt *stmt;
    const char *check_query = "PRAGMA table_info(rooms);";
    
    int has_topic_filter = 0;
    int has_difficulty_filter = 0;
    
    if (sqlite3_prepare_v2(db, check_query, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *col_name = (const char *)sqlite3_column_text(stmt, 1);
            if (col_name && strcmp(col_name, "topic_filter") == 0) {
                has_topic_filter = 1;
            }
            if (col_name && strcmp(col_name, "difficulty_filter") == 0) {
                has_difficulty_filter = 1;
            }
        }
        sqlite3_finalize(stmt);
    }
    
    // Add missing columns
    if (!has_topic_filter) {
        const char *add_topic = "ALTER TABLE rooms ADD COLUMN topic_filter TEXT;";
        if (sqlite3_exec(db, add_topic, NULL, NULL, NULL) == SQLITE_OK) {
            printf("✓ Added topic_filter column to rooms\n");
        } else {
            fprintf(stderr, "Failed to add topic_filter column: %s\n", sqlite3_errmsg(db));
            return 0;
        }
    }
    
    if (!has_difficulty_filter) {
        const char *add_difficulty = "ALTER TABLE rooms ADD COLUMN difficulty_filter TEXT;";
        if (sqlite3_exec(db, add_difficulty, NULL, NULL, NULL) == SQLITE_OK) {
            printf("✓ Added difficulty_filter column to rooms\n");
        } else {
            fprintf(stderr, "Failed to add difficulty_filter column: %s\n", sqlite3_errmsg(db));
            return 0;
        }
    }
    
    return 1;
}

// Migrate all data from text files to SQLite database
int migrate_from_text_files(const char *data_dir) {
    char questions_path[256], users_path[256];
    
    snprintf(questions_path, sizeof(questions_path), "%s/questions.txt", data_dir);
    snprintf(users_path, sizeof(users_path), "%s/users.txt", data_dir);
    
    printf("\n=== Starting Data Migration ===\n");
    
    // Step 0: Apply schema migrations
    printf("Step 0: Checking for schema updates...\n");
    if (!migrate_rooms_add_filters()) {
        fprintf(stderr, "Failed to apply schema migrations\n");
        return 0;
    }
    
    // Step 1: Migrate topics (creates topic_id references)
    printf("Step 1: Migrating topics...\n");
    if (!migrate_topics(questions_path)) {
        fprintf(stderr, "Failed to migrate topics\n");
        return 0;
    }
    
    // Step 2: Migrate questions
    printf("Step 2: Migrating questions...\n");
    if (!migrate_questions(questions_path)) {
        fprintf(stderr, "Failed to migrate questions\n");
        return 0;
    }
    
    // Step 3: Migrate users
    printf("Step 3: Migrating users...\n");
    if (!migrate_users(users_path)) {
        fprintf(stderr, "Failed to migrate users\n");
        return 0;
    }
    
    printf("=== Migration Complete ===\n\n");
    return 1;
}

// Verify migration integrity
int verify_migration(void) {
    sqlite3_stmt *stmt;
    
    printf("Migration Verification:\n");
    
    // Check topics count
    const char *topics_query = "SELECT COUNT(*) FROM topics";
    if (sqlite3_prepare_v2(db, topics_query, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  Topics: %d\n", sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    
    // Check questions count
    const char *questions_query = "SELECT COUNT(*) FROM questions";
    if (sqlite3_prepare_v2(db, questions_query, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  Questions: %d\n", sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    
    // Check users count
    const char *users_query = "SELECT COUNT(*) FROM users";
    if (sqlite3_prepare_v2(db, users_query, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("  Users: %d\n", sqlite3_column_int(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }
    
    return 1;
}

// Load data from SQL file
int load_sql_file(const char *sql_file_path) {
    FILE *f = fopen(sql_file_path, "r");
    if (!f) {
        fprintf(stderr, "Error opening SQL file: %s\n", sql_file_path);
        return 0;
    }
    
    // Read entire file into buffer
    char *sql_buffer = NULL;
    size_t buffer_size = 0;
    size_t buffer_capacity = 4096;
    
    sql_buffer = malloc(buffer_capacity);
    if (!sql_buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(f);
        return 0;
    }
    
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (buffer_size >= buffer_capacity - 1) {
            buffer_capacity *= 2;
            char *new_buffer = realloc(sql_buffer, buffer_capacity);
            if (!new_buffer) {
                fprintf(stderr, "Memory reallocation failed\n");
                free(sql_buffer);
                fclose(f);
                return 0;
            }
            sql_buffer = new_buffer;
        }
        sql_buffer[buffer_size++] = c;
    }
    fclose(f);
    
    sql_buffer[buffer_size] = '\0';
    
    // Execute SQL statements
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql_buffer, NULL, NULL, &errmsg);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errmsg);
        sqlite3_free(errmsg);
        free(sql_buffer);
        return 0;
    }
    
    free(sql_buffer);
    printf("✓ Loaded SQL file: %s\n", sql_file_path);
    return 1;
}
