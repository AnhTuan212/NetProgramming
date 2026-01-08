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

// Note: All data is now persisted in SQLite database (test_system.db)
// No longer using text files for rooms/results (deprecated)

typedef struct {
    char username[64];
    int db_id;                           // Database participant ID
    int score; 
    char answers[MAX_QUESTIONS_PER_ROOM];
    int score_history[MAX_ATTEMPTS]; 
    int history_count; 
    time_t submit_time;
    time_t start_time;
} Participant;

typedef struct {
    int db_id;                           // Database room ID for persistence
    char name[64];
    char owner[64];
    int numQuestions;
    int duration;
    QItem questions[MAX_QUESTIONS_PER_ROOM];
    Participant participants[MAX_PARTICIPANTS];
    int participantCount;
    int started;
    time_t start_time;
} Room;

typedef struct {
    int sock;
    char username[64];
    int user_id;           // 🔧 Track user ID for question creator logging
    int loggedIn;
    char role[32];
    int current_question_id;  // 🔧 For PRACTICE mode: track current question
    char current_question_correct;  // 🔧 Correct answer for current question
} Client;

// Chỉ khai báo prototype, không viết hàm ở đây nữa
void writeLog(const char *event); 
int register_user_with_role(const char *username, const char *password, const char *role);
int validate_user(const char *username, const char *password, char *role_out);
void save_rooms();
void load_rooms();
void save_results();

Room rooms[MAX_ROOMS];
int roomCount = 0;
QItem practiceQuestions[MAX_Q];
int practiceQuestionCount = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void trim_newline(char *s) {
    char *p = strchr(s, '\n'); if (p) *p = 0;
    p = strchr(s, '\r'); if (p) *p = 0;
}

void send_msg(int sock, const char *msg) {
    char full[BUF_SIZE];
    snprintf(full, sizeof(full), "%s\n", msg);
    send(sock, full, strlen(full), 0);
}

Room* find_room(const char *name) {
    for (int i = 0; i < roomCount; i++)
        if (strcmp(rooms[i].name, name) == 0) return &rooms[i];
    return NULL;
}

Participant* find_participant(Room *r, const char *user) {
    for (int i = 0; i < r->participantCount; i++)
        if (strcmp(r->participants[i].username, user) == 0) return &r->participants[i];
    return NULL;
}

void save_rooms() {
    // Rooms are now persisted to database on creation/deletion
    // This function kept for compatibility but all real persistence is in database
    // The in-memory rooms[] array is used for active session management
}

void load_rooms() {
    // Load active rooms from database (rooms that haven't been deleted/finished)
    // Rooms are created/deleted via database calls, not file I/O
    // This maintains in-memory state for active exam sessions on server restart
    
    DBRoom db_rooms[MAX_ROOMS];
    int loaded_count = db_load_all_rooms(db_rooms, MAX_ROOMS);
    
    roomCount = 0;
    int total_participants = 0;
    
    for (int i = 0; i < loaded_count && roomCount < MAX_ROOMS; i++) {
        Room *r = &rooms[roomCount];
        r->db_id = db_rooms[i].id;
        strncpy(r->name, db_rooms[i].name, 63);
        r->name[63] = '\0';
        
        // Get owner username from database
        char owner_name[64] = "";
        if (db_get_username_by_id(db_rooms[i].owner_id, owner_name, 64)) {
            strcpy(r->owner, owner_name);
        } else {
            strcpy(r->owner, "unknown");
        }
        
        r->duration = db_rooms[i].duration_minutes;
        r->started = db_rooms[i].is_started;
        r->start_time = time(NULL);  // Use current time for resumed session
        r->participantCount = 0;
        
        // Load questions for this room from database
        DBQuestion db_questions[MAX_QUESTIONS_PER_ROOM];
        int num_questions = db_get_room_questions(db_rooms[i].id, db_questions, MAX_QUESTIONS_PER_ROOM);
        
        // Convert DBQuestion to QItem format
        r->numQuestions = 0;
        for (int q = 0; q < num_questions; q++) {
            r->questions[q].id = db_questions[q].id;
            strncpy(r->questions[q].text, db_questions[q].text, 255);
            r->questions[q].text[255] = '\0';
            strncpy(r->questions[q].A, db_questions[q].option_a, 127);
            r->questions[q].A[127] = '\0';
            strncpy(r->questions[q].B, db_questions[q].option_b, 127);
            r->questions[q].B[127] = '\0';
            strncpy(r->questions[q].C, db_questions[q].option_c, 127);
            r->questions[q].C[127] = '\0';
            strncpy(r->questions[q].D, db_questions[q].option_d, 127);
            r->questions[q].D[127] = '\0';
            r->questions[q].correct = db_questions[q].correct_option;
            strncpy(r->questions[q].topic, db_questions[q].topic, 63);
            r->questions[q].topic[63] = '\0';
            strncpy(r->questions[q].difficulty, db_questions[q].difficulty, 31);
            r->questions[q].difficulty[31] = '\0';
            r->numQuestions++;
        }
        
        if (r->numQuestions > 0) {
            // 🔧 NEW: Load participants + answers from database to restore test results
            DBParticipantInfo db_participants[MAX_PARTICIPANTS];
            int num_participants = db_load_room_participants(r->db_id, db_participants, MAX_PARTICIPANTS);
            
            // Populate in-memory Participant array
            for (int p = 0; p < num_participants; p++) {
                r->participants[p].db_id = db_participants[p].db_id;
                strncpy(r->participants[p].username, db_participants[p].username, 63);
                r->participants[p].username[63] = '\0';
                r->participants[p].score = db_participants[p].score;
                strncpy(r->participants[p].answers, db_participants[p].answers, MAX_QUESTIONS_PER_ROOM - 1);
                r->participants[p].answers[MAX_QUESTIONS_PER_ROOM - 1] = '\0';
                r->participants[p].history_count = 0;
                r->participants[p].submit_time = 0;
                r->participants[p].start_time = time(NULL);  // Reset timer
            }
            r->participantCount = num_participants;
            total_participants += num_participants;
            
            roomCount++;
            printf("✓ Loaded room: %s (ID: %d, Questions: %d, Participants: %d)\n", 
                   r->name, r->db_id, r->numQuestions, num_participants);
        }
    }
    
    printf("Loaded %d rooms with %d total participants from database on startup\n", roomCount, total_participants);
}

