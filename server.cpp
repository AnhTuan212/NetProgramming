// ========================================
// server.cpp — Online Test Server (Fixed)
// ========================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>

// --- Removed custom .h includes ---

#define PORT 9000
#define MAX_CLIENTS 50
#define MAX_ROOMS 100
#define MAX_PARTICIPANTS 50
#define MAX_QUESTIONS_PER_ROOM 50
#define BUF_SIZE 4096
#define MAX_Q 100 // Added definition

// ======================= Structures =======================

// Manually added QItem struct definition
typedef struct {
    int id;
    char text[256];
    char A[128];
    char B[128];
    char C[128];
    char D[128];
    char correct; // 'A'..'D'
    char topic[64];
    char difficulty[32];
} QItem;

typedef struct {
    char username[64];
    int score;
    char answers[MAX_QUESTIONS_PER_ROOM];
} Participant;

typedef struct {
    char name[64];
    char owner[64];
    int numQuestions;
    int duration;
    QItem questions[MAX_QUESTIONS_PER_ROOM]; // Using QItem
    Participant participants[MAX_PARTICIPANTS];
    int participantCount;
    int started;
} Room;

typedef struct {
    int sock;
    char username[64];
    int loggedIn;
    char role[32];
} Client;

// ======================= External Function Prototypes =======================
// Manually added prototypes from other .cpp files

// From logger.cpp
void writeLog(const char *event);

// From user_manager.cpp
int register_user(const char *username, const char *password);
int validate_user(const char *username, const char *password, char *role_out);

// From question_bank.cpp
int loadQuestionsTxt(const char *filename, QItem questions[], int maxQ, const char* topic, const char* diff);


// ======================= Globals =======================
Room rooms[MAX_ROOMS];
int roomCount = 0;
QItem practiceQuestions[MAX_Q]; // Global bank for practice mode
int practiceQuestionCount = 0;
pthread_mutex_t lock;

// ======================= Utilities =======================
void trim_newline(char *s) {
    char *p = strchr(s, '\n');
    if (p) *p = 0;
    p = strchr(s, '\r');
    if (p) *p = 0;
}

