#ifndef DB_MIGRATION_H
#define DB_MIGRATION_H

// Migrate topics from questions.txt
int migrate_topics(const char *questions_file);

// Migrate questions from questions.txt
int migrate_questions(const char *questions_file);

// Migrate users from users.txt
int migrate_users(const char *users_file);

// Main migration function
int migrate_from_text_files(const char *data_dir);

// Verify migration completed successfully
int verify_migration(void);

// Load and execute SQL file
int load_sql_file(const char *sql_file_path);

#endif // DB_MIGRATION_H
