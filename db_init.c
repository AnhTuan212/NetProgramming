#include "db_init.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

// ✅ FIXED: Define db here (it's used by all db functions)
sqlite3 *db = NULL;
static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

// Initialize database connection
int db_init(const char *db_path) {
    fprintf(stderr, "[DEBUG] db_init() opening %s\n", db_path);
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error opening database: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    fprintf(stderr, "[DEBUG] Database opened successfully, db pointer = %p\n", (void*)db);
    
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
    fprintf(stderr, "[DEBUG] db_create_tables() starting...\n");
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
    
    fprintf(stderr, "[DEBUG] Creating %d tables...\n", num_queries);
    for (int i = 0; i < num_queries; i++) {
        int rc = sqlite3_exec(db, schema_queries[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Error creating table[%d]: %s\n", i, err_msg);
            sqlite3_free(err_msg);
            return 0;
        }
    }
    
    fprintf(stderr, "[DEBUG] All tables created successfully\n");
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

// ✅ NEW: Load 30 hardcoded sample questions if database is empty
int db_load_sample_data(void) {
    // Check if questions table has data
    sqlite3_stmt *stmt;
    const char *count_query = "SELECT COUNT(*) FROM questions";
    
    if (sqlite3_prepare_v2(db, count_query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    int has_questions = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        has_questions = sqlite3_column_int(stmt, 0) > 0;
    }
    sqlite3_finalize(stmt);
    
    if (has_questions) {
        return 1;  // Already have questions
    }
    
    fprintf(stderr, "[DEBUG] Loading 30 sample questions...\n");
    
    // First ensure topics exist
    const char *topics[] = {
        "INSERT OR IGNORE INTO topics (name, description) VALUES ('cloud', 'Cloud Computing');",
        "INSERT OR IGNORE INTO topics (name, description) VALUES ('database', 'Database Management');",
        "INSERT OR IGNORE INTO topics (name, description) VALUES ('networking', 'Computer Networking');",
        "INSERT OR IGNORE INTO topics (name, description) VALUES ('programming', 'Programming Languages');",
        "INSERT OR IGNORE INTO topics (name, description) VALUES ('security', 'Cybersecurity');"
    };
    
    for (int i = 0; i < 5; i++) {
        char *err = NULL;
        sqlite3_exec(db, topics[i], NULL, NULL, &err);
        if (err) sqlite3_free(err);
    }
    
    // 30 hardcoded sample questions - 5 topics x 6 questions per topic
    // cloud (3 easy, 2 medium, 1 hard), database, networking, programming, security
    const char *inserts[] = {
        // Cloud Computing - 6 questions
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is cloud computing?', 'On-premise servers', 'Internet-based computing resources', 'Local network storage', 'Hard drive storage', 'B', 1, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Which is a cloud service provider?', 'Oracle', 'Amazon Web Services', 'IBM', 'All of above', 'D', 1, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What does IaaS stand for?', 'Information as a Service', 'Infrastructure as a Service', 'Internet as a Service', 'Integration as a Service', 'B', 1, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What are the three main cloud service models?', 'IaaS, PaaS, SaaS', 'IaaS, DaaS, FaaS', 'PaaS, MaaS, NaaS', 'SaaS, BaaS, CaaS', 'A', 1, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Which cloud deployment model offers the highest security?', 'Public Cloud', 'Private Cloud', 'Hybrid Cloud', 'Community Cloud', 'B', 1, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Explain the concept of multi-tenancy in cloud computing', 'Multiple servers in one data center', 'Multiple customers sharing same resources', 'Multiple storage devices', 'Multiple networks', 'B', 1, 3, 1);",
        
        // Database - 6 questions
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is a primary key?', 'A key for primary office', 'Unique identifier for a record', 'Password for database', 'Network key', 'B', 2, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Which language is used for database queries?', 'HTML', 'Python', 'SQL', 'JavaScript', 'C', 2, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is ACID in databases?', 'A chemical compound', 'Atomicity, Consistency, Isolation, Durability', 'A type of database', 'Access Control ID', 'B', 2, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is normalization?', 'Making database larger', 'Organizing data to reduce redundancy', 'Encrypting data', 'Backing up data', 'B', 2, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What does JOIN do?', 'Merges two tables', 'Combines rows from two tables', 'Deletes duplicate rows', 'Sorts table data', 'B', 2, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Describe the difference between INNER and OUTER JOIN', 'INNER keeps matching rows, OUTER keeps all rows', 'No difference', 'OUTER is faster', 'INNER is for deletion', 'A', 2, 3, 1);",
        
        // Networking - 6 questions
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is an IP address?', 'Internet Protocol number', 'Unique identifier for device on network', 'Computer name', 'WiFi password', 'B', 3, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('How many bits are in an IPv4 address?', '16 bits', '32 bits', '64 bits', '128 bits', 'B', 3, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is the purpose of a firewall?', 'To clean computer', 'To protect network from unauthorized access', 'To speed up internet', 'To store files', 'B', 3, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What are the seven layers of OSI model?', 'Physical, Data Link, Network, Transport, Session, Presentation, Application', 'Server, Client, Router, Switch, Cable, Modem, Internet', 'Only 3 layers', 'Only 5 layers', 'A', 3, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is TCP/IP?', 'A type of cable', 'Protocols for internet communication', 'A programming language', 'A network device', 'B', 3, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Explain what DNS does', 'Translates domain names to IP addresses', 'Encrypts network traffic', 'Manages network bandwidth', 'Controls firewall rules', 'A', 3, 3, 1);",
        
        // Programming - 6 questions
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What does OOP stand for?', 'Object Oriented Programming', 'Online Open Platform', 'Offline Operations Protocol', 'Object Output Processing', 'A', 4, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Which is not a programming paradigm?', 'Functional', 'Procedural', 'Algebraic', 'Object-Oriented', 'C', 4, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is a variable?', 'A mathematical equation', 'Named storage location for data', 'A function parameter', 'A constant value', 'B', 4, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is the difference between while and do-while loop?', 'No difference', 'do-while runs at least once', 'while is faster', 'do-while is deprecated', 'B', 4, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is recursion?', 'A programming error', 'Function calling itself', 'Loop structure', 'Variable declaration', 'B', 4, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Explain the concept of polymorphism', 'Many forms through inheritance/interfaces', 'Multiple variables', 'Several loops', 'Different data types', 'A', 4, 3, 1);",
        
        // Security - 6 questions
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is encryption?', 'Deleting sensitive data', 'Converting data into secret code', 'Backing up files', 'Organizing files', 'B', 5, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is a password attack called?', 'Network attack', 'Brute force attack', 'Server attack', 'Hardware attack', 'B', 5, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What does SSL stand for?', 'Secure Socket Layer', 'System Security License', 'Server Side Logic', 'Secure Storage Line', 'A', 5, 1, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is two-factor authentication?', 'Using two passwords', 'Combining two authentication methods', 'Two logins required', 'Two security questions', 'B', 5, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('What is phishing?', 'A fishing technique', 'Fraudulent attempt to obtain sensitive info', 'A type of malware', 'A network protocol', 'B', 5, 2, 1);",
        
        "INSERT INTO questions (text, option_a, option_b, option_c, option_d, correct_option, topic_id, difficulty_id, created_by) "
        "VALUES ('Explain what zero-day vulnerability means', 'No security issues', 'Unknown exploit before public disclosure', 'Old security flaw', 'Malware type', 'B', 5, 3, 1);"
    };
    
    int num_inserts = sizeof(inserts) / sizeof(inserts[0]);
    int executed = 0;
    
    for (int i = 0; i < num_inserts; i++) {
        char *err = NULL;
        int rc = sqlite3_exec(db, inserts[i], NULL, NULL, &err);
        
        if (rc == SQLITE_OK) {
            executed++;
        } else if (err) {
            fprintf(stderr, "[ERROR] Insert failed at statement %d: %s\n", i, err);
            sqlite3_free(err);
        }
    }
    
    fprintf(stderr, "[DEBUG] ✓ Loaded %d sample questions\n", executed);
    return 1;
}
