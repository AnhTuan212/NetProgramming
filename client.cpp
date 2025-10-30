#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000

#define BUFFER_SIZE 8192

// === State ===
int sockfd;
char currentUser[100] = "";
char currentRoom[100] = "";
int loggedIn = 0;
int connected = 0;

// === Helper ===
void trim_input_newline(char *s) {
    s[strcspn(s, "\n")] = 0;
}

void send_message(const char *msg) {
    char full_msg[BUFFER_SIZE];
    snprintf(full_msg, sizeof(full_msg), "%s\n", msg); // Send newline
    send(sockfd, full_msg, strlen(full_msg), 0);
}

int recv_message(char *buffer, int size) {
    memset(buffer, 0, size);
    int n = read(sockfd, buffer, size - 1);
    if (n > 0) {
        buffer[n] = 0; // Null terminate
    }
    return n;
}

void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void print_banner() {
    system("clear");
    printf("====== ONLINE TEST CLIENT ======\n");
    if (loggedIn) {
        printf("👤 Logged in as: %s\n", currentUser);
        if (strlen(currentRoom) > 0) printf("🏠 Current Room: %s\n", currentRoom);
    } else {
        printf("👤 Not logged in\n");
    }
    printf("===========================================\n");
    if (!loggedIn) {
        printf("1. Login\n");
        printf("2. Register\n");
    } else {
        printf("3. Create Test Room\n");
        printf("4. View Room List\n");
        printf("5. Join Room\n");
        printf("6. Start Test (Interactive)\n");
        printf("7. View Room Results\n");
        printf("8. View Leaderboard\n");
        printf("9. Practice (1 Random Question)\n");
    }
    printf("0. Exit\n");
    printf("===========================================\n>> ");
}

// === Menu Handlers ===

void handle_register() {
    char user[100], pass[100], buffer[BUFFER_SIZE];
    printf("Enter new username: ");
    fgets(user, sizeof(user), stdin);
    trim_input_newline(user);
    printf("Enter new password: ");
    fgets(pass, sizeof(pass), stdin);
    trim_input_newline(pass);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "REGISTER %s %s", user, pass);
    send_message(cmd);
    
    recv_message(buffer, sizeof(buffer));
    printf("%s", buffer);
}

void handle_login() {
    char user[100], pass[100], buffer[BUFFER_SIZE];
    printf("Enter username: ");
    fgets(user, sizeof(user), stdin);
    trim_input_newline(user);
    printf("Enter password: ");
    fgets(pass, sizeof(pass), stdin);
    trim_input_newline(pass);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "LOGIN %s %s", user, pass);
    send_message(cmd);

    recv_message(buffer, sizeof(buffer));
    printf("%s", buffer); // Show server message
    
    if (strncmp(buffer, "SUCCESS", 7) == 0) { // Check for SUCCESS
        loggedIn = 1;
        strcpy(currentUser, user);
    }
}

void handle_create_room() {
    if (!loggedIn) { printf("⚠️ Please login first.\n"); return; }
    
    char room[100], topic[100], diff[100];
    int num, dur;
    
    printf("Room name: ");
    fgets(room, sizeof(room), stdin);
    trim_input_newline(room);
    
    printf("Number of questions: ");
    scanf("%d", &num);
    
    printf("Duration (seconds): ");
    scanf("%d", &dur);
    clear_stdin(); // Consume newline
    
    printf("Topic (optional, press Enter to skip): ");
    fgets(topic, sizeof(topic), stdin);
    trim_input_newline(topic);
    
    printf("Difficulty (optional, press Enter to skip): ");
    fgets(diff, sizeof(diff), stdin);
    trim_input_newline(diff);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "CREATE %s %d %d %s %s", room, num, dur, topic, diff);
    send_message(cmd);

    char buffer[BUFFER_SIZE];
    recv_message(buffer, sizeof(buffer));
    printf("%s", buffer);
}

