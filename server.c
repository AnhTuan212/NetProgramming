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

#define ADMIN_CODE "network_programming"
#define PORT 9000
#define BUF_SIZE 8192
#define MAX_ROOMS 100
#define MAX_PARTICIPANTS 50
#define MAX_Q 200
#define MAX_ATTEMPTS 10

#define ROOMS_FILE "data/rooms.txt"
#define RESULTS_FILE "data/results.txt"
// LOG_FILE được định nghĩa trong logger.c nên không cần define ở đây

typedef struct {
    char username[64];
    int score; 
    char answers[MAX_QUESTIONS_PER_ROOM];
    int score_history[MAX_ATTEMPTS]; 
    int history_count; 
    time_t submit_time;
    time_t start_time;
} Participant;

typedef struct {
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
    FILE *fp = fopen(ROOMS_FILE, "w");
    if (!fp) return;
    for (int i = 0; i < roomCount; i++) {
        Room *r = &rooms[i];
        fprintf(fp, "%s|%s|%d|%d|%d\n", r->name, r->owner, r->numQuestions, r->duration, r->started);
        for (int j = 0; j < r->numQuestions; j++) {
            QItem *q = &r->questions[j];
            fprintf(fp, "Q|%s|%s|%s|%s|%s|%c|%s|%s\n",
                    q->text, q->A, q->B, q->C, q->D, q->correct, q->topic, q->difficulty);
        }
    }
    fclose(fp);
}

void load_rooms() {
    FILE *fp = fopen(ROOMS_FILE, "r");
    if (!fp) return;
    char line[1024];
    Room *cur = NULL;
    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);
        if (line[0] == '\0') continue;
        if (strncmp(line, "Q|", 2) == 0 && cur) {
            QItem q;
            sscanf(line, "Q|%255[^|]|%127[^|]|%127[^|]|%127[^|]|%127[^|]|%c|%63[^|]|%31s",
                   q.text, q.A, q.B, q.C, q.D, &q.correct, q.topic, q.difficulty);
            if (cur->numQuestions < MAX_QUESTIONS_PER_ROOM)
                cur->questions[cur->numQuestions++] = q;
        } else {
            cur = &rooms[roomCount++];
            sscanf(line, "%63[^|]|%63[^|]|%d|%d|%d", cur->name, cur->owner, &cur->numQuestions, &cur->duration, &cur->started);
            cur->participantCount = 0;
            cur->numQuestions = 0;
            cur->started = 1;
        }
    }
    fclose(fp);
}

