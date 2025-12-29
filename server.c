#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/stat.h>
#include <strings.h>
#include <sqlite3.h>

#define ADMIN_CODE "network_programming"
#define PORT 9000
#define BUF_SIZE 8192
#define MAX_ROOMS 100
#define MAX_PARTICIPANTS 50
#define MAX_Q 200
#define MAX_ATTEMPTS 10

#define SCORE_NOT_SUBMITTED -1
#define SCORE_TIMEOUT_AUTO_SUBMIT -2

// ===== GLOBAL VARIABLES =====
Room rooms[MAX_ROOMS];
int roomCount = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

QItem practiceQuestions[MAX_Q];
int practiceQuestionCount = 0;

// ✅ FIXED: db is now defined in db_init.c, not here
extern sqlite3 *db;

// ===== FUNCTION PROTOTYPES =====
void writeLog(const char *event); 
int register_user_with_role(const char *username, const char *password, const char *role);
int validate_user(const char *username, const char *password, char *role_out);
int db_init(const char *db_path);
int db_create_tables(void);
int db_init_default_difficulties(void);
int db_load_sample_data(void);
void db_close(void);
int db_get_user_id(const char *username);
int db_add_question(const char *text, const char *A, const char *B, const char *C, const char *D, 
                    char correct, const char *topic, const char *difficulty, int creator_id);
int search_questions_by_id(int id, QItem *q);
int search_questions_by_topic(const char *topic, char *output);
int search_questions_by_difficulty(const char *difficulty, char *output);
int migrate_from_text_files(const char *data_dir);  // ✅ Changed from void to int
int verify_migration(void);                         // ✅ Changed from void to int
void to_lowercase(char *str);
void init_max_question_id(void);
int add_question_to_file(const QItem *q);
int delete_question_by_id(int id);
int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff);
int loadQuestionsWithFilters(const char *filename, QItem *questions, int maxQ,
                             const char *topic_filter, const char *diff_filter);
int validate_question_input(const QItem *q, char *error_msg);
int get_all_topics_with_counts(char *output);
int get_all_difficulties_with_counts(char *output);

// ===== DATABASE FUNCTIONS =====

