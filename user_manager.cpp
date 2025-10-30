#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEN 128
const char *USER_FILE = "data/users.txt";

// Remove newline from end of string
static void trim_newline(char *s) {
    char *p = strchr(s, '\n');
    if (p) *p = '\0';
}

// Trim leading and trailing spaces
static char *trim_str(char *str) {
    if (!str) return str;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return str;
}

// Check if user exists
static int user_exists(const char *username) {
    FILE *fp = fopen(USER_FILE, "r");
    if (!fp) return 0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);
        char u[MAX_LEN];
        if (sscanf(line, "%127[^,],", u) == 1) {
            if (strcmp(trim_str(u), username) == 0) {
                fclose(fp);
                return 1;
            }
        }
    }
    fclose(fp);
    return 0;
}

// Validate user by username and password
int validate_user(const char *username, const char *password, char *role_out) {
    FILE *fp = fopen(USER_FILE, "r");
    if (!fp) {
        perror("Cannot open users.txt");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);
        if (strlen(line) == 0) continue;

        char u[MAX_LEN], p[MAX_LEN], r[MAX_LEN] = "student"; // Default role
        memset(u, 0, sizeof(u));
        memset(p, 0, sizeof(p));

        int n = sscanf(line, "%127[^,],%127[^,],%127s", u, p, r);
        if (n >= 2) {
            if (strcmp(trim_str(u), username) == 0 &&
                strcmp(trim_str(p), password) == 0) {
                
                strcpy(role_out, trim_str(r)); // Copy role
                fclose(fp);
                return 1; // login success
            }
        }
    }

    fclose(fp);
    return 0; // not found
}

// Register a new user
int register_user(const char *username, const char *password) {
    if (user_exists(username)) {
        return 0; // User already exists
    }

    FILE *fp = fopen(USER_FILE, "a");
    if (!fp) {
        perror("Cannot open users.txt for writing");
        return -1; // File error
    }

    // New users are "student" by default
    fprintf(fp, "%s,%s,student\n", username, password);
    fclose(fp);
    return 1; // Success
}