void send_msg(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

// ======================= Room Management =======================
Room* find_room(const char *name) {
    for (int i = 0; i < roomCount; i++) {
        if (strcmp(rooms[i].name, name) == 0)
            return &rooms[i];
    }
    return NULL;
}

Participant* find_participant(Room *r, const char *user) {
    for (int i = 0; i < r->participantCount; i++) {
        if (strcmp(r->participants[i].username, user) == 0)
            return &r->participants[i];
    }
    return NULL;
}

// ======================= Command Handling =======================
void* handle_client(void *arg) {
    Client *cli = (Client*)arg;
    char buffer[BUF_SIZE];
    char log_msg[512];

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(cli->sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;

        trim_newline(buffer);
        char cmd[32];
        sscanf(buffer, "%31s", cmd);

        pthread_mutex_lock(&lock);

        if (strcmp(cmd, "REGISTER") == 0) {
            char user[64], pass[64];
            if (sscanf(buffer, "REGISTER %63s %63s", user, pass) == 2) {
                int result = register_user(user, pass);
                if (result == 1) {
                    send_msg(cli->sock, "SUCCESS Registered. Please login.\n");
                    sprintf(log_msg, "User %s registered.", user);
                    writeLog(log_msg);
                } else if (result == 0) {
                    send_msg(cli->sock, "FAIL User already exists\n");
                } else {
                    send_msg(cli->sock, "FAIL Server error registering user\n");
                }
            } else {
                send_msg(cli->sock, "FAIL Invalid REGISTER format\n");
            }
        }
        
        else if (strcmp(cmd, "LOGIN") == 0) {
            char user[64], pass[64];
            sscanf(buffer, "LOGIN %63s %63s", user, pass);
            char role[32] = "student";

            if (validate_user(user, pass, role)) {
                strcpy(cli->username, user);
                strcpy(cli->role, role);
                cli->loggedIn = 1;

                char msg[128];
                sprintf(msg, "SUCCESS %s\n", role); // Send SUCCESS
                send_msg(cli->sock, msg);
                
                sprintf(log_msg, "User %s logged in as %s.", user, role);
                writeLog(log_msg);
            } else {
                send_msg(cli->sock, "FAIL Invalid username or password\n");
            }
        }

        else if (!cli->loggedIn) {
            send_msg(cli->sock, "FAIL Please login first\n");
        }

        else if (strcmp(cmd, "CREATE") == 0) {
            char roomName[64], topic[64] = "", diff[32] = "";
            int numQ, dur;
            int n = sscanf(buffer, "CREATE %63s %d %d %63s %31s", roomName, &numQ, &dur, topic, diff);
            
            if (n < 3) {
                 send_msg(cli->sock, "FAIL Usage: CREATE <name> <numQ> <duration> [topic] [difficulty]\n");
            } else if (find_room(roomName)) {
                send_msg(cli->sock, "FAIL Room exists\n");
            } else if (numQ > MAX_QUESTIONS_PER_ROOM) {
                sprintf(buffer, "FAIL Max questions is %d\n", MAX_QUESTIONS_PER_ROOM);
                send_msg(cli->sock, buffer);
            } else {
                Room *r = &rooms[roomCount++];
                strcpy(r->name, roomName);
                strcpy(r->owner, cli->username);
                r->duration = dur;
                r->participantCount = 0;
                r->started = 0;
                
                // Load questions using module, with filters
                r->numQuestions = loadQuestionsTxt("data/questions.txt", r->questions, numQ, 
                                                 (n >= 4 ? topic : NULL), 
                                                 (n >= 5 ? diff : NULL));

                send_msg(cli->sock, "SUCCESS Room created\n");
                sprintf(log_msg, "User %s created room %s (%d questions).", cli->username, roomName, r->numQuestions);
                writeLog(log_msg);
            }
        }

        else if (strcmp(cmd, "LIST") == 0) {
            char msg[2048] = "SUCCESS Rooms:\n";
            if (roomCount == 0) {
                strcat(msg, "No rooms available.\n");
            }
            for (int i = 0; i < roomCount; i++) {
                char line[256];
                sprintf(line, "- %s (Owner: %s, Qs: %d, Time: %d sec, Status: %s)\n",
                        rooms[i].name, rooms[i].owner, rooms[i].numQuestions, rooms[i].duration,
                        rooms[i].started ? "Started" : "Waiting");
                strcat(msg, line);
            }
            send_msg(cli->sock, msg);
        }

        else if (strcmp(cmd, "JOIN") == 0) {
            char roomName[64];
            sscanf(buffer, "JOIN %63s", roomName); // Fixed: only need room name
            Room *r = find_room(roomName);
            if (!r) {
                send_msg(cli->sock, "FAIL Room not found\n");
            } else if (r->started) {
                send_msg(cli->sock, "FAIL Test has already started\n");
            } else {
                Participant *p = find_participant(r, cli->username);
                if (!p) {
                    p = &r->participants[r->participantCount++];
                    strcpy(p->username, cli->username);
                    p->score = 0;
                    memset(p->answers, 0, sizeof(p->answers));
                }
                send_msg(cli->sock, "SUCCESS Joined room\n");
                sprintf(log_msg, "User %s joined room %s.", cli->username, roomName);
                writeLog(log_msg);
            }
        }

        else if (strcmp(cmd, "START") == 0) {
            char roomName[64];
            sscanf(buffer, "START %63s", roomName);
            Room *r = find_room(roomName);
            if (!r) send_msg(cli->sock, "FAIL Room not found\n");
            else if (strcmp(r->owner, cli->username) != 0 && strcmp(cli->role, "admin") != 0) {
                send_msg(cli->sock, "FAIL Only the owner or admin can start the test\n");
            } else {
                r->started = 1;
                // Send SUCCESS with num questions and duration
                sprintf(buffer, "SUCCESS %d %d\n", r->numQuestions, r->duration);
                send_msg(cli->sock, buffer);
                
                sprintf(log_msg, "Room %s started by %s.", roomName, cli->username);
                writeLog(log_msg);
            }
        }

        else if (strcmp(cmd, "GET_QUESTION") == 0) {
            char roomName[64];
            int q_index;
            sscanf(buffer, "GET_QUESTION %63s %d", roomName, &q_index);
            Room *r = find_room(roomName);
            if (!r) send_msg(cli->sock, "FAIL Room not found\n");
            else if (!r->started) send_msg(cli->sock, "FAIL Test not started\n");
            else if (q_index < 0 || q_index >= r->numQuestions) send_msg(cli->sock, "FAIL Invalid question index\n");
            else {
                // Send the full question data
                QItem *q = &r->questions[q_index];
                sprintf(buffer, "[%d/%d] %s\nA) %s\nB) %s\nC) %s\nD) %s\n",
                        q_index + 1, r->numQuestions, q->text, q->A, q->B, q->C, q->D);
                send_msg(cli->sock, buffer);
            }
        }

        else if (strcmp(cmd, "SUBMIT") == 0) {
            char roomName[64], answers[256];
            sscanf(buffer, "SUBMIT %63s %255s", roomName, answers);
            Room *r = find_room(roomName);
            if (!r) send_msg(cli->sock, "FAIL Room not found\n");
            else {
                Participant *p = find_participant(r, cli->username);
                if (!p) send_msg(cli->sock, "FAIL You are not in this room\n");
                else {
                    int score = 0;
                    for (int i = 0; i < r->numQuestions && i < (int)strlen(answers); i++) {
                        if (answers[i] == '.') continue; // Unanswered
                        if (toupper(answers[i]) == r->questions[i].correct)
                            score++;
                    }
                    p->score = score;
                    
                    // --- Feature from stats.h REMOVED ---
                    // append_result_file(roomName, cli->username, score);
                    
                    char msg[128];
                    sprintf(msg, "SUCCESS Submitted. Score=%d/%d\n", score, r->numQuestions);
                    send_msg(cli->sock, msg);
                    
                    sprintf(log_msg, "User %s submitted for room %s. Score: %d/%d.", cli->username, roomName, score, r->numQuestions);
                    writeLog(log_msg);
                }
            }
        }
        
        else if (strcmp(cmd, "RESULTS") == 0) {
            char roomName[64];
            sscanf(buffer, "RESULTS %63s", roomName);
            Room *r = find_room(roomName);
            if (!r) send_msg(cli->sock, "FAIL Room not found\n");
            else {
                char msg[2048];
                sprintf(msg, "SUCCESS Results for %s (%d Qs):\n", roomName, r->numQuestions);
                for(int i=0; i<r->participantCount; i++) {
                    char line[128];
                    sprintf(line, "- %-20s: %d/%d\n", r->participants[i].username, r->participants[i].score, r->numQuestions);
                    strcat(msg, line);
                }
                send_msg(cli->sock, msg);
            }
        }
        
        else if (strcmp(cmd, "LEADERBOARD") == 0) {
            // --- Feature from stats.h REMOVED ---
            // char *board = get_leaderboard();
            // send_msg(cli->sock, board);
            send_msg(cli->sock, "FAIL Leaderboard feature is disabled.\n");
        }
        
        else if (strcmp(cmd, "PRACTICE") == 0) {
            if (practiceQuestionCount == 0) {
                send_msg(cli->sock, "FAIL No practice questions available on server.\n");
            } else {
                // Send one random question
                int rand_idx = rand() % practiceQuestionCount;
                QItem *q = &practiceQuestions[rand_idx];
                sprintf(buffer, "PRACTICE_Q %s\nA) %s\nB) %s\nC) %s\nD) %s\n%c\n",
                        q->text, q->A, q->B, q->C, q->D, q->correct);
                send_msg(cli->sock, buffer);
            }
        }

        else if (strcmp(cmd, "EXIT") == 0) {
            send_msg(cli->sock, "SUCCESS Goodbye\n");
            pthread_mutex_unlock(&lock);
            break;
        }

        else {
            send_msg(cli->sock, "FAIL Unknown command\n");
        }

        pthread_mutex_unlock(&lock);
    }

    close(cli->sock);
    sprintf(log_msg, "Client %s disconnected.", cli->username[0] ? cli->username : "(unknown)");
    writeLog(log_msg);
    free(cli);
    return NULL;
}

// ======================= Main =======================
int main() {
    pthread_mutex_init(&lock, NULL);
    srand(time(NULL)); // For practice mode

    // Load practice questions at start
    practiceQuestionCount = loadQuestionsTxt("data/questions.txt", practiceQuestions, MAX_Q, NULL, NULL);
    if (practiceQuestionCount > 0) {
        printf("Loaded %d practice questions.\n", practiceQuestionCount);
    } else {
        printf("Warning: Could not load practice questions from data/questions.txt\n");
    }

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    // Allow address reuse
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_sock, 10);
    printf("Server running on port %d...\n", PORT);
    writeLog("SERVER_STARTED");

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t len = sizeof(cli_addr);
        int cli_sock = accept(server_sock, (struct sockaddr*)&cli_addr, &len);
        if (cli_sock >= 0) {
            Client *cli = (Client*) malloc(sizeof(Client));
            cli->sock = cli_sock;
            cli->loggedIn = 0;
            strcpy(cli->username, "");
            strcpy(cli->role, "student");

            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, cli);
            pthread_detach(tid);
        }
    }

    close(server_sock);
    pthread_mutex_destroy(&lock);
    return 0;
}