void save_results() {
    // Results are now persisted to database when submitted via SUBMIT command
    // This function kept for compatibility but all real persistence is in database
    // The in-memory participant[] arrays are used for active session management
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
                if (p->score == -1 && p->start_time > 0) {
                    double elapsed = difftime(now, p->start_time);
                    if (elapsed >= r->duration + 2) {
                        p->score = 0;
                        for (int q = 0; q < r->numQuestions; q++) {
                            if (p->answers[q] != '.' && toupper(p->answers[q]) == r->questions[q].correct)
                                p->score++;
                        }
                        p->submit_time = now;
                        printf("Auto-submitted for user %s in room %s\n", p->username, r->name);
                        
                        // Persist auto-submitted answers to database
                        for (int q = 0; q < r->numQuestions; q++) {
                            char selected = p->answers[q];
                            int is_correct = (selected != '.' && toupper(selected) == r->questions[q].correct) ? 1 : 0;
                            db_record_answer(p->db_id, r->questions[q].id, selected, is_correct);
                        }
                        db_add_result(p->db_id, r->db_id, p->score, r->numQuestions, p->score);
                        
                        char log_msg[256];
                        snprintf(log_msg, sizeof(log_msg), "User %s auto-submitted in room %s: %d/%d", 
                                p->username, r->name, p->score, r->numQuestions);
                        writeLog(log_msg);
                    }
                }
            }
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void* handle_client(void *arg) {
    Client *cli = (Client*)arg;
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
                send_msg(cli->sock, "FAIL Usage: REGISTER <username> <password> [role] [code]\n");
            } else {
                if (strcasecmp(role, "admin") != 0 && strcasecmp(role, "student") != 0) strcpy(role, "student");
                int authorized = 1;
                if (strcasecmp(role, "admin") == 0) {
                    if (strcmp(code, ADMIN_CODE) != 0) authorized = 0;
                }
                if (!authorized) {
                    send_msg(cli->sock, "FAIL Invalid Admin Secret Code!");
                    snprintf(log_msg, sizeof(log_msg), "Register failed for admin %s (Wrong Code)", user);
                    writeLog(log_msg);
                } else {
                    // 🔧 FIX: Use database directly instead of register_user_with_role
                    int user_id = db_add_user(user, pass, role);
                    if (user_id > 0) {
                        send_msg(cli->sock, "SUCCESS Registered. Please login.\n");
                        snprintf(log_msg, sizeof(log_msg), "User %s registered as %s in database", user, role);
                        writeLog(log_msg);
                    } else if (user_id == 0) {
                        send_msg(cli->sock, "FAIL User already exists\n");
                    } else {
                        send_msg(cli->sock, "FAIL Server error\n");
                    }
                }
            }
        }        
        else if (strcmp(cmd, "LOGIN") == 0) {
            char user[64], pass[64], role[32] = "student";
            sscanf(buffer, "LOGIN %63s %63s", user, pass);
            // 🔧 FIX: Use database functions directly
            int user_id = db_validate_user(user, pass);
            if (user_id > 0) {  // Only succeed if user_id is positive (valid user)
                db_get_user_role(user, role);
                strcpy(cli->username, user);
                strcpy(cli->role, role);
                cli->user_id = user_id;
                cli->loggedIn = 1;
                
                snprintf(log_msg, sizeof(log_msg), "User %s logged in as %s", user, role);
                writeLog(log_msg);
                
                char msg[128];
                snprintf(msg, sizeof(msg), "SUCCESS %s", role);
                send_msg(cli->sock, msg);
            } else {
                send_msg(cli->sock, "FAIL Invalid credentials");
                snprintf(log_msg, sizeof(log_msg), "Login failed for user %s", user);
                writeLog(log_msg);
            }
        }
        else if (!cli->loggedIn) {
            send_msg(cli->sock, "FAIL Please login first");
        }
        else if (strcmp(cmd, "CREATE") == 0 && strcmp(cli->role, "admin") == 0) {
            char name[64], topic_filter[512] = "", diff_filter[512] = "";
            int numQ, dur;
            
            // Parse command: CREATE name numQ dur [TOPICS topic1:count1 topic2:count2 ...] [DIFFICULTIES easy:count ...]
            // Example: CREATE test_1 10 100 TOPICS database:5 intro_to_ai:5 DIFFICULTIES easy:10
            int parsed = sscanf(buffer, "CREATE %63s %d %d", name, &numQ, &dur);
            
            if (parsed != 3) {
                send_msg(cli->sock, "FAIL Usage: CREATE <name> <numQ> <duration> [TOPICS ...] [DIFFICULTIES ...]");
            } else {
                // Parse TOPICS section
                char *topics_start = strstr(buffer, "TOPICS ");
                if (topics_start) {
                    topics_start += 7;  // Skip "TOPICS "
                    char *difficulties_start = strstr(topics_start, "DIFFICULTIES");
                    
                    if (difficulties_start) {
                        // Topics are between "TOPICS " and "DIFFICULTIES"
                        int topics_len = difficulties_start - topics_start;
                        strncpy(topic_filter, topics_start, topics_len);
                        topic_filter[topics_len] = '\0';
                        
                        // Trim trailing spaces
                        int i = strlen(topic_filter) - 1;
                        while (i >= 0 && isspace((unsigned char)topic_filter[i])) {
                            topic_filter[i] = '\0';
                            i--;
                        }
                    } else {
                        // Topics go to end of string
                        strcpy(topic_filter, topics_start);
                        
                        // Trim trailing spaces
                        int i = strlen(topic_filter) - 1;
                        while (i >= 0 && isspace((unsigned char)topic_filter[i])) {
                            topic_filter[i] = '\0';
                            i--;
                        }
                    }
                }
                
                // Parse DIFFICULTIES section
                char *difficulties_start = strstr(buffer, "DIFFICULTIES ");
                if (difficulties_start) {
                    difficulties_start += 13;  // Skip "DIFFICULTIES "
                    strcpy(diff_filter, difficulties_start);
                    
                    // Trim trailing spaces
                    int i = strlen(diff_filter) - 1;
                    while (i >= 0 && isspace((unsigned char)diff_filter[i])) {
                        diff_filter[i] = '\0';
                        i--;
                    }
                }
                
                // If topic_filter is "#", treat as empty
                if (strcmp(topic_filter, "#") == 0) {
                    strcpy(topic_filter, "");
                }
                
                // If diff_filter is "#", treat as empty
                if (strcmp(diff_filter, "#") == 0) {
                    strcpy(diff_filter, "");
                }
                
                // Validate inputs
                if (numQ < 1 || numQ > MAX_QUESTIONS_PER_ROOM) {
                    send_msg(cli->sock, "FAIL Number of questions must be 1-50");
                } else if (dur < 10 || dur > 86400) {
                    send_msg(cli->sock, "FAIL Duration must be 10-86400 seconds");
                } else if (find_room(name)) {
                    send_msg(cli->sock, "FAIL Room already exists");
                } else {
                    fprintf(stderr, "[CREATE_ROOM] DEBUG START\n");
                    fprintf(stderr, "[CREATE_ROOM] room_name='%s', numQ=%d, duration=%d\n", name, numQ, dur);
                    fprintf(stderr, "[CREATE_ROOM] topic_filter='%s' (len=%zu)\n", topic_filter, strlen(topic_filter));
                    fprintf(stderr, "[CREATE_ROOM] diff_filter='%s' (len=%zu)\n", diff_filter, strlen(diff_filter));
                    fflush(stderr);
                    
                    // Load questions from database using filtered queries
                    QItem temp_questions[MAX_QUESTIONS_PER_ROOM];
                    int loaded = 0;
                    
                    if (strlen(topic_filter) > 0 && strlen(diff_filter) > 0) {
                        fprintf(stderr, "[CREATE_ROOM] Path 1: Both topic and difficulty filters\n");
                        fflush(stderr);
                        // Use filtered selection: parse topics and difficulties
                        int topic_counts[32];
                        int difficulty_counts[3];
                        
                        char *topic_ids = db_parse_topic_filter(topic_filter, topic_counts, 32);
                        db_parse_difficulty_filter(diff_filter, difficulty_counts);
                        
                        if (topic_ids && (difficulty_counts[0] + difficulty_counts[1] + difficulty_counts[2]) == numQ) {
                            // Load questions for each difficulty level
                            DBQuestion db_temp_questions[MAX_QUESTIONS_PER_ROOM];
                            int q_idx = 0;
                            
                            // Load easy questions
                            if (difficulty_counts[0] > 0) {
                                int loaded_easy = db_get_random_filtered_questions(topic_ids, 1, difficulty_counts[0], db_temp_questions + q_idx);
                                fprintf(stderr, "[CREATE_ROOM] Loaded %d easy questions\n", loaded_easy);
                                q_idx += loaded_easy;
                            }
                            
                            // Load medium questions
                            if (difficulty_counts[1] > 0) {
                                int loaded_med = db_get_random_filtered_questions(topic_ids, 2, difficulty_counts[1], db_temp_questions + q_idx);
                                fprintf(stderr, "[CREATE_ROOM] Loaded %d medium questions\n", loaded_med);
                                q_idx += loaded_med;
                            }
                            
                            // Load hard questions
                            if (difficulty_counts[2] > 0) {
                                int loaded_hard = db_get_random_filtered_questions(topic_ids, 3, difficulty_counts[2], db_temp_questions + q_idx);
                                fprintf(stderr, "[CREATE_ROOM] Loaded %d hard questions\n", loaded_hard);
                                q_idx += loaded_hard;
                            }
                            
                            loaded = q_idx;
                            
                            // Convert DBQuestion to QItem
                            for (int i = 0; i < loaded; i++) {
                                temp_questions[i].id = db_temp_questions[i].id;
                                strcpy(temp_questions[i].text, db_temp_questions[i].text);
                                strcpy(temp_questions[i].A, db_temp_questions[i].option_a);
                                strcpy(temp_questions[i].B, db_temp_questions[i].option_b);
                                strcpy(temp_questions[i].C, db_temp_questions[i].option_c);
                                strcpy(temp_questions[i].D, db_temp_questions[i].option_d);
                                temp_questions[i].correct = db_temp_questions[i].correct_option;
                                strcpy(temp_questions[i].topic, db_temp_questions[i].topic);
                                strcpy(temp_questions[i].difficulty, db_temp_questions[i].difficulty);
                            }
                        } else {
                            send_msg(cli->sock, "FAIL Invalid topic or difficulty filter, or total count does not match numQ");
                            loaded = 0;
                        }
                    } else if (strlen(topic_filter) > 0 && strlen(diff_filter) == 0) {
                        fprintf(stderr, "[CREATE_ROOM] Path 2: Topic filter only (skip difficulty)\n");
                        fprintf(stderr, "[CREATE_ROOM] Calling db_get_questions_with_distribution('%s', '#', numQ=%d)\n", topic_filter, numQ);
                        fflush(stderr);
                        // Topic filter only - no difficulty filter
                        // Call db_get_questions_with_distribution with diff_filter as "#"
                        DBQuestion db_temp_questions[MAX_QUESTIONS_PER_ROOM];
                        loaded = db_get_questions_with_distribution(topic_filter, "#", db_temp_questions, numQ);
                        
                        fprintf(stderr, "[CREATE_ROOM] db_get_questions_with_distribution returned %d questions\n", loaded);
                        fflush(stderr);
                        
                        // Convert DBQuestion to QItem
                        for (int i = 0; i < loaded; i++) {
                            fprintf(stderr, "[CREATE_ROOM] Question %d: id=%d, topic=%s, difficulty=%s\n", 
                                    i, db_temp_questions[i].id, db_temp_questions[i].topic, db_temp_questions[i].difficulty);
                            temp_questions[i].id = db_temp_questions[i].id;
                            strcpy(temp_questions[i].text, db_temp_questions[i].text);
                            strcpy(temp_questions[i].A, db_temp_questions[i].option_a);
                            strcpy(temp_questions[i].B, db_temp_questions[i].option_b);
                            strcpy(temp_questions[i].C, db_temp_questions[i].option_c);
                            strcpy(temp_questions[i].D, db_temp_questions[i].option_d);
                            temp_questions[i].correct = db_temp_questions[i].correct_option;
                            strcpy(temp_questions[i].topic, db_temp_questions[i].topic);
                            strcpy(temp_questions[i].difficulty, db_temp_questions[i].difficulty);
                        }
                    } else {
                        fprintf(stderr, "[CREATE_ROOM] Path 3: No filters - load all questions\n");
                        fflush(stderr);
                        // No filters: load random questions
                        DBQuestion db_temp_questions[MAX_QUESTIONS_PER_ROOM];
                        loaded = db_get_all_questions(db_temp_questions, numQ);
                        
                        // Convert to QItem
                        for (int i = 0; i < loaded; i++) {
                            temp_questions[i].id = db_temp_questions[i].id;
                            strcpy(temp_questions[i].text, db_temp_questions[i].text);
                            strcpy(temp_questions[i].A, db_temp_questions[i].option_a);
                            strcpy(temp_questions[i].B, db_temp_questions[i].option_b);
                            strcpy(temp_questions[i].C, db_temp_questions[i].option_c);
                            strcpy(temp_questions[i].D, db_temp_questions[i].option_d);
                            temp_questions[i].correct = db_temp_questions[i].correct_option;
                            strcpy(temp_questions[i].topic, db_temp_questions[i].topic);
                            strcpy(temp_questions[i].difficulty, db_temp_questions[i].difficulty);
                        }
                    }
                    
                    if (loaded == 0) {
                        send_msg(cli->sock, "FAIL No questions match your criteria");
                    } else {
                        // Create room in database
                        int room_id = db_create_room(name, cli->user_id, dur);
                        if (room_id <= 0) {
                            send_msg(cli->sock, "FAIL Could not create room in database");
                        } else {
                            // Add questions to room in database
                            for (int q_idx = 0; q_idx < loaded; q_idx++) {
                                db_add_question_to_room(room_id, temp_questions[q_idx].id, q_idx);
                            }
                            
                            // Add to in-memory array for active session management
                            Room *r = &rooms[roomCount++];
                            r->db_id = room_id;                  // Store database room ID
                            strcpy(r->name, name);
                            strcpy(r->owner, cli->username);
                            r->duration = dur;
                            r->started = 1;
                            r->start_time = time(NULL);
                            r->participantCount = 0;
                            r->numQuestions = loaded;
                            memcpy(r->questions, temp_questions, loaded * sizeof(QItem));
                            
                            char log_msg[256];
                            snprintf(log_msg, sizeof(log_msg), "Admin %s created room %s with %d questions", cli->username, name, loaded);
                            writeLog(log_msg);
                            db_add_log(cli->user_id, "CREATE_ROOM", log_msg);
                            
                            send_msg(cli->sock, "SUCCESS Room created");
                        }
                    }
                }
            }
        }
        else if (strcmp(cmd, "LIST") == 0) {
            char msg[4096] = "SUCCESS Rooms:\n";
            if (roomCount == 0) strcat(msg, "No rooms.\n");
            for (int i = 0; i < roomCount; i++) {
                char line[256];
                snprintf(line, sizeof(line), "- %s (Owner: %s, Q: %d, Time: %ds)\n",
                        rooms[i].name, rooms[i].owner, rooms[i].numQuestions, rooms[i].duration);
                strcat(msg, line);
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
                Participant *p = find_participant(r, cli->username);
                if (!p) {
                    p = &r->participants[r->participantCount++];
                    strcpy(p->username, cli->username);
                    p->db_id = db_add_participant(r->db_id, cli->user_id);  // Add to database and store ID
                    p->score = -1;
                    p->history_count = 0;
                    memset(p->answers, '.', MAX_QUESTIONS_PER_ROOM);
                    p->submit_time = 0;
                    p->start_time = time(NULL);
                    db_add_log(cli->user_id, "JOIN_ROOM", name);
                } else {
                    if (p->score != -1) { 
                        if (p->history_count < MAX_ATTEMPTS) {
                            p->score_history[p->history_count++] = p->score;
                        }
                        p->score = -1;
                        memset(p->answers, '.', MAX_QUESTIONS_PER_ROOM);
                        p->submit_time = 0;
                        p->start_time = time(NULL);
                    }
                }
                
                int elapsed = (int)(time(NULL) - p->start_time);
                int remaining = r->duration - elapsed;
                if (remaining < 0) remaining = 0;

                char msg[128];
                snprintf(msg, sizeof(msg), "SUCCESS Joined %d %d", r->numQuestions, remaining);
                send_msg(cli->sock, msg);
            }
        }
        else if (strcmp(cmd, "GET_QUESTION") == 0) {
            char name[64]; int idx;
            sscanf(buffer, "GET_QUESTION %63s %d", name, &idx);
            Room *r = find_room(name);
            if (!r || idx >= r->numQuestions) {
                send_msg(cli->sock, "FAIL Invalid");
            } else {
                Participant *p = find_participant(r, cli->username);
                QItem *q = &r->questions[idx];
                
                // --- LẤY ĐÁP ÁN HIỆN TẠI TỪ SERVER ---
                char currentAns = ' ';
                if (p && p->score == -1) { // Nếu đang làm bài
                    currentAns = p->answers[idx];
                    if (currentAns == '.') currentAns = ' ';
                }
                // -------------------------------------

                char temp[BUF_SIZE];
                // Gửi kèm dòng [Your Selection: X] ở cuối
                snprintf(temp, sizeof(temp),
                         "[%d/%d] %s\nA) %s\nB) %s\nC) %s\nD) %s\n\n[Your Selection: %c]\n",
                         idx+1, r->numQuestions, q->text, q->A, q->B, q->C, q->D, 
                         currentAns);
                send_msg(cli->sock, temp);
            }
        }
        else if (strcmp(cmd, "ANSWER") == 0) {
            // Distinguish between: 
            // - Test room answer: "ANSWER roomname qIdx answer"
            // - Practice answer: "ANSWER A"
            
            // Count spaces to determine which type
            int space_count = 0;
            char *ptr = buffer;
            while (*ptr) {
                if (*ptr == ' ') space_count++;
                ptr++;
            }
            
            if (space_count == 1) {
                // Practice mode answer: "ANSWER A" (1 space)
                char *answer_str = strchr(buffer, ' ');
                if (answer_str == NULL || answer_str[1] == '\0') {
                    send_msg(cli->sock, "FAIL Invalid answer format");
                } else {
                    char answer_char = answer_str[1];
                    answer_char = toupper(answer_char);
                    
                    if (answer_char == cli->current_question_correct) {
                        send_msg(cli->sock, "CORRECT");
                    } else {
                        char response[256];
                        snprintf(response, sizeof(response), "WRONG|%c", cli->current_question_correct);
                        send_msg(cli->sock, response);
                    }
                }
            } else if (space_count >= 3) {
                // Test room mode answer: "ANSWER roomname qIdx answer"
                char roomName[64], ansChar;
                int qIdx;
                sscanf(buffer, "ANSWER %63s %d %c", roomName, &qIdx, &ansChar);
                
                Room *r = find_room(roomName);
                if (r) {
                    Participant *p = find_participant(r, cli->username);
                    if (p && p->score == -1) {
                        if (qIdx >= 0 && qIdx < r->numQuestions) {
                            p->answers[qIdx] = ansChar;
                        }
                    }
                }
            }
        }
        else if (strcmp(cmd, "SUBMIT") == 0) {
            char name[64], ans[256];
            sscanf(buffer, "SUBMIT %63s %255s", name, ans);
            Room *r = find_room(name);
            if (!r) send_msg(cli->sock, "FAIL Room not found");
            else {
                Participant *p = find_participant(r, cli->username);
                if (!p || p->score != -1) send_msg(cli->sock, "FAIL Not in room or submitted");
                else {
                    int score = 0;
                    for (int i = 0; i < r->numQuestions && i < (int)strlen(ans); i++) {
                        if (ans[i] != '.' && toupper(ans[i]) == r->questions[i].correct) score++;
                    }
                    p->score = score;
                    p->submit_time = time(NULL);
                    strcpy(p->answers, ans);
                    
                    // Persist results to database
                    // 1. Record each answer
                    for (int i = 0; i < r->numQuestions && i < (int)strlen(ans); i++) {
                        char selected = ans[i];
                        int is_correct = (selected != '.' && toupper(selected) == r->questions[i].correct) ? 1 : 0;
                        db_record_answer(p->db_id, r->questions[i].id, selected, is_correct);
                    }
                    
                    // 2. Save result summary
                    db_add_result(p->db_id, r->db_id, score, r->numQuestions, score);
                    
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "User %s submitted answers in room %s: %d/%d", 
                            cli->username, name, score, r->numQuestions);
                    writeLog(log_msg);
                    db_add_log(cli->user_id, "SUBMIT_ROOM", log_msg);
                    
                    char msg[128];
                    snprintf(msg, sizeof(msg), "SUCCESS Score: %d/%d", score, r->numQuestions);
                    send_msg(cli->sock, msg);
                }
            }
        }
        else if (strcmp(cmd, "RESULTS") == 0) {
            char name[64];
            sscanf(buffer, "RESULTS %63s", name);
            Room *r = find_room(name);
            if (!r) send_msg(cli->sock, "FAIL Not found");
            else {
                char msg[4096] = "SUCCESS Results:\n";
                for (int i = 0; i < r->participantCount; i++) {
                    Participant *p = &r->participants[i];
                    char line[512];
                    char historyStr[256] = "";
                    for(int k=0; k < p->history_count; k++) {
                        char tmp[32];
                        snprintf(tmp, sizeof(tmp), "Att%d:%d/%d ", k+1, p->score_history[k], r->numQuestions);
                        strcat(historyStr, tmp);
                    }
                    if (p->score != -1) {
                         char tmp[64];
                         snprintf(tmp, sizeof(tmp), "Latest:%d/%d", p->score, r->numQuestions);
                         strcat(historyStr, tmp);
                    } else {
                         strcat(historyStr, "Doing...");
                    }
                    snprintf(line, sizeof(line), "- %s | %s\n", p->username, historyStr);
                    strcat(msg, line);
                }
                send_msg(cli->sock, msg);
            }
        }
        else if (strcmp(cmd, "PREVIEW") == 0 && strcmp(cli->role, "admin") == 0) {
            char name[64]; sscanf(buffer, "PREVIEW %63s", name);
            Room *r = find_room(name);
            if (!r) send_msg(cli->sock, "FAIL Room not found");
            else if (strcmp(r->owner, cli->username) != 0) send_msg(cli->sock, "FAIL Not your room");
            else {
                char msg[BUF_SIZE] = "SUCCESS Preview:\n";
                for (int i = 0; i < r->numQuestions; i++) {
                    QItem *q = &r->questions[i];
                    char line[1024];
                    snprintf(line, sizeof(line),"[%d/%d] %s\nA) %s\nB) %s\nC) %s\nD) %s\nCorrect: %c\n\n",
                             i+1, r->numQuestions, q->text, q->A, q->B, q->C, q->D, q->correct);
                    strcat(msg, line);
                }
                send_msg(cli->sock, msg);
            }
        }
        else if (strcmp(cmd, "DELETE") == 0 && strcmp(cli->role, "admin") == 0) {
            char name[64]; sscanf(buffer, "DELETE %63s", name);
            Room *r = find_room(name);
            if (!r) send_msg(cli->sock, "FAIL Room not found");
            else if (strcmp(r->owner, cli->username) != 0) send_msg(cli->sock, "FAIL Not your room");
            else {
                // 🔧 FIX: Delete from database
                int room_id = db_get_room_id_by_name(name);
                if (room_id > 0) {
                    db_delete_room(room_id);
                    printf("[DEBUG] Room '%s' (id=%d) deleted from database\n", name, room_id);
                }
                
                // Remove from in-memory array
                for (int i = r - rooms; i < roomCount - 1; i++) rooms[i] = rooms[i + 1];
                roomCount--;
                
                char log_msg[256];
                snprintf(log_msg, sizeof(log_msg), "Admin %s deleted room %s", cli->username, name);
                writeLog(log_msg);
                
                send_msg(cli->sock, "SUCCESS Room deleted");
            }
        }
        else if (strcmp(cmd, "LEADERBOARD") == 0) {
            // 🔧 FIX: Parse room name from command
            char room_name[64] = "";
            sscanf(buffer, "LEADERBOARD %63s", room_name);
            
            if (strlen(room_name) == 0) {
                send_msg(cli->sock, "FAIL Please specify a room name: LEADERBOARD <room_name>");
            } else {
                // Find room by name
                Room *r = find_room(room_name);
                if (r == NULL) {
                    char error[256];
                    snprintf(error, sizeof(error), "FAIL Room '%s' not found", room_name);
                    send_msg(cli->sock, error);
                } else {
                    // Get leaderboard for this room (no SUCCESS prefix)
                    char output[4096];
                    output[0] = '\0';
                    int count = db_get_leaderboard(r->db_id, output, sizeof(output));
                    
                    if (count == 0) {
                        snprintf(output, sizeof(output), "No results yet for room '%s'\n", room_name);
                    }
                    
                    send_msg(cli->sock, output);
                    
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "User %s viewed leaderboard for room %s", cli->username, room_name);
                    writeLog(log_msg);
                }
            }
        }
        else if (strcmp(cmd, "PRACTICE") == 0) {
            // Parse command: PRACTICE or PRACTICE <topic_name>
            char topic_name[64] = "";
            char *ptr = strchr(buffer, ' ');
            if (ptr != NULL) {
                sscanf(ptr + 1, "%63s", topic_name);
            }
            
            if (strlen(topic_name) == 0) {
                // First call: send list of topics
                char topics_output[2048];
                int topic_count = db_get_all_topics(topics_output);
                
                if (topic_count == 0) {
                    send_msg(cli->sock, "FAIL No topics available");
                } else {
                    char response[2048];
                    snprintf(response, sizeof(response), "TOPICS %s", topics_output);
                    send_msg(cli->sock, response);
                }
            } else {
                // Second call: PRACTICE <topic_name> - return random question
                DBQuestion question;
                if (!db_get_random_question_by_topic(topic_name, &question)) {
                    char error[256];
                    snprintf(error, sizeof(error), "FAIL No questions found for topic '%s'", topic_name);
                    send_msg(cli->sock, error);
                } else {
                    // Format question response
                    char response[BUF_SIZE];
                    snprintf(response, sizeof(response),
                            "PRACTICE_Q %d|%s|%s|%s|%s|%s|%c|%s",
                            question.id, question.text,
                            question.option_a, question.option_b, 
                            question.option_c, question.option_d,
                            question.correct_option, question.topic);
                    send_msg(cli->sock, response);
                    
                    // Store current question for answer checking
                    cli->current_question_id = question.id;
                    cli->current_question_correct = question.correct_option;
                }
            }
        }
        else if (strcmp(cmd, "GET_TOPICS") == 0) {
            char topics_output[2048] = "SUCCESS ";
            char topics_data[1024] = "";
            
            // Use database function instead of file I/O
            if (get_all_topics_with_counts(topics_data) > 0 && strlen(topics_data) > 0) {
                // Format: topic1:count1|topic2:count2|... (keep as is)
                strcat(topics_output, topics_data);
            }
            send_msg(cli->sock, topics_output);
        }
        else if (strcmp(cmd, "GET_DIFFICULTIES") == 0) {
            char diff_output[1024] = "SUCCESS ";
            char diff_data[256] = "";
            
            // Use database function instead of file I/O
            if (get_all_difficulties_with_counts(diff_data) > 0 && strlen(diff_data) > 0) {
                // Format: easy:count|medium:count|hard:count (keep as is)
                strcat(diff_output, diff_data);
            }
            send_msg(cli->sock, diff_output);
        }
        else if (strcmp(cmd, "GET_DIFFICULTIES_FOR_TOPICS") == 0) {
            // Format: GET_DIFFICULTIES_FOR_TOPICS topic1:count1 topic2:count2 ...
            char topic_filter[512] = "";
            // Extract everything after "GET_DIFFICULTIES_FOR_TOPICS "
            const char *prefix = "GET_DIFFICULTIES_FOR_TOPICS ";
            if (strncmp(buffer, prefix, strlen(prefix)) == 0) {
                strcpy(topic_filter, buffer + strlen(prefix));
            }
            
            printf("[DEBUG] GET_DIFFICULTIES_FOR_TOPICS received: '%s'\n", topic_filter);
            fflush(stdout);
            
            if (strlen(topic_filter) == 0) {
                send_msg(cli->sock, "FAIL No topics provided");
            } else {
                // Parse topics and get topic_ids string
                int topic_counts[32];
                char *topic_ids = db_parse_topic_filter(topic_filter, topic_counts, 32);
                printf("[DEBUG] Parsed topic_ids: '%s'\n", topic_ids ? topic_ids : "NULL");
                fflush(stdout);
                
                if (!topic_ids) {
                    send_msg(cli->sock, "FAIL Invalid topic names");
                } else {
                    // Count difficulties for these topics
                    int difficulty_counts[3];
                    db_count_difficulties_for_topics(topic_ids, difficulty_counts);
                    printf("[DEBUG] Difficulty counts: easy=%d, medium=%d, hard=%d\n", 
                           difficulty_counts[0], difficulty_counts[1], difficulty_counts[2]);
                    fflush(stdout);
                    
                    // Format: easy:count|medium:count|hard:count (match database format)
                    char response[256];
                    snprintf(response, sizeof(response), 
                             "SUCCESS easy:%d|medium:%d|hard:%d|",
                             difficulty_counts[0], difficulty_counts[1], difficulty_counts[2]);
                    send_msg(cli->sock, response);
                }
            }

        }
        else if (strcmp(cmd, "ADD_QUESTION") == 0 && strcmp(cli->role, "admin") == 0) {
            // Format: ADD_QUESTION text|A|B|C|D|correct|topic|difficulty
            char text[256], A[128], B[128], C[128], D[128];
            char correct_str[2], topic[64], difficulty[32];
            
            int parsed = sscanf(buffer, 
                "ADD_QUESTION %255[^|]|%127[^|]|%127[^|]|%127[^|]|%127[^|]|%1c|%63[^|]|%31s",
                text, A, B, C, D, correct_str, topic, difficulty);
            
            if (parsed != 8) {
                send_msg(cli->sock, "FAIL Invalid format: ADD_QUESTION text|A|B|C|D|correct|topic|difficulty");
            } else {
                // Create QItem for validation
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
                
                // Validate question
                char error_msg[256];
                if (!validate_question_input(&new_q, error_msg)) {
                    send_msg(cli->sock, error_msg);
                } else {
                    // 🔧 FIX: Use database directly (don't call add_question_to_file to avoid duplicates)
                    int new_id = db_add_question(text, A, B, C, D, correct_str[0], 
                                                 topic, difficulty, cli->user_id);
                    if (new_id > 0) {
                        // Success - question added to database
                        char log_msg[512];
                        snprintf(log_msg, sizeof(log_msg), "Admin %s added question ID %d to database: %s/%s", 
                                cli->username, new_id, topic, difficulty);
                        writeLog(log_msg);
                        
                        char msg[256];
                        snprintf(msg, sizeof(msg), "SUCCESS Question added with ID %d", new_id);
                        send_msg(cli->sock, msg);
                    } else {
                        char msg[256];
                        snprintf(msg, sizeof(msg), "FAIL Could not add question to database");
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
                    snprintf(result + strlen(result), sizeof(result) - strlen(result), "%d|%s|%s|%s|%s|%s|%c|%s|%s",
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
                    strcpy(result, "FAIL No questions found with that topic");
                }
            }
            else if (strcmp(filter_type, "difficulty") == 0) {
                char output[8192];
                count = search_questions_by_difficulty(search_value, output);
                if (count > 0) {
                    strcat(result, output);
                } else {
                    strcpy(result, "FAIL No questions found with that difficulty");
                }
            }
            else {
                strcpy(result, "FAIL Invalid filter type: use id, topic, or difficulty");
            }
            
            send_msg(cli->sock, result);
        }
        else if (strcmp(cmd, "DELETE_QUESTION") == 0 && strcmp(cli->role, "admin") == 0) {
            int question_id;
            sscanf(buffer, "DELETE_QUESTION %d", &question_id);
            
            // First verify the question exists
            QItem q;
            if (!search_questions_by_id(question_id, &q)) {
                send_msg(cli->sock, "FAIL Question not found");
            } else {
                // Delete the question
                if (delete_question_by_id(question_id)) {
                    // Note: No need to renumber questions anymore since we use auto-increment IDs
                    // Gaps in IDs are normal and don't cause issues
                    
                    char msg[256];
                    snprintf(msg, sizeof(msg), "SUCCESS Question ID %d deleted", question_id);
                    send_msg(cli->sock, msg);
                    
                    // Log
                    char log_msg[512];
                    snprintf(log_msg, sizeof(log_msg), "Admin %s deleted question ID %d (%s)", cli->username, question_id, q.text);
                    writeLog(log_msg);
                } else {
                    send_msg(cli->sock, "FAIL Could not delete question");
                }
            }
        }
        else if (strcmp(cmd, "EXIT") == 0) {
            send_msg(cli->sock, "SUCCESS Goodbye");
            pthread_mutex_unlock(&lock); break;
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
    mkdir("data", 0755);
    pthread_mutex_init(&lock, NULL);
    srand(time(NULL));
    
    // ===== PHASE 1-2: Initialize Database =====
    printf("Initializing SQLite database...\n");
    if (!db_init(DB_PATH)) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    if (!db_create_tables()) {
        fprintf(stderr, "Failed to create database tables\n");
        db_close();
        return 1;
    }
    
    if (!db_init_default_difficulties()) {
        fprintf(stderr, "Failed to initialize difficulties\n");
        db_close();
        return 1;
    }
    
    printf("Database initialized successfully\n");
    
    // Load test data from SQL file if database is empty
    sqlite3_stmt *check_stmt;
    int topics_count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM topics", -1, &check_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            topics_count = sqlite3_column_int(check_stmt, 0);
        }
        sqlite3_finalize(check_stmt);
    }
    
    if (topics_count == 0) {
        printf("Database is empty. Loading test data from SQL file...\n");
        if (!load_sql_file("insert_questions.sql")) {
            fprintf(stderr, "Warning: Failed to load SQL file, continuing with empty database\n");
        }
    }
    
    printf("Database ready for use\n");
    
    // Load practice questions from database
    // Note: practiceQuestionCount is now managed by database queries
    // No longer loading from text files - using database directly
    
    writeLog("SERVER_STARTED");
    
    // Load rooms from database
    load_rooms();

    pthread_t mon_tid;
    pthread_create(&mon_tid, NULL, monitor_exam_thread, NULL);
    pthread_detach(mon_tid); 

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = INADDR_ANY };
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 10);
    printf("Server running on port %d\n", PORT);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        int cli_sock = accept(server_sock, (struct sockaddr*)&cli_addr, &len);
        if (cli_sock >= 0) {
            Client *cli = malloc(sizeof(Client));
            cli->sock = cli_sock; cli->loggedIn = 0;
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, cli);
            pthread_detach(tid);
        }
    }
    
    db_close();
    return 0;
}