void handle_view_rooms() {
    if (!loggedIn) { printf("⚠️ Please login first.\n"); return; }
    
    send_message("LIST");
    char buffer[BUFFER_SIZE];
    recv_message(buffer, sizeof(buffer));
    printf("\n====== AVAILABLE ROOMS ======\n");
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        printf("%s", buffer + 8); // Print after "SUCCESS "
    } else {
        printf("%s", buffer);
    }
    printf("=============================\n");
}

void handle_join_room() {
    if (!loggedIn) { printf("⚠️ Please login first.\n"); return; }
    
    char room[100], buffer[BUFFER_SIZE];
    printf("Enter room name to join: ");
    fgets(room, sizeof(room), stdin);
    trim_input_newline(room);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "JOIN %s", room); // Fixed: only send room
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    
    printf("%s", buffer);
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        strcpy(currentRoom, room);
    }
}

void handle_start_test() {
    if (strlen(currentRoom) == 0) {
        printf("⚠️ Join a room first.\n");
        return;
    }

    char cmd[256], buffer[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "START %s", currentRoom);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));

    if (strncmp(buffer, "SUCCESS", 7) != 0) {
        printf("❌ Failed to start: %s\n", buffer);
        return;
    }

    int totalQ, duration;
    sscanf(buffer, "SUCCESS %d %d", &totalQ, &duration);
    if (totalQ == 0) {
        printf("This room has 0 questions. Test complete.\n");
        return;
    }
    printf("🎯 Test starting! %d questions, %d seconds.\n", totalQ, duration);

    char *answers = (char*)malloc(totalQ + 1);
    memset(answers, '.', totalQ); // '.' = unanswered
    answers[totalQ] = 0;

    char **questions = (char**)malloc(sizeof(char*) * totalQ);

    // 1. Download all questions
    for (int i = 0; i < totalQ; i++) {
        snprintf(cmd, sizeof(cmd), "GET_QUESTION %s %d", currentRoom, i);
        send_message(cmd);
        questions[i] = (char*)malloc(BUFFER_SIZE);
        recv_message(questions[i], BUFFER_SIZE);
    }

    // 2. Interactive answer loop
    while (1) {
        system("clear");
        printf("=== TEST IN PROGRESS: %s ===\n", currentRoom);
        for (int i = 0; i < totalQ; i++) {
            char q_line[100];
            strncpy(q_line, questions[i], 99);
            q_line[99] = 0;
            char* first_nl = strchr(q_line, '\n');
            if (first_nl) *first_nl = 0;
            
            printf("%d. %s (Answer: %c)\n", i + 1, q_line, answers[i]);
        }
        printf("----------------------------------\n");
        printf("Enter question number (1-%d) to answer/change, or 'SUBMIT': ", totalQ);

        char input[32];
        fgets(input, sizeof(input), stdin);
        trim_input_newline(input);
        
        if (strcasecmp(input, "SUBMIT") == 0) {
            printf("Are you sure you want to submit? (y/n): ");
            fgets(input, sizeof(input), stdin);
            if (input[0] == 'y' || input[0] == 'Y') {
                break; // Exit loop to submit
            }
            continue;
        }
        
        int q_num = atoi(input);
        if (q_num < 1 || q_num > totalQ) {
            printf("Invalid question number. Press Enter.\n");
            getchar();
            continue;
        }
        
        // Show the selected question
        system("clear");
        printf("%s\n", questions[q_num - 1]);
        
        char ans_in[10];
        while (1) {
            printf("Your Answer (A/B/C/D): ");
            fgets(ans_in, sizeof(ans_in), stdin);
            trim_input_newline(ans_in);
            if (strlen(ans_in) == 1 && strchr("ABCDabcd", ans_in[0])) {
                answers[q_num - 1] = toupper(ans_in[0]);
                break;
            }
            printf("Invalid input. Must be A, B, C, or D.\n");
        }
    }

    // 3. Submit
    snprintf(cmd, sizeof(cmd), "SUBMIT %s %s", currentRoom, answers);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("✅ Test submitted! %s\n", buffer);

    // Cleanup
    strcpy(currentRoom, ""); // Exit room after submit
    free(answers);
    for (int i = 0; i < totalQ; i++) free(questions[i]);
    free(questions);
}


