#include "db_init.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sqlite3 *db = NULL;
static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

// Initialize database connection
int db_init(const char *db_path) {
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error opening database: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    // Enable foreign keys
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    
    return 1;
}

// Close database connection
void db_close(void) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

// Create database tables
int db_create_tables(void) {
    const char *schema_queries[] = {
        // Topics
        "CREATE TABLE IF NOT EXISTS topics ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  description TEXT,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",
        
        // Difficulties
        "CREATE TABLE IF NOT EXISTS difficulties ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL UNIQUE,"
        "  level INTEGER NOT NULL CHECK(level BETWEEN 1 AND 3),"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",
        
        // Users
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  username TEXT NOT NULL UNIQUE,"
        "  password TEXT NOT NULL,"
        "  role TEXT NOT NULL DEFAULT 'student' CHECK(role IN ('admin', 'student')),"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",
        
        // Questions
        "CREATE TABLE IF NOT EXISTS questions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  text TEXT NOT NULL,"
        "  option_a TEXT NOT NULL,"
        "  option_b TEXT NOT NULL,"
        "  option_c TEXT NOT NULL,"
        "  option_d TEXT NOT NULL,"
        "  correct_option TEXT NOT NULL CHECK(correct_option IN ('A', 'B', 'C', 'D')),"
        "  topic_id INTEGER NOT NULL,"
        "  difficulty_id INTEGER NOT NULL,"
        "  created_by INTEGER,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY(topic_id) REFERENCES topics(id) ON DELETE RESTRICT,"
        "  FOREIGN KEY(difficulty_id) REFERENCES difficulties(id) ON DELETE RESTRICT,"
        "  FOREIGN KEY(created_by) REFERENCES users(id) ON DELETE SET NULL"
        ");",
        
        // Create indexes
        "CREATE INDEX IF NOT EXISTS idx_questions_topic ON questions(topic_id);",
        "CREATE INDEX IF NOT EXISTS idx_questions_difficulty ON questions(difficulty_id);",
        
        // Rooms
        "CREATE TABLE IF NOT EXISTS rooms ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  owner_id INTEGER NOT NULL,"
        "  duration_minutes INTEGER DEFAULT 60,"
        "  is_started INTEGER DEFAULT 0,"
        "  is_finished INTEGER DEFAULT 0,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY(owner_id) REFERENCES users(id) ON DELETE CASCADE"
        ");",
        
        // Room Questions
        "CREATE TABLE IF NOT EXISTS room_questions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  room_id INTEGER NOT NULL,"
        "  question_id INTEGER NOT NULL,"
        "  order_num INTEGER NOT NULL,"
        "  UNIQUE(room_id, question_id),"
        "  FOREIGN KEY(room_id) REFERENCES rooms(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(question_id) REFERENCES questions(id) ON DELETE CASCADE"
        ");",
        
        // Participants
        "CREATE TABLE IF NOT EXISTS participants ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  room_id INTEGER NOT NULL,"
        "  user_id INTEGER NOT NULL,"
        "  joined_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  started_at DATETIME,"
        "  submitted_at DATETIME,"
        "  UNIQUE(room_id, user_id),"
        "  FOREIGN KEY(room_id) REFERENCES rooms(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
        ");",
        
        // Create indexes
        "CREATE INDEX IF NOT EXISTS idx_participants_room ON participants(room_id);",
        "CREATE INDEX IF NOT EXISTS idx_participants_user ON participants(user_id);",
        
        // Answers
        "CREATE TABLE IF NOT EXISTS answers ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  participant_id INTEGER NOT NULL,"
        "  question_id INTEGER NOT NULL,"
        "  selected_option TEXT,"
        "  is_correct INTEGER DEFAULT 0,"
        "  submitted_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  UNIQUE(participant_id, question_id),"
        "  FOREIGN KEY(participant_id) REFERENCES participants(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(question_id) REFERENCES questions(id) ON DELETE CASCADE"
        ");",
        
        // Results
        "CREATE TABLE IF NOT EXISTS results ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  participant_id INTEGER NOT NULL,"
        "  room_id INTEGER NOT NULL,"
        "  score INTEGER DEFAULT 0,"
        "  total_questions INTEGER DEFAULT 0,"
        "  correct_answers INTEGER DEFAULT 0,"
        "  submitted_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  UNIQUE(participant_id, room_id),"
        "  FOREIGN KEY(participant_id) REFERENCES participants(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(room_id) REFERENCES rooms(id) ON DELETE CASCADE"
        ");",
        
        // Logs
        "CREATE TABLE IF NOT EXISTS logs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id INTEGER,"
        "  event_type TEXT NOT NULL,"
        "  description TEXT,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE SET NULL"
        ");",
        
        "CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp);"
    };
    
    int num_queries = sizeof(schema_queries) / sizeof(schema_queries[0]);
    char *err_msg = NULL;
    
    for (int i = 0; i < num_queries; i++) {
        int rc = sqlite3_exec(db, schema_queries[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Error creating table: %s\n", err_msg);
            sqlite3_free(err_msg);
            return 0;
        }
    }
    
    return 1;
}

// Get database connection (thread-safe)
sqlite3* db_get_connection(void) {
    return db;
}

// Lock database for thread-safe access
void db_lock_acquire(void) {
    pthread_mutex_lock(&db_lock);
}

// Unlock database
void db_lock_release(void) {
    pthread_mutex_unlock(&db_lock);
}

// Initialize default difficulties
int db_init_default_difficulties(void) {
    const char *difficulties[] = {"easy", "medium", "hard"};
    const int levels[] = {1, 2, 3};
    
    for (int i = 0; i < 3; i++) {
        sqlite3_stmt *stmt;
        const char *query = "INSERT OR IGNORE INTO difficulties (name, level) VALUES (?, ?);";
        
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
            return 0;
        }
        
        sqlite3_bind_text(stmt, 1, difficulties[i], -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, levels[i]);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "Error inserting difficulty: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return 0;
        }
        
        sqlite3_finalize(stmt);
    }
    
    return 1;
}

// Database health check
int db_health_check(void) {
    const char *check_query = "SELECT COUNT(*) FROM sqlite_master WHERE type='table';";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, check_query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    int table_count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        table_count = sqlite3_column_int(stmt, 0);
    }
    
    sqlite3_finalize(stmt);
    
    // We should have at least 8 tables
    return table_count >= 8;
}
