#include "common.h"
#include "db_queries.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 🔧 FIX: Use database for user registration (no more users.txt)
int register_user_with_role(const char *username, const char *password, const char *role) {
    if (!username || !password || !role) return -1;
    
    // Check if user already exists in database
    if (db_username_exists(username)) {
        printf("[DEBUG] User '%s' already exists in database\n", username);
        return 0;  // User exists
    }
    
    // Add user to database
    int user_id = db_add_user(username, password, role);
    
    if (user_id > 0) {
        printf("[DEBUG] User '%s' registered in database (id=%d, role=%s)\n", username, user_id, role);
        return 1;  // Success
    } else {
        printf("[ERROR] Failed to register user '%s' in database\n", username);
        return -1;  // Database error
    }
}

// 🔧 FIX: Use database for user validation (no more users.txt)
int validate_user(const char *username, const char *password, char *role_out) {
    if (!username || !password) return 0;
    
    // Validate user credentials from database
    if (!db_validate_user(username, password)) {
        printf("[DEBUG] Login failed: invalid credentials for '%s'\n", username);
        return 0;  // Invalid credentials
    }
    
    // Get user's role from database
    if (role_out) {
        if (!db_get_user_role(username, role_out)) {
            printf("[ERROR] Failed to get role for user '%s'\n", username);
            strcpy(role_out, "student");  // Default fallback
        }
    }
    
    printf("[DEBUG] User '%s' validated successfully (role=%s)\n", username, role_out ? role_out : "?");
    return 1;  // Valid user
}