#include "db_queries.h"
#include "db_init.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== QUESTIONS ====================

// Add question to database
int db_add_question(const char *text, const char *opt_a, const char *opt_b,
                   const char *opt_c, const char *opt_d, char correct,
                   const char *topic, const char *difficulty, int created_by_id) {
    sqlite3_stmt *stmt;
    
    // Get topic ID
    int topic_id = 0;
    const char *topic_query = "SELECT id FROM topics WHERE name = ?";
    if (sqlite3_prepare_v2(db, topic_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, topic, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            topic_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    // Get difficulty ID
    int difficulty_id = 0;
    const char *diff_query = "SELECT id FROM difficulties WHERE name = ?";
    if (sqlite3_prepare_v2(db, diff_query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, difficulty, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            difficulty_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    
    if (topic_id == 0 || difficulty_id == 0) {
        fprintf(stderr, "Error: topic or difficulty not found (topic='%s' id=%d, diff='%s' id=%d)\n", 
                topic, topic_id, difficulty, difficulty_id);
        return -1;
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
    
    // Bind created_by - use NULL if not specified (id = 0)
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

// Get question by ID
int db_get_question(int id, DBQuestion *q) {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT id, text, option_a, option_b, option_c, option_d, "
        "correct_option, topic_id, difficulty_id FROM questions WHERE id = ?";
    
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
        "SELECT id, text, option_a, option_b, option_c, option_d, "
        "correct_option, topic_id, difficulty_id FROM questions "
        "ORDER BY id LIMIT ?";
    
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
        "q.correct_option, q.topic_id, q.difficulty_id FROM questions q "
        "JOIN topics t ON q.topic_id = t.id WHERE t.name = ? LIMIT ?";
    
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
        "q.correct_option, q.topic_id, q.difficulty_id FROM questions q "
        "JOIN difficulties d ON q.difficulty_id = d.id WHERE d.name = ? LIMIT ?";
    
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
                       "q.correct_option, q.topic_id, q.difficulty_id FROM questions q "
                       "JOIN topics t ON q.topic_id = t.id "
                       "JOIN difficulties d ON q.difficulty_id = d.id WHERE 1=1";
    
    if (strlen(topic_filter) > 0) {
        strcat(query, " AND t.name IN (");
        char *topic_ptr = strtok(topic_copy, " ");
        int first = 1;
        while (topic_ptr) {
            char *colon = strchr(topic_ptr, ':');
            if (colon) *colon = '\0';
            
            if (!first) strcat(query, ", ");
            strcat(query, "'");
            strcat(query, topic_ptr);
            strcat(query, "'");
            first = 0;
            topic_ptr = strtok(NULL, " ");
        }
        strcat(query, ")");
    }
    
    if (strlen(diff_filter) > 0) {
        strcat(query, " AND d.name IN (");
        char *diff_ptr = strtok(diff_copy, " ");
        int first = 1;
        while (diff_ptr) {
            char *colon = strchr(diff_ptr, ':');
            if (colon) *colon = '\0';
            
            if (!first) strcat(query, ", ");
            strcat(query, "'");
            strcat(query, diff_ptr);
            strcat(query, "'");
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
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get all topics
int db_get_all_topics(char *output) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT name, COUNT(*) FROM questions q "
                       "JOIN topics t ON q.topic_id = t.id GROUP BY t.id ORDER BY name";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    strcpy(output, "");
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *topic = (const char*)sqlite3_column_text(stmt, 0);
        int topic_count = sqlite3_column_int(stmt, 1);
        
        if (count > 0) strcat(output, " ");
        sprintf(output + strlen(output), "%s:%d", topic, topic_count);
        count++;
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Get all difficulties
int db_get_all_difficulties(char *output) {
    sqlite3_stmt *stmt;
    const char *query = "SELECT name, COUNT(*) FROM questions q "
                       "JOIN difficulties d ON q.difficulty_id = d.id GROUP BY d.id ORDER BY d.level";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    strcpy(output, "");
    int count = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *difficulty = (const char*)sqlite3_column_text(stmt, 0);
        int diff_count = sqlite3_column_int(stmt, 1);
        
        if (count > 0) strcat(output, " ");
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
        "q.correct_option, q.topic_id, q.difficulty_id FROM questions q "
        "JOIN room_questions rq ON q.id = rq.question_id "
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