void handle_view_results() {
    if (!loggedIn) { printf("⚠️ Please login first.\n"); return; }
    
    char room[100];
    printf("Room name to view results: ");
    fgets(room, sizeof(room), stdin);
    trim_input_newline(room);

    char cmd[256], buffer[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "RESULTS %s", room);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    
    printf("\n====== RESULTS (%s) ======\n", room);
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        printf("%s", buffer + 8);
    } else {
        printf("%s", buffer);
    }
    printf("=============================\n");
}

void handle_leaderboard() {
    if (!loggedIn) { printf("⚠️ Please login first.\n"); return; }
    
    send_message("LEADERBOARD");
    char buffer[BUFFER_SIZE];
    recv_message(buffer, sizeof(buffer));
    printf("\n%s\n", buffer);
}

void handle_practice() {
    if (!loggedIn) { printf("⚠️ Please login first.\n"); return; }
    
    send_message("PRACTICE");
    char buffer[BUFFER_SIZE];
    recv_message(buffer, sizeof(buffer));
    
    if (strncmp(buffer, "PRACTICE_Q", 10) == 0) {
        char *question = buffer + 11;
        char *correct_ans = strrchr(question, '\n');
        *correct_ans = 0; // Split question from answer
        correct_ans++;
        
        printf("\n--- Practice Question ---\n");
        printf("%s\n", question);
        
        char ans_in[10];
        while (1) {
            printf("Your Answer (A/B/C/D): ");
            fgets(ans_in, sizeof(ans_in), stdin);
            trim_input_newline(ans_in);
            if (strlen(ans_in) == 1 && strchr("ABCDabcd", ans_in[0])) {
                break;
            }
            printf("Invalid input.\n");
        }
        
        if (toupper(ans_in[0]) == correct_ans[0]) {
            printf("Correct! The answer was %c.\n", correct_ans[0]);
        } else {
            printf("Incorrect. The correct answer was %c.\n", correct_ans[0]);
        }
        
    } else {
        printf("%s", buffer); // Print "FAIL No practice questions..."
    }
}

void handle_exit() {
    if (loggedIn) send_message("EXIT");
    printf("Goodbye %s 👋\n", currentUser);
    close(sockfd);
    connected = 0;
}

// === MAIN ===
int main() {
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("❌ Connection failed");
        return 1;
    }

    connected = 1;
    printf("Connected to Online Test Server.\n");
    printf("Press Enter to start...");
    getchar();


    while (connected) {
        print_banner();
        char choice[10];
        fgets(choice, sizeof(choice), stdin);
        trim_input_newline(choice);

        if (!loggedIn) {
            if (strcmp(choice, "1") == 0) handle_login();
            else if (strcmp(choice, "2") == 0) handle_register();
            else if (strcmp(choice, "0") == 0) { handle_exit(); break; }
            else printf("Invalid choice!\n");
        } else {
            if (strcmp(choice, "3") == 0) handle_create_room();
            else if (strcmp(choice, "4") == 0) handle_view_rooms();
            else if (strcmp(choice, "5") == 0) handle_join_room();
            else if (strcmp(choice, "6") == 0) handle_start_test();
            else if (strcmp(choice, "7") == 0) handle_view_results();
            else if (strcmp(choice, "8") == 0) handle_leaderboard();
            else if (strcmp(choice, "9") == 0) handle_practice();
            else if (strcmp(choice, "0") == 0) { handle_exit(); break; }
            else printf("Invalid choice!\n");
        }

        if (connected) {
            printf("\nPress Enter to continue...");
            getchar();
        }
    }

    return 0;
}