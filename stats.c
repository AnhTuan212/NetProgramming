#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <math.h>

extern sqlite3 *db;

void show_leaderboard(const char *output_file) {
    mkdir("data", 0755);
    
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Error: Could not open output file\n");
        return;
    }
    
    if (!db) {
        fprintf(out, "ERROR: Database not initialized\n");
        fclose(out);
        return;
    }
    
    fprintf(out, "=== GLOBAL LEADERBOARD (Avg Score) ===\n");
    fprintf(out, "Rank | User Name | Avg Score | Total Attempts\n");
    fprintf(out, "────────────────────────────────────────────\n");
    
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT u.username, AVG(r.score) as avg_score, COUNT(r.id) as attempts "
        "FROM users u "
        "LEFT JOIN results r ON u.id = r.participant_id "
        "GROUP BY u.id, u.username "
        "ORDER BY avg_score DESC, attempts DESC";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        int rank = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *username = (const char*)sqlite3_column_text(stmt, 0);
            double avg_score = sqlite3_column_double(stmt, 1);
            int attempts = sqlite3_column_int(stmt, 2);
            
            if (isnan(avg_score)) avg_score = 0.0;
            
            fprintf(out, "%d. %-20s | %.2f | %d\n", rank, username, avg_score, attempts);
            rank++;
        }
        sqlite3_finalize(stmt);
    } else {
        fprintf(out, "Error: %s\n", sqlite3_errmsg(db));
    }
    
    fclose(out);
}

void show_leaderboard_for_room(const char *room_name, const char *output_file) {
    mkdir("data", 0755);
    
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Error: Could not open output file\n");
        return;
    }
    
    if (!db) {
        fprintf(out, "ERROR: Database not initialized\n");
        fclose(out);
        return;
    }
    
    fprintf(out, "=== LEADERBOARD FOR ROOM: %s ===\n", room_name);
    fprintf(out, "Rank | User Name | Score | Date\n");
    fprintf(out, "────────────────────────────────\n");
    
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT u.username, r.score, r.total_questions, r.submitted_at "
        "FROM results r "
        "JOIN users u ON r.participant_id = u.id "
        "JOIN rooms rm ON r.room_id = rm.id "
        "WHERE rm.name = ? "
        "ORDER BY r.score DESC, r.submitted_at ASC";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, room_name, -1, SQLITE_STATIC);
        
        int rank = 1;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *username = (const char*)sqlite3_column_text(stmt, 0);
            int score = sqlite3_column_int(stmt, 1);
            int total = sqlite3_column_int(stmt, 2);
            const char *submitted_at = (const char*)sqlite3_column_text(stmt, 3);
            
            fprintf(out, "%d. %-20s | %d/%d | %s\n", rank, username, score, total, submitted_at);
            rank++;
        }
        sqlite3_finalize(stmt);
    } else {
        fprintf(out, "Error: %s\n", sqlite3_errmsg(db));
    }
    
    fclose(out);
}