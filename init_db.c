#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "db_init.h"

// Forward declarations
int db_create_tables(void);

int main() {
    // Initialize database
    if (!db_init("test_system.db")) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    // Create all tables
    if (!db_create_tables()) {
        fprintf(stderr, "Failed to create tables\n");
        return 1;
    }
    
    printf("Database initialized successfully!\n");
    
    db_close();
    return 0;
}
