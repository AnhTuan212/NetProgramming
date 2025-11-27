#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define USER_FILE "data/users.txt"

int register_user_with_role(const char *username, const char *password, const char *role) {
    mkdir("data", 0755);
    if (validate_user(username, password, NULL)) return 0;  // Exists

    FILE *fp = fopen(USER_FILE, "a");
    if (!fp) return -1;
    fprintf(fp, "%s|%s|%s\n", username, password, role);  // DÙNG |
    fclose(fp);
    return 1;
}

int validate_user(const char *username, const char *password, char *role_out) {
    FILE *fp = fopen(USER_FILE, "r");
    if (!fp) return 0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char u[64], p[64], r[32] = "student";
        if (sscanf(line, "%63[^|]|%63[^|]|%31s", u, p, r) == 3) {
            if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
                if (role_out) strcpy(role_out, r);
                fclose(fp);
                return 1;
            }
        }
    }
    fclose(fp);
    return 0;
}