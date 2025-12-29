#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

// ===== REMOVE: register_user_with_role() text file version =====

// ===== ADD: Database-only version =====

int register_user_with_role(const char *username, const char *password, const char *role) {
    // Check if user already exists
    if (db_username_exists(username)) {
        return 0;  // User exists
    }
    
    // Add to database
    int result = db_add_user(username, password, role);
    
    if (result > 0) {
        return 1;  // Success
    }
    
    return -1;  // Error
}

int validate_user(const char *username, const char *password, char *role_out) {
    // Query database
    if (!db_validate_user(username, password)) {
        return 0;  // Invalid credentials
    }
    
    // Get role
    if (role_out) {
        if (!db_get_user_role(username, role_out)) {
            return 0;
        }
    }
    
    return 1;  // Valid
}