void save_results() {
    FILE *fp = fopen(RESULTS_FILE, "w");
    if (!fp) return;
    for (int i = 0; i < roomCount; i++) {
        Room *r = &rooms[i];
        for (int j = 0; j < r->participantCount; j++) {
            Participant *p = &r->participants[j];
            fprintf(fp, "%s,%s,", r->name, p->username);
            for(int k=0; k < p->history_count; k++) {
                fprintf(fp, "%d;", p->score_history[k]);
            }
            if (p->score != -1) {
                fprintf(fp, "%d", p->score);
            }
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
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
                        save_results();
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
                    sprintf(log_msg, "Register failed for admin %s (Wrong Code)", user);
                    writeLog(log_msg);
                } else {
                    int result = register_user_with_role(user, pass, role);
                    if (result == 1) {
                        send_msg(cli->sock, "SUCCESS Registered. Please login.\n");
                        sprintf(log_msg, "User %s registered as %s.", user, role);
                        writeLog(log_msg);
                    } else if (result == 0) {
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
            if (validate_user(user, pass, role)) {
                strcpy(cli->username, user);
                strcpy(cli->role, role);
                cli->user_id = db_get_user_id(user);  // 🔧 Get user ID from database
                cli->loggedIn = 1;
                char msg[128];
                sprintf(msg, "SUCCESS %s", role);
                send_msg(cli->sock, msg);
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
            
            // Parse command: CREATE name numQ dur [TOPICS topic:count ...] [DIFFICULTIES diff:count ...]
            sscanf(buffer, "CREATE %63s %d %d %511s", name, &numQ, &dur, rest);
            
            // Parse filters from rest
            if (strlen(rest) > 0) {
                char rest_copy[512];
                strcpy(rest_copy, rest);
                
                // Look for TOPICS and DIFFICULTIES keywords
                char *topics_start = strstr(rest_copy, "TOPICS");
                char *diffs_start = strstr(rest_copy, "DIFFICULTIES");
                
                if (topics_start) {
                    topics_start += 6; // Skip "TOPICS"
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
                    diffs_start += 12; // Skip "DIFFICULTIES"
                    while (*diffs_start == ' ') diffs_start++;
                    strcpy(diff_filter, diffs_start);
                }
            }
            
            // Validate inputs
            if (numQ < 1 || numQ > MAX_QUESTIONS_PER_ROOM) {
                send_msg(cli->sock, "FAIL Number of questions must be 1-50");
            } else if (dur < 10 || dur > 86400) {
                send_msg(cli->sock, "FAIL Duration must be 10-86400 seconds");
            } else if (find_room(name)) {
                send_msg(cli->sock, "FAIL Room already exists");
            } else {
                // Load questions with combined filters
                QItem temp_questions[MAX_QUESTIONS_PER_ROOM];
                int loaded = loadQuestionsWithFilters("data/questions.txt", temp_questions, numQ,
                                                      strlen(topic_filter) > 0 ? topic_filter : NULL,
                                                      strlen(diff_filter) > 0 ? diff_filter : NULL);
                
                if (loaded == 0) {
                    send_msg(cli->sock, "FAIL No questions match your criteria");
                } else {
                    Room *r = &rooms[roomCount++];
                    strcpy(r->name, name);
                    strcpy(r->owner, cli->username);
                    r->duration = dur;
                    r->started = 1;
                    r->start_time = time(NULL);
                    r->participantCount = 0;
                    r->numQuestions = loaded;
                    memcpy(r->questions, temp_questions, loaded * sizeof(QItem));
                    
                    char log_msg[256];
                    sprintf(log_msg, "Admin %s created room %s with %d questions", cli->username, name, loaded);
                    writeLog(log_msg);
                    
                    send_msg(cli->sock, "SUCCESS Room created");
                    save_rooms();
                }
            }
        }
        else if (strcmp(cmd, "LIST") == 0) {
            char msg[4096] = "SUCCESS Rooms:\n";
            if (roomCount == 0) strcat(msg, "No rooms.\n");
            for (int i = 0; i < roomCount; i++) {
                char line[256];
                sprintf(line, "- %s (Owner: %s, Q: %d, Time: %ds)\n",
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
                    p->score = -1;
                    p->history_count = 0;
                    memset(p->answers, '.', MAX_QUESTIONS_PER_ROOM);
                    p->submit_time = 0;
                    p->start_time = time(NULL);
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
                sprintf(msg, "SUCCESS Joined %d %d", r->numQuestions, remaining);
                send_msg(cli->sock, msg);
                save_rooms();
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
                    save_results();
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
            if (!r) send_msg(cli->sock, "FAIL Not found");
            else {
                char msg[4096] = "SUCCESS Results:\n";
                for (int i = 0; i < r->participantCount; i++) {
                    Participant *p = &r->participants[i];
                    char line[512];
                    char historyStr[256] = "";
                    for(int k=0; k < p->history_count; k++) {
                        char tmp[32];
                        sprintf(tmp, "Att%d:%d/%d ", k+1, p->score_history[k], r->numQuestions);
                        strcat(historyStr, tmp);
                    }
                    if (p->score != -1) {
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
                for (int i = r - rooms; i < roomCount - 1; i++) rooms[i] = rooms[i + 1];
                roomCount--;
                send_msg(cli->sock, "SUCCESS Room deleted");
                save_rooms(); save_results();
            }
        }
        else if (strcmp(cmd, "LEADERBOARD") == 0) {
            show_leaderboard("leaderboard_output.txt");
            FILE *fp = fopen("leaderboard_output.txt", "r");
            if (fp) {
                char line[256]; char output[2048] = "SUCCESS Leaderboard (Avg Score):\n";
                while (fgets(line, sizeof(line), fp)) strcat(output, line);
                send_msg(cli->sock, output); fclose(fp);
            } else send_msg(cli->sock, "FAIL No data");
        }
        else if (strcmp(cmd, "PRACTICE") == 0) {
            if (practiceQuestionCount == 0) send_msg(cli->sock, "FAIL No practice questions");
            else {
                int idx = rand() % practiceQuestionCount;
                QItem *q = &practiceQuestions[idx];
                char temp[BUF_SIZE];
                snprintf(temp, sizeof(temp),"PRACTICE_Q [%d/%d] %s\nA) %s\nB) %s\nC) %s\nD) %s\nANSWER %c\n",
                         idx+1, practiceQuestionCount, q->text, q->A, q->B, q->C, q->D, q->correct);
                send_msg(cli->sock, temp);
            }
        }
        else if (strcmp(cmd, "GET_TOPICS") == 0) {
            char topics_output[2048] = "SUCCESS ";
            FILE *fp = fopen("data/questions.txt", "r");
            if (fp) {
                char line[BUF_SIZE];
                char topics[512] = "";
                char topic_counts[512] = "";
                int topic_count[20] = {0};
                char topic_names[20][64];
                int num_topics = 0;
                
                while (fgets(line, sizeof(line), fp)) {
                    char topic[64] = "";
                    sscanf(line, "%*[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%63[^|]", topic);
                    if (strlen(topic) > 0) {
                        to_lowercase(topic);
                        // Find or add topic
                        int found = -1;
                        for (int i = 0; i < num_topics; i++) {
                            if (strcmp(topic_names[i], topic) == 0) {
                                found = i;
                                break;
                            }
                        }
                        if (found == -1) {
                            strcpy(topic_names[num_topics], topic);
                            topic_count[num_topics] = 1;
                            num_topics++;
                        } else {
                            topic_count[found]++;
                        }
                    }
                }
                fclose(fp);
                
                // Format output: Topic(count)|Topic(count)|...
                for (int i = 0; i < num_topics; i++) {
                    char first_letter = topic_names[i][0];
                    first_letter = toupper(first_letter);
                    char rest[64];
                    strcpy(rest, topic_names[i] + 1);
                    char temp[128];
                    snprintf(temp, sizeof(temp), "%c%s(%d)|", first_letter, rest, topic_count[i]);
                    strcat(topics_output, temp);
                }
            }
            send_msg(cli->sock, topics_output);
        }
        else if (strcmp(cmd, "GET_DIFFICULTIES") == 0) {
            char diff_output[1024] = "SUCCESS ";
            FILE *fp = fopen("data/questions.txt", "r");
            if (fp) {
                char line[BUF_SIZE];
                int easy_count = 0, medium_count = 0, hard_count = 0;
                while (fgets(line, sizeof(line), fp)) {
                    char difficulty[32] = "";
                    sscanf(line, "%*[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%*[^|]|%31s", difficulty);
                    if (strlen(difficulty) > 0) {
                        to_lowercase(difficulty);
                        if (strcmp(difficulty, "easy") == 0) easy_count++;
                        else if (strcmp(difficulty, "medium") == 0) medium_count++;
                        else if (strcmp(difficulty, "hard") == 0) hard_count++;
                    }
                }
                fclose(fp);
                char count_str[256];
                snprintf(count_str, sizeof(count_str), "Easy(%d)|Medium(%d)|Hard(%d)|", easy_count, medium_count, hard_count);
                strcat(diff_output, count_str);
            }
            send_msg(cli->sock, diff_output);
        }
        else if (strcmp(cmd, "ADD_QUESTION") == 0 && strcmp(cli->role, "admin") == 0) {
            // Format: ADD_QUESTION text|A|B|C|D|correct|topic|difficulty
            char text[256], A[128], B[128], C[128], D[128];
            char correct_str[2], topic[64], difficulty[32];
            
            int parsed = sscanf(buffer, 
                "ADD_QUESTION %255[^|]|%127[^|]|%127[^|]|%127[^|]|%127[^|]|%1[^|]|%63[^|]|%31s",
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
                    // Add to file (under mutex)
                    int new_id = add_question_to_file(&new_q);
                    if (new_id < 0) {
                        send_msg(cli->sock, "FAIL Could not add question to file");
                    } else {
                        // 🔧 Log 1: File write success
                        char log_msg[512];
                        sprintf(log_msg, "Admin %s added question ID %d to file: %s/%s", 
                                cli->username, new_id, new_q.topic, new_q.difficulty);
                        writeLog(log_msg);
                        
                        // Reload practice questions
                        practiceQuestionCount = loadQuestionsTxt("data/questions.txt", practiceQuestions, MAX_Q, NULL, NULL);
                        
                        // 🔧 Add to database with case normalization
                        int db_result = db_add_question(text, A, B, C, D, correct_str[0], 
                                                       topic, difficulty, cli->user_id);
                        
                        if (db_result > 0) {
                            // 🔧 Log 2: Database insert success
                            sprintf(log_msg, "Database: Question %d synced to database by admin %s", 
                                   new_id, cli->username);
                            writeLog(log_msg);
                            
                            char msg[256];
                            sprintf(msg, "SUCCESS Question added with ID %d", new_id);
                            send_msg(cli->sock, msg);
                        } else {
                            // 🔧 Log 2: Database insert failure
                            if (db_result == -2) {
                                sprintf(log_msg, "Database error: Question %d file-only, invalid difficulty '%s' by admin %s", 
                                       new_id, difficulty, cli->username);
                            } else {
                                sprintf(log_msg, "Database error: Question %d file-only, insert failed by admin %s", 
                                       new_id, cli->username);
                            }
                            writeLog(log_msg);
                            
                            char msg[256];
                            sprintf(msg, "PARTIAL Question %d saved to file but database sync failed (see logs)", new_id);
                            send_msg(cli->sock, msg);
                        }
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
                    // Reload practice questions
                    practiceQuestionCount = loadQuestionsTxt("data/questions.txt", practiceQuestions, MAX_Q, NULL, NULL);
                    
                    char msg[256];
                    sprintf(msg, "SUCCESS Question ID %d deleted", question_id);
                    send_msg(cli->sock, msg);
                    
                    // Log
                    char log_msg[512];
                    sprintf(log_msg, "Admin %s deleted question ID %d (%s)", cli->username, question_id, q.text);
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
    
    // ===== PHASE 3: Migrate data from text files =====
    // Check if migration is needed (first run)
    sqlite3_stmt *stmt;
    int needs_migration = 0;
    
    const char *check_users = "SELECT COUNT(*) FROM users";
    if (sqlite3_prepare_v2(db, check_users, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) == 0) {
            needs_migration = 1;
        }
        sqlite3_finalize(stmt);
    }
    
    if (needs_migration) {
        printf("Performing initial data migration from text files...\n");
        migrate_from_text_files(DATA_DIR);
        verify_migration();
    }
    
    // 🔧 Sync any additional questions from file to database (for consistency)
    int synced = db_sync_questions_from_file("data/questions.txt");
    if (synced > 0) {
        printf("Synced %d questions from file to database\n", synced);
        char log_msg[256];
        sprintf(log_msg, "Server startup: Synced %d questions from file to database", synced);
        writeLog(log_msg);
    }
    
    // Gọi hàm writeLog từ logger.c
    writeLog("SERVER_STARTED");
    
    // Load rooms from text files (temporarily for backward compatibility)
    load_rooms();
    practiceQuestionCount = loadQuestionsTxt("data/questions.txt", practiceQuestions, MAX_Q, NULL, NULL);

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