void save_results_to_db() {
    pthread_mutex_lock(&lock);
    
    for (int i = 0; i < roomCount; i++) {
        Room *r = &rooms[i];
        for (int j = 0; j < r->participantCount; j++) {
            Participant *p = &r->participants[j];
            
            int user_id = db_get_user_id(p->username);
            if (user_id <= 0) {
                user_id = -1;
            }
            
            sqlite3_stmt *stmt;
            const char *room_query = "SELECT id FROM rooms WHERE name = ?";
            int room_id = -1;
            
            if (sqlite3_prepare_v2(db, room_query, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, r->name, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    room_id = sqlite3_column_int(stmt, 0);
                }
                sqlite3_finalize(stmt);
            }
            
            if (room_id > 0) {
                const char *result_query = 
                    "INSERT INTO results (participant_id, room_id, score, total_questions, correct_answers, submitted_at) "
                    "VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)";
                
                sqlite3_stmt *res_stmt;
                if (sqlite3_prepare_v2(db, result_query, -1, &res_stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(res_stmt, 1, user_id);
                    sqlite3_bind_int(res_stmt, 2, room_id);
                    sqlite3_bind_int(res_stmt, 3, p->score);
                    sqlite3_bind_int(res_stmt, 4, r->numQuestions);
                    sqlite3_bind_int(res_stmt, 5, p->score);
                    
                    if (sqlite3_step(res_stmt) != SQLITE_DONE) {
                        fprintf(stderr, "Error inserting result: %s\n", sqlite3_errmsg(db));
                    }
                    sqlite3_finalize(res_stmt);
                }
            }
        }
    }
    
    pthread_mutex_unlock(&lock);
}

int create_room_in_db(const char *name, const char *owner, const QItem *questions, int num_questions, int duration) {
    sqlite3_stmt *stmt;
    
    if (!questions || num_questions <= 0) {
        fprintf(stderr, "Invalid questions array\n");
        return 0;
    }
    
    int owner_id = db_get_user_id(owner);
    if (owner_id <= 0) {
        fprintf(stderr, "Owner user not found\n");
        return 0;
    }
    
    const char *query = 
        "INSERT INTO rooms (name, owner_id, duration_minutes, is_started, is_finished) "
        "VALUES (?, ?, ?, 0, 0)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, owner_id);
    sqlite3_bind_int(stmt, 3, duration);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE) {
        int room_id = (int)sqlite3_last_insert_rowid(db);
        
        // ✅ NEW: Insert actual question IDs from questions array
        for (int i = 0; i < num_questions && i < MAX_QUESTIONS_PER_ROOM; i++) {
            const char *rq_query = 
                "INSERT INTO room_questions (room_id, question_id, order_num) "
                "VALUES (?, ?, ?)";
            
            sqlite3_stmt *rq_stmt;
            if (sqlite3_prepare_v2(db, rq_query, -1, &rq_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int(rq_stmt, 1, room_id);
                sqlite3_bind_int(rq_stmt, 2, questions[i].id);
                sqlite3_bind_int(rq_stmt, 3, i);
                sqlite3_step(rq_stmt);
                sqlite3_finalize(rq_stmt);
            }
        }
        
        fprintf(stderr, "[DEBUG] Room '%s' created with %d questions in database\n", name, num_questions);
        return room_id;
    }
    
    return 0;
}

int load_rooms_from_db() {
    sqlite3_stmt *stmt;
    const char *query = 
        "SELECT id, name, owner_id, duration_minutes, is_started FROM rooms";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    roomCount = 0;
    
    while (sqlite3_step(stmt) == SQLITE_ROW && roomCount < MAX_ROOMS) {
        Room *r = &rooms[roomCount];
        
        int room_id = sqlite3_column_int(stmt, 0);
        strcpy(r->name, (const char*)sqlite3_column_text(stmt, 1));
        strcpy(r->owner, "");
        r->duration = sqlite3_column_int(stmt, 3);
        r->started = sqlite3_column_int(stmt, 4);
        
        sqlite3_stmt *q_stmt;
        const char *q_query = 
            "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
            "q.correct_option FROM questions q "
            "JOIN room_questions rq ON q.id = rq.question_id "
            "WHERE rq.room_id = ? ORDER BY rq.order_num";
        
        if (sqlite3_prepare_v2(db, q_query, -1, &q_stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(q_stmt, 1, room_id);
            
            r->numQuestions = 0;
            while (sqlite3_step(q_stmt) == SQLITE_ROW && r->numQuestions < MAX_QUESTIONS_PER_ROOM) {
                QItem *q = &r->questions[r->numQuestions];
                q->id = sqlite3_column_int(q_stmt, 0);
                strcpy(q->text, (const char*)sqlite3_column_text(q_stmt, 1));
                strcpy(q->A, (const char*)sqlite3_column_text(q_stmt, 2));
                strcpy(q->B, (const char*)sqlite3_column_text(q_stmt, 3));
                strcpy(q->C, (const char*)sqlite3_column_text(q_stmt, 4));
                strcpy(q->D, (const char*)sqlite3_column_text(q_stmt, 5));
                q->correct = *((char*)sqlite3_column_text(q_stmt, 6));
                
                r->numQuestions++;
            }
            
            sqlite3_finalize(q_stmt);
        }
        
        r->participantCount = 0;
        roomCount++;
    }
    
    sqlite3_finalize(stmt);
    return roomCount;
}

void trim_newline(char *s) {
    if (!s) return;
    char *p = strchr(s, '\n'); 
    if (p) *p = 0;
    p = strchr(s, '\r'); 
    if (p) *p = 0;
}

void send_msg(int sock, const char *msg) {
    if (!msg) return;
    char full[BUF_SIZE];
    snprintf(full, sizeof(full), "%s\n", msg);
    send(sock, full, strlen(full), 0);
}

Room* find_room(const char *name) {
    if (!name) return NULL;
    
    // First check in-memory array
    for (int i = 0; i < roomCount; i++)
        if (strcmp(rooms[i].name, name) == 0) return &rooms[i];
    
    // If not found in memory, try to load from database
    sqlite3_stmt *stmt;
    const char *query = "SELECT id, name, owner_id, duration_minutes FROM rooms WHERE name = ? LIMIT 1";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // Room exists in database - add to in-memory array
            if (roomCount < MAX_ROOMS) {
                Room *r = &rooms[roomCount++];
                strcpy(r->name, (const char*)sqlite3_column_text(stmt, 1));
                
                // Get owner name from user_id
                int owner_id = sqlite3_column_int(stmt, 2);
                sqlite3_stmt *owner_stmt;
                if (sqlite3_prepare_v2(db, "SELECT username FROM users WHERE id = ?", -1, &owner_stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(owner_stmt, 1, owner_id);
                    if (sqlite3_step(owner_stmt) == SQLITE_ROW) {
                        strcpy(r->owner, (const char*)sqlite3_column_text(owner_stmt, 0));
                    }
                    sqlite3_finalize(owner_stmt);
                }
                
                r->duration = sqlite3_column_int(stmt, 3);
                r->started = 1;
                r->start_time = time(NULL);
                r->participantCount = 0;
                r->numQuestions = 0;  // Will be loaded on first JOIN
                
                fprintf(stderr, "[DEBUG] Loaded room '%s' from database into memory\n", name);
                sqlite3_finalize(stmt);
                return r;
            }
        }
        sqlite3_finalize(stmt);
    }
    
    return NULL;  // Room not found in database either
}

// ✅ NEW: Load questions for a specific room from database
int load_questions_for_room(Room *r) {
    if (!r || !db) return -1;
    
    // Get room_id from database
    sqlite3_stmt *room_stmt;
    const char *room_query = "SELECT id FROM rooms WHERE name = ?";
    
    if (sqlite3_prepare_v2(db, room_query, -1, &room_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Could not prepare room query: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_text(room_stmt, 1, r->name, -1, SQLITE_TRANSIENT);
    
    if (sqlite3_step(room_stmt) != SQLITE_ROW) {
        fprintf(stderr, "[ERROR] Room '%s' not found in database\n", r->name);
        sqlite3_finalize(room_stmt);
        return -1;
    }
    
    int room_id = sqlite3_column_int(room_stmt, 0);
    sqlite3_finalize(room_stmt);
    
    // Load questions for this room
    sqlite3_stmt *q_stmt;
    const char *q_query = 
        "SELECT q.id, q.text, q.option_a, q.option_b, q.option_c, q.option_d, "
        "q.correct_option FROM questions q "
        "JOIN room_questions rq ON q.id = rq.question_id "
        "WHERE rq.room_id = ? ORDER BY rq.order_num";
    
    if (sqlite3_prepare_v2(db, q_query, -1, &q_stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Could not prepare questions query: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int(q_stmt, 1, room_id);
    
    r->numQuestions = 0;
    while (sqlite3_step(q_stmt) == SQLITE_ROW && r->numQuestions < MAX_QUESTIONS_PER_ROOM) {
        QItem *q = &r->questions[r->numQuestions];
        q->id = sqlite3_column_int(q_stmt, 0);
        strcpy(q->text, (const char*)sqlite3_column_text(q_stmt, 1));
        strcpy(q->A, (const char*)sqlite3_column_text(q_stmt, 2));
        strcpy(q->B, (const char*)sqlite3_column_text(q_stmt, 3));
        strcpy(q->C, (const char*)sqlite3_column_text(q_stmt, 4));
        strcpy(q->D, (const char*)sqlite3_column_text(q_stmt, 5));
        q->correct = *((char*)sqlite3_column_text(q_stmt, 6));
        
        r->numQuestions++;
    }
    
    sqlite3_finalize(q_stmt);
    
    fprintf(stderr, "[DEBUG] Loaded %d questions for room '%s'\n", r->numQuestions, r->name);
    return r->numQuestions;
}

Participant* find_participant(Room *r, const char *user) {
    if (!r || !user) return NULL;
    for (int i = 0; i < r->participantCount; i++)
        if (strcmp(r->participants[i].username, user) == 0) return &r->participants[i];
    return NULL;
}

void* monitor_exam_thread(void *arg) {
    (void)arg;
    while (1) {
        sleep(1);
        pthread_mutex_lock(&lock);
        time_t now = time(NULL);
        
        for (int i = 0; i < roomCount; i++) {
            Room *r = &rooms[i];
            for (int j = 0; j < r->participantCount; j++) {
                Participant *p = &r->participants[j];
                int elapsed = now - p->start_time;
                
                if (elapsed > r->duration && p->score == SCORE_NOT_SUBMITTED) {
                    int score = 0;
                    for (int k = 0; k < r->numQuestions; k++) {
                        // ✅ FIXED: Complete the condition
                        if (p->answers[k] != '.' && 
                            toupper(p->answers[k]) == r->questions[k].correct) {
                            score++;
                        }
                    }
                    
                    if (p->history_count < MAX_ATTEMPTS) {
                        p->score_history[p->history_count++] = score;
                    }
                    
                    p->score = score;
                    p->submit_time = now;
                    save_results_to_db();
                }
            }
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void* handle_client(void *arg) {
    Client *cli = (Client*)arg;
    if (!cli) return NULL;
    
    char buffer[BUF_SIZE];
    char log_msg[512];
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(cli->sock, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) break;
        trim_newline(buffer);
        
        char cmd[32];
        sscanf(buffer, "%31s", cmd);

        pthread_mutex_lock(&lock);

        if (strcmp(cmd, "REGISTER") == 0) {
            char user[64], pass[64], role[32] = "student";
            char code[64] = "";
            int args = sscanf(buffer, "REGISTER %63s %63s %31s %63s", user, pass, role, code);
            
            if (args < 2) {
                send_msg(cli->sock, "FAIL Usage: REGISTER <username> <password> [role] [code]");
            } else {
                if (strcasecmp(role, "admin") != 0 && strcasecmp(role, "student") != 0) 
                    strcpy(role, "student");
                
                int authorized = 1;
                if (strcasecmp(role, "admin") == 0) {
                    if (strcmp(code, ADMIN_CODE) != 0) authorized = 0;
                }
                
                if (!authorized) {
                    send_msg(cli->sock, "FAIL Invalid Admin Secret Code!");
                    sprintf(log_msg, "Register failed for admin %s (Wrong Code)", user);
                    writeLog(log_msg);
                } else {
                    int result = register_user_with_role(user, pass, role);
                    if (result == 1) {
                        send_msg(cli->sock, "SUCCESS Registered. Please login.");
                        sprintf(log_msg, "User %s registered as %s", user, role);
                        writeLog(log_msg);
                    } else if (result == 0) {
                        send_msg(cli->sock, "FAIL User already exists");
                    } else {
                        send_msg(cli->sock, "FAIL Server error");
                    }
                }
            }
        }        
        else if (strcmp(cmd, "LOGIN") == 0) {
            char user[64], pass[64], role[32] = "student";
            sscanf(buffer, "LOGIN %63s %63s", user, pass);
            
            if (validate_user(user, pass, role)) {
                strcpy(cli->username, user);
                strcpy(cli->role, role);
                cli->user_id = db_get_user_id(user);
                cli->loggedIn = 1;
                
                char msg[128];
                sprintf(msg, "SUCCESS %s", role);
                send_msg(cli->sock, msg);
                
                sprintf(log_msg, "User %s logged in as %s", user, role);
                writeLog(log_msg);
            } else {
                send_msg(cli->sock, "FAIL Invalid credentials");
            }
        }
        else if (!cli->loggedIn) {
            send_msg(cli->sock, "FAIL Please login first");
        }
        else if (strcmp(cmd, "CREATE") == 0 && strcmp(cli->role, "admin") == 0) {
            char name[64], rest[512] = "";
            int numQ, dur;
            char topic_filter[256] = "", diff_filter[256] = "";
            
            sscanf(buffer, "CREATE %63s %d %d %511s", name, &numQ, &dur, rest);
            
            if (strlen(rest) > 0) {
                char rest_copy[512];
                strcpy(rest_copy, rest);
                
                char *topics_start = strstr(rest_copy, "TOPICS");
                char *diffs_start = strstr(rest_copy, "DIFFICULTIES");
                
                if (topics_start) {
                    topics_start += 6;
                    while (*topics_start == ' ') topics_start++;
                    
                    int topic_len = 0;
                    if (diffs_start) {
                        topic_len = diffs_start - topics_start - 1;
                    } else {
                        topic_len = strlen(topics_start);
                    }
                    strncpy(topic_filter, topics_start, topic_len);
                    topic_filter[topic_len] = '\0';
                }
                
                if (diffs_start) {
                    diffs_start += 12;
                    while (*diffs_start == ' ') diffs_start++;
                    strcpy(diff_filter, diffs_start);
                }
            }
            
            if (numQ < 1 || numQ > MAX_QUESTIONS_PER_ROOM) {
                send_msg(cli->sock, "FAIL Number of questions must be 1-50");
            } else if (dur < 10 || dur > 86400) {
                send_msg(cli->sock, "FAIL Duration must be 10-86400 seconds");
            } else if (find_room(name)) {
                send_msg(cli->sock, "FAIL Room already exists");
            } else {
                QItem temp_questions[MAX_QUESTIONS_PER_ROOM];
                int loaded = loadQuestionsWithMultipleFilters(temp_questions, numQ,
                                                      strlen(topic_filter) > 0 ? topic_filter : NULL,
                                                      strlen(diff_filter) > 0 ? diff_filter : NULL);
                
                if (loaded == 0) {
                    send_msg(cli->sock, "FAIL No questions match your criteria");
                } else {
                    int room_id = create_room_in_db(name, cli->username, temp_questions, loaded, dur);
                    
                    if (room_id > 0) {
                        Room *r = &rooms[roomCount++];
                        strcpy(r->name, name);
                        strcpy(r->owner, cli->username);
                        r->duration = dur;
                        r->started = 1;
                        r->start_time = time(NULL);
                        r->participantCount = 0;
                        r->numQuestions = loaded;
                        memcpy(r->questions, temp_questions, loaded * sizeof(QItem));
                        
                        sprintf(log_msg, "Admin %s created room %s with %d questions", 
                                cli->username, name, loaded);
                        writeLog(log_msg);
                        
                        send_msg(cli->sock, "SUCCESS Room created");
                    } else {
                        // ✅ FIXED: Complete the else block
                        send_msg(cli->sock, "FAIL Failed to create room in database");
                    }
                }
            }
        }
        else if (strcmp(cmd, "LIST") == 0) {
            char msg[4096] = "SUCCESS Rooms:\n";
            if (roomCount == 0) {
                strcat(msg, "No rooms.\n");
            } else {
                for (int i = 0; i < roomCount; i++) {
                    char line[256];
                    sprintf(line, "- %s (Owner: %s, Q: %d, Time: %ds)\n",
                            rooms[i].name, rooms[i].owner, rooms[i].numQuestions, rooms[i].duration);
                    strcat(msg, line);
                }
            }
            send_msg(cli->sock, msg);
        }
        else if (strcmp(cmd, "JOIN") == 0) {
            char name[64];
            sscanf(buffer, "JOIN %63s", name);
            
            Room *r = find_room(name);
            if (!r) {
                send_msg(cli->sock, "FAIL Room not found");
            } else {
                // ✅ NEW: Load questions if not already loaded
                if (r->numQuestions == 0) {
                    fprintf(stderr, "[DEBUG] Room '%s' has 0 questions in memory, loading from database...\n", name);
                    if (load_questions_for_room(r) <= 0) {
                        send_msg(cli->sock, "FAIL Could not load questions for room");
                        goto join_failed;
                    }
                }
                
                Participant *p = find_participant(r, cli->username);
                if (!p) {
                    p = &r->participants[r->participantCount++];
                    strcpy(p->username, cli->username);
                    p->score = SCORE_NOT_SUBMITTED;
                    p->history_count = 0;
                    memset(p->answers, '.', MAX_QUESTIONS_PER_ROOM);
                    p->submit_time = 0;
                    p->start_time = time(NULL);
                } else {
                    if (p->score != SCORE_NOT_SUBMITTED) { 
                        if (p->history_count < MAX_ATTEMPTS) {
                            p->score_history[p->history_count++] = p->score;
                        }
                        p->score = SCORE_NOT_SUBMITTED;
                        memset(p->answers, '.', MAX_QUESTIONS_PER_ROOM);
                        p->submit_time = 0;
                        p->start_time = time(NULL);
                    }
                }
                
                int elapsed = (int)(time(NULL) - p->start_time);
                int remaining = r->duration - elapsed;
                if (remaining < 0) remaining = 0;

                char msg[128];
                sprintf(msg, "SUCCESS %d %d", r->numQuestions, remaining);
                send_msg(cli->sock, msg);
                
                sprintf(log_msg, "User %s joined room %s", cli->username, name);
                writeLog(log_msg);
            }
            join_failed:;
        }
        else if (strcmp(cmd, "GET_QUESTION") == 0) {
            char name[64];
            int idx;
            sscanf(buffer, "GET_QUESTION %63s %d", name, &idx);
            
            Room *r = find_room(name);
            if (!r || idx < 0 || idx >= r->numQuestions) {
                send_msg(cli->sock, "FAIL Invalid question");
            } else {
                Participant *p = find_participant(r, cli->username);
                QItem *q = &r->questions[idx];
                
                char currentAns = ' ';
                if (p && p->score == SCORE_NOT_SUBMITTED) {
                    currentAns = p->answers[idx];
                    if (currentAns == '.') currentAns = ' ';
                }

                char temp[BUF_SIZE];
                snprintf(temp, sizeof(temp),
                         "[%d/%d] %s\nA) %s\nB) %s\nC) %s\nD) %s\n\n[Your Selection: %c]\n",
                         idx+1, r->numQuestions, q->text, q->A, q->B, q->C, q->D, 
                         currentAns);
                send_msg(cli->sock, temp);
            }
        }
        else if (strcmp(cmd, "ANSWER") == 0) {
            char roomName[64];
            char ansChar;
            int qIdx;
            sscanf(buffer, "ANSWER %63s %d %c", roomName, &qIdx, &ansChar);
            
            Room *r = find_room(roomName);
            if (r) {
                Participant *p = find_participant(r, cli->username);
                if (p && p->score == SCORE_NOT_SUBMITTED) {
                    if (qIdx >= 0 && qIdx < r->numQuestions) {
                        p->answers[qIdx] = toupper(ansChar);
                    }
                }
            }
        }
        else if (strcmp(cmd, "SUBMIT") == 0) {
            char name[64], ans[256];
            sscanf(buffer, "SUBMIT %63s %255s", name, ans);
            
            Room *r = find_room(name);
            
            if (!r) {
                send_msg(cli->sock, "FAIL Room not found");
            } else {
                Participant *p = find_participant(r, cli->username);
                if (!p || p->score != SCORE_NOT_SUBMITTED) {
                    send_msg(cli->sock, "FAIL Not in room or already submitted");
                } else {
                    int score = 0;
                    for (int i = 0; i < r->numQuestions && i < (int)strlen(ans); i++) {
                        if (ans[i] != '.' && toupper(ans[i]) == r->questions[i].correct) {
                            score++;
                        }
                    }
                    
                    if (p->history_count < MAX_ATTEMPTS) {
                        p->score_history[p->history_count++] = score;
                    }
                    
                    p->score = score;
                    p->submit_time = time(NULL);
                    strcpy(p->answers, ans);
                    
                    save_results_to_db();
                    
                    sprintf(log_msg, "User %s submitted in room %s: %d/%d", 
                            cli->username, name, score, r->numQuestions);
                    writeLog(log_msg);
                    
                    char msg[128];
                    sprintf(msg, "SUCCESS Score: %d/%d", score, r->numQuestions);
                    send_msg(cli->sock, msg);
                }
            }
        }
        else if (strcmp(cmd, "RESULTS") == 0) {
            char name[64];
            sscanf(buffer, "RESULTS %63s", name);
            
            Room *r = find_room(name);
            if (!r) {
                send_msg(cli->sock, "FAIL Room not found");
            } else {
                char msg[4096] = "SUCCESS Results:\n";
                for (int i = 0; i < r->participantCount; i++) {
                    Participant *p = &r->participants[i];
                    char line[512];
                    char historyStr[256] = "";
                    
                    for (int k = 0; k < p->history_count; k++) {
                        char tmp[32];
                        sprintf(tmp, "Att%d:%d/%d ", k+1, p->score_history[k], r->numQuestions);
                        strcat(historyStr, tmp);
                    }
                    
                    if (p->score != SCORE_NOT_SUBMITTED) {
                        char tmp[64];
                        sprintf(tmp, "Latest:%d/%d", p->score, r->numQuestions);
                        strcat(historyStr, tmp);
                    } else {
                        strcat(historyStr, "Doing...");
                    }
                    
                    sprintf(line, "- %s | %s\n", p->username, historyStr);
                    strcat(msg, line);
                }
                send_msg(cli->sock, msg);
            }
        }
        else if (strcmp(cmd, "PREVIEW") == 0 && strcmp(cli->role, "admin") == 0) {
            char name[64];
            sscanf(buffer, "PREVIEW %63s", name);
            
            Room *r = find_room(name);
            if (!r) {
                send_msg(cli->sock, "FAIL Room not found");
            } else if (strcmp(r->owner, cli->username) != 0) {
                send_msg(cli->sock, "FAIL Not your room");
            } else {
                char msg[BUF_SIZE] = "SUCCESS Preview:\n";
                for (int i = 0; i < r->numQuestions; i++) {
                    QItem *q = &r->questions[i];
                    char line[1024];
                    snprintf(line, sizeof(line),
                             "[%d/%d] %s\nA) %s\nB) %s\nC) %s\nD) %s\nCorrect: %c\n\n",
                             i+1, r->numQuestions, q->text, q->A, q->B, q->C, q->D, q->correct);
                    strcat(msg, line);
                }
                send_msg(cli->sock, msg);
            }
        }
        else if (strcmp(cmd, "DELETE") == 0 && strcmp(cli->role, "admin") == 0) {
            char name[64];
            sscanf(buffer, "DELETE %63s", name);
            
            Room *r = find_room(name);
            if (!r) {
                send_msg(cli->sock, "FAIL Room not found");
            } else if (strcmp(r->owner, cli->username) != 0) {
                send_msg(cli->sock, "FAIL Not your room");
            } else {
                int idx = r - rooms;
                for (int i = idx; i < roomCount - 1; i++) {
                    rooms[i] = rooms[i + 1];
                }
                roomCount--;
                
                sprintf(log_msg, "Admin %s deleted room %s", cli->username, name);
                writeLog(log_msg);
                
                send_msg(cli->sock, "SUCCESS Room deleted");
            }
        }
        else if (strcmp(cmd, "LEADERBOARD") == 0) {
            char room_filter[64] = "";
            sscanf(buffer, "LEADERBOARD %63s", room_filter);
            
            // ✅ Get leaderboard directly from database, no file I/O
            char lboard[8192] = "SUCCESS\n";
            
            if (strlen(room_filter) == 0) {
                // Global leaderboard
                sqlite3_stmt *stmt;
                const char *query = 
                    "SELECT u.username, COUNT(r.id) as rooms, AVG(r.score) as avg_score "
                    "FROM users u "
                    "LEFT JOIN (SELECT p.user_id, res.score FROM participants p "
                    "           JOIN results res ON p.id = res.participant_id) r ON u.id = r.user_id "
                    "GROUP BY u.id ORDER BY avg_score DESC LIMIT 20";
                
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *username = (const char*)sqlite3_column_text(stmt, 0);
                        int rooms = sqlite3_column_int(stmt, 1);
                        double avg_score = sqlite3_column_double(stmt, 2);
                        
                        char line[256];
                        snprintf(line, sizeof(line), "%s: %d rooms, avg %.1f%%\n", 
                                username, rooms, avg_score * 100);
                        if (strlen(lboard) + strlen(line) < sizeof(lboard)) {
                            strcat(lboard, line);
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            } else {
                // Room-specific leaderboard
                sqlite3_stmt *stmt;
                char query[1024];
                snprintf(query, sizeof(query),
                    "SELECT u.username, res.score, res.total_questions "
                    "FROM participants p "
                    "JOIN users u ON p.user_id = u.id "
                    "JOIN results res ON p.id = res.participant_id "
                    "JOIN rooms r ON res.room_id = r.id "
                    "WHERE r.name = ? "
                    "ORDER BY res.score DESC");
                
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, room_filter, -1, SQLITE_TRANSIENT);
                    
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *username = (const char*)sqlite3_column_text(stmt, 0);
                        int score = sqlite3_column_int(stmt, 1);
                        int total = sqlite3_column_int(stmt, 2);
                        
                        char line[256];
                        snprintf(line, sizeof(line), "%s: %d/%d\n", username, score, total);
                        if (strlen(lboard) + strlen(line) < sizeof(lboard)) {
                            strcat(lboard, line);
                        }
                    }
                    sqlite3_finalize(stmt);
                }
            }
            
            send_msg(cli->sock, lboard);
        }
        else if (strcmp(cmd, "LEADERBOARD_GLOBAL") == 0) {
            // ✅ Global leaderboard from database, no file I/O
            char lboard[8192] = "SUCCESS\n";
            
            sqlite3_stmt *stmt;
            const char *query = 
                "SELECT u.username, COUNT(DISTINCT res.room_id) as rooms, "
                "       AVG(CAST(res.score AS FLOAT) / res.total_questions) as avg_score "
                "FROM users u "
                "LEFT JOIN participants p ON u.id = p.user_id "
                "LEFT JOIN results res ON p.id = res.participant_id "
                "GROUP BY u.id ORDER BY avg_score DESC LIMIT 20";
            
            if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char *username = (const char*)sqlite3_column_text(stmt, 0);
                    int rooms = sqlite3_column_int(stmt, 1);
                    double avg_score = sqlite3_column_double(stmt, 2);
                    
                    char line[256];
                    snprintf(line, sizeof(line), "%s: %d rooms, avg %.1f%%\n", 
                            username, rooms, avg_score * 100);
                    if (strlen(lboard) + strlen(line) < sizeof(lboard)) {
                        strcat(lboard, line);
                    }
                }
                sqlite3_finalize(stmt);
            }
            
            send_msg(cli->sock, lboard);
        }
        else if (strcmp(cmd, "PRACTICE") == 0) {
            if (practiceQuestionCount == 0) {
                send_msg(cli->sock, "FAIL No practice questions available");
            } else {
                int idx = rand() % practiceQuestionCount;
                QItem *q = &practiceQuestions[idx];
                char temp[BUF_SIZE];
                snprintf(temp, sizeof(temp),
                         "PRACTICE_Q [%d/%d] %s\nA) %s\nB) %s\nC) %s\nD) %s\nCorrect: %c\n",
                         idx+1, practiceQuestionCount, q->text, q->A, q->B, q->C, q->D, q->correct);
                send_msg(cli->sock, temp);
            }
        }
        else if (strcmp(cmd, "GET_TOPICS") == 0) {
            char topics_output[2048];
            if (get_all_topics_with_counts(topics_output) == 0) {
                char msg[2100];  // ✅ Increased size
                snprintf(msg, sizeof(msg), "SUCCESS %s", topics_output);
                send_msg(cli->sock, msg);
            } else {
                send_msg(cli->sock, "FAIL Could not retrieve topics");
            }
        }
        else if (strcmp(cmd, "GET_DIFFICULTIES") == 0) {
            char diff_output[1024];
            if (get_all_difficulties_with_counts(diff_output) == 0) {
                char msg[1100];  // ✅ Increased size
                snprintf(msg, sizeof(msg), "SUCCESS %s", diff_output);
                send_msg(cli->sock, msg);
            } else {
                send_msg(cli->sock, "FAIL Could not retrieve difficulties");
            }
        }
        else if (strcmp(cmd, "GET_DIFFICULTIES_FOR_TOPICS") == 0) {
            // Extract everything after "GET_DIFFICULTIES_FOR_TOPICS " (including spaces)
            const char *prefix = "GET_DIFFICULTIES_FOR_TOPICS ";
            const char *topics_str = buffer + strlen(prefix);
            
            if (strlen(topics_str) == 0) {
                send_msg(cli->sock, "FAIL No topics specified");
            } else {
                fprintf(stderr, "[DEBUG] Received topic filter: '%s'\n", topics_str);
                // Use new function to get difficulties filtered by selected topics
                char diff_output[256];
                if (get_difficulties_for_topics(topics_str, diff_output) == 0) {
                    char response[512];
                    snprintf(response, sizeof(response), "SUCCESS %s", diff_output);
                    fprintf(stderr, "[DEBUG] diff_output: %s\n", diff_output);
                    fprintf(stderr, "[DEBUG] response: %s\n", response);
                    send_msg(cli->sock, response);
                } else {
                    send_msg(cli->sock, "FAIL Could not retrieve difficulties");
                }
            }
        }
        else if (strcmp(cmd, "ADD_QUESTION") == 0 && strcmp(cli->role, "admin") == 0) {
            char text[256], A[128], B[128], C[128], D[128];
            char correct_str[2], topic[64], difficulty[32];
            
            int parsed = sscanf(buffer, 
                "ADD_QUESTION %255[^|]|%127[^|]|%127[^|]|%127[^|]|%127[^|]|%1[^|]|%63[^|]|%31s",
                text, A, B, C, D, correct_str, topic, difficulty);
            
            if (parsed != 8) {
                send_msg(cli->sock, "FAIL Invalid format");
            } else {
                QItem new_q;
                memset(&new_q, 0, sizeof(QItem));
                strncpy(new_q.text, text, sizeof(new_q.text)-1);
                strncpy(new_q.A, A, sizeof(new_q.A)-1);
                strncpy(new_q.B, B, sizeof(new_q.B)-1);
                strncpy(new_q.C, C, sizeof(new_q.C)-1);
                strncpy(new_q.D, D, sizeof(new_q.D)-1);
                new_q.correct = toupper(correct_str[0]);
                strncpy(new_q.topic, topic, sizeof(new_q.topic)-1);
                strncpy(new_q.difficulty, difficulty, sizeof(new_q.difficulty)-1);
                
                char error_msg[256];
                if (!validate_question_input(&new_q, error_msg)) {
                    send_msg(cli->sock, error_msg);
                } else {
                    // ✅ Call database directly
                    to_lowercase(new_q.topic);
                    to_lowercase(new_q.difficulty);
                    int new_id = db_add_question(new_q.text, new_q.A, new_q.B, new_q.C, new_q.D, 
                                                  new_q.correct, new_q.topic, new_q.difficulty, cli->user_id);
                    
                    if (new_id <= 0) {
                        send_msg(cli->sock, "FAIL Could not add question");
                    } else {
                        // Reload practice questions from database
                        practiceQuestionCount = loadQuestionsFromDB(practiceQuestions, MAX_Q, NULL, NULL);
                        
                        // Log the action
                        sprintf(log_msg, "Admin %s added question ID %d: %s/%s", 
                                cli->username, new_id, new_q.topic, new_q.difficulty);
                        writeLog(log_msg);
                        
                        char msg[128];
                        sprintf(msg, "SUCCESS Question ID %d added", new_id);
                        send_msg(cli->sock, msg);
                    }
                }
            }
        }
        else if (strcmp(cmd, "SEARCH_QUESTIONS") == 0 && strcmp(cli->role, "admin") == 0) {
            char filter_type[32], search_value[256];
            sscanf(buffer, "SEARCH_QUESTIONS %31s %255[^\n]", filter_type, search_value);
            
            char result[8192] = "SUCCESS ";
            int count = 0;
            
            if (strcmp(filter_type, "id") == 0) {
                int id = atoi(search_value);
                QItem q;
                if (search_questions_by_id(id, &q)) {
                    sprintf(result + strlen(result), "%d|%s|%s|%s|%s|%s|%c|%s|%s",
                            q.id, q.text, q.A, q.B, q.C, q.D, q.correct, q.topic, q.difficulty);
                    count = 1;
                } else {
                    strcpy(result, "FAIL No question found with that ID");
                }
            }
            else if (strcmp(filter_type, "topic") == 0) {
                char output[8192];
                count = search_questions_by_topic(search_value, output);
                if (count > 0) {
                    strcat(result, output);
                } else {
                    strcpy(result, "FAIL No questions found");
                }
            }
            else if (strcmp(filter_type, "difficulty") == 0) {
                char output[8192];
                count = search_questions_by_difficulty(search_value, output);
                if (count > 0) {
                    strcat(result, output);
                } else {
                    strcpy(result, "FAIL No questions found");
                }
            }
            else {
                strcpy(result, "FAIL Invalid filter type");
            }
            
            send_msg(cli->sock, result);
        }
        else if (strcmp(cmd, "DELETE_QUESTION") == 0 && strcmp(cli->role, "admin") == 0) {
            int question_id;
            sscanf(buffer, "DELETE_QUESTION %d", &question_id);
            
            QItem q;
            if (!search_questions_by_id(question_id, &q)) {
                send_msg(cli->sock, "FAIL Question not found");
            } else {
                // ✅ Delete from database
                if (delete_question_by_id(question_id)) {
                    // Reload practice questions from database
                    practiceQuestionCount = loadQuestionsFromDB(practiceQuestions, MAX_Q, NULL, NULL);
                    
                    // Log the action
                    sprintf(log_msg, "Admin %s deleted question ID %d", cli->username, question_id);
                    writeLog(log_msg);
                    
                    char msg[128];
                    sprintf(msg, "SUCCESS Question ID %d deleted", question_id);
                    send_msg(cli->sock, msg);
                } else {
                    send_msg(cli->sock, "FAIL Could not delete question");
                }
            }
        }
        else if (strcmp(cmd, "EXIT") == 0) {
            send_msg(cli->sock, "SUCCESS Goodbye");
            pthread_mutex_unlock(&lock); 
            break;
        }
        else {
            send_msg(cli->sock, "FAIL Unknown command");
        }
        
        pthread_mutex_unlock(&lock);
    }
    
    close(cli->sock);
    free(cli);
    return NULL;
}

int main() {
    writeLog("SERVER_STARTED");
    
    pthread_mutex_init(&lock, NULL);
    srand(time(NULL));
    
    mkdir("data", 0755);
    
    printf("Initializing SQLite database...\n");
    if (!db_init(DB_PATH)) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    if (!db_create_tables()) {
        fprintf(stderr, "Failed to create tables\n");
        db_close();
        return 1;
    }
    
    if (!db_init_default_difficulties()) {
        fprintf(stderr, "Failed to initialize difficulties\n");
        db_close();
        return 1;
    }
    
    // ✅ Insert default admin user if not exists
    sqlite3_stmt *stmt;
    const char *check_admin = "SELECT COUNT(*) FROM users WHERE username = 'admin'";
    if (sqlite3_prepare_v2(db, check_admin, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
            // Admin doesn't exist, insert it
            db_add_user("admin", "admin123", "admin");
            fprintf(stderr, "[DEBUG] Default admin user created\n");
        }
        sqlite3_finalize(stmt);
    }
    
    printf("Database initialized successfully\n");
    
    init_max_question_id();
    
    // ✅ Load sample questions from database
    db_load_sample_data();
    
    practiceQuestionCount = loadQuestionsFromDB(practiceQuestions, MAX_Q, NULL, NULL);
    printf("✓ Loaded %d practice questions from database\n", practiceQuestionCount);
    
    pthread_t mon_tid;
    pthread_create(&mon_tid, NULL, monitor_exam_thread, NULL);
    pthread_detach(mon_tid);

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }
    
    struct sockaddr_in addr = { 
        .sin_family = AF_INET, 
        .sin_port = htons(PORT), 
        .sin_addr.s_addr = INADDR_ANY 
    };
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    
    listen(server_sock, 10);
    printf("Server running on port %d\n", PORT);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        int cli_sock = accept(server_sock, (struct sockaddr*)&cli_addr, &len);
        
        if (cli_sock >= 0) {
            Client *cli = malloc(sizeof(Client));
            cli->sock = cli_sock; 
            cli->loggedIn = 0;
            cli->user_id = -1;
            memset(cli->username, 0, sizeof(cli->username));
            memset(cli->role, 0, sizeof(cli->role));
            
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, cli);
            pthread_detach(tid);
        }
    }
    
    close(server_sock);
    db_close();
    return 0;
}