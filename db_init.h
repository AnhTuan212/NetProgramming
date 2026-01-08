#ifndef DB_INIT_H
#define DB_INIT_H

#include <sqlite3.h>
#include <pthread.h>

// Database connection (global for convenience)
extern sqlite3 *db;

// Initialize database and create tables
int db_init(const char *db_path);

// Close database connection
void db_close(void);

// Create database tables
int db_create_tables(void);

// Get database connection
sqlite3* db_get_connection(void);

// Thread-safe locking
void db_lock_acquire(void);
void db_lock_release(void);

// Initialize default difficulties
int db_init_default_difficulties(void);

// Database health check
int db_health_check(void);

// Load and execute SQL file
int load_sql_file(const char *sql_file_path);

#endif // DB_INIT_H
