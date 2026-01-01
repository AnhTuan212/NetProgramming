#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#include <strings.h>

#define SERVER_PORT 9000
#define BUFFER_SIZE 8192

int sockfd;
char currentUser[100] = "";
char currentRole[32] = "";
char currentRoom[100] = "";
int loggedIn = 0;
int connected = 0;
int inTest = 0;

void trim_input_newline(char *s) {
    s[strcspn(s, "\n")] = 0;
}

void send_message(const char *msg) {
    char full_msg[BUFFER_SIZE];
    snprintf(full_msg, sizeof(full_msg), "%s\n", msg);
    send(sockfd, full_msg, strlen(full_msg), 0);
}

int recv_message(char *buffer, int size) {
    memset(buffer, 0, size);
    int n = read(sockfd, buffer, size - 1);
    if (n > 0) buffer[n] = 0;
    return n;
}

void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void to_lowercase_client(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void print_banner() {
    system("clear");
    printf("====== ONLINE TEST CLIENT ======\n");
    if (loggedIn) {
        printf("Logged in as: %s (%s)\n", currentUser, currentRole);
        if (strlen(currentRoom) > 0) printf("In Room: %s\n", currentRoom);
        if (inTest) printf("*** DOING TEST ***\n");
    } else {
        printf("Not logged in\n");
    }
    printf("===========================================\n");

    if (!loggedIn) {
        printf("1. Login\n");
        printf("2. Register\n");
    } else if (strcmp(currentRole, "admin") == 0) {
        printf("3. Create Test Room\n");
        printf("4. View Room List\n");
        printf("5. Preview Test (Your Rooms)\n");
        printf("6. Delete Test Room (Your Rooms)\n");
        printf("7. Join Room (Preview/Do)\n");
        printf("8. View Room Results\n");
        printf("9. View Leaderboard\n");
        printf("10. Practice\n");
        printf("11. Add Questions\n");
        printf("12. Delete Questions\n");
    } else {
        printf("3. View Room List\n");
        printf("4. Join Room → Start Test\n");
        printf("5. View Room Results\n");
        printf("6. View Leaderboard\n");
        printf("7. Practice\n");
    }
    printf("0. Exit\n");
    printf("===========================================\n>> ");
}

void handle_register() {
    char user[100], pass[100], role[20], buffer[BUFFER_SIZE];
    char adminCode[100] = "";

    printf("Enter new username: ");
    fgets(user, sizeof(user), stdin); trim_input_newline(user);
    printf("Enter new password: ");
    fgets(pass, sizeof(pass), stdin); trim_input_newline(pass);
    
    while (1) {
        printf("Role (admin/student): ");
        fgets(role, sizeof(role), stdin); trim_input_newline(role);
        if (strcasecmp(role, "admin") == 0 || strcasecmp(role, "student") == 0) break;
        printf("Invalid role!\n");
    }

    if (strcasecmp(role, "admin") == 0) {
        printf("Enter Admin Secret Code: ");
        fgets(adminCode, sizeof(adminCode), stdin); 
        trim_input_newline(adminCode);
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "REGISTER %s %s %s %s", user, pass, role, adminCode);
    
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void handle_login() {
    char user[100], pass[100], buffer[BUFFER_SIZE];
    printf("Enter username: ");
    fgets(user, sizeof(user), stdin); trim_input_newline(user);
    printf("Enter password: ");
    fgets(pass, sizeof(pass), stdin); trim_input_newline(pass);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "LOGIN %s %s", user, pass);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("%s\n", buffer);
    
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        loggedIn = 1;
        strcpy(currentUser, user);
        sscanf(buffer, "SUCCESS %31s", currentRole);
    }
}

void handle_create_room() {
    if (!loggedIn || strcmp(currentRole, "admin") != 0) {
        printf("Only admin can create rooms.\n"); return;
    }
    
    char room[100];
    int total_num, dur;
    
    printf("Room name: ");
    fgets(room, sizeof(room), stdin); trim_input_newline(room);
    printf("Total number of questions: ");
    scanf("%d", &total_num); clear_stdin();
    printf("Duration (seconds): ");
    scanf("%d", &dur); clear_stdin();
    
    // Get topic counts from server
    char buffer[BUFFER_SIZE];
    send_message("GET_TOPICS");
    recv_message(buffer, sizeof(buffer));
    
    printf("\n====== SELECT TOPICS AND QUESTION DISTRIBUTION ======\n");
    
    char topic_selection[512] = "";
    
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        char *topicData = buffer + 8;
        
        // Parse topics with counts from format: topic1:count1|topic2:count2|...
        char topics_copy[512];
        strcpy(topics_copy, topicData);
        
        // Extract individual topics
        char *topic_list[32];
        int topic_counts[32];
        int topic_count = 0;
        char *saveptr;
        char *token = strtok_r(topics_copy, "|", &saveptr);
        
        while (token && topic_count < 32) {
            // Parse format: "topic_name:count"
            char *colon = strchr(token, ':');
            if (colon) {
                // Extract topic name
                int name_len = colon - token;
                topic_list[topic_count] = malloc(name_len + 1);
                strncpy(topic_list[topic_count], token, name_len);
                topic_list[topic_count][name_len] = '\0';
                
                // Extract count
                topic_counts[topic_count] = atoi(colon + 1);
                topic_count++;
            }
            token = strtok_r(NULL, "|", &saveptr);
        }
        
        // Display available topics
        printf("\n====== AVAILABLE TOPICS ======\n");
        if (topic_count > 0) {
            for (int i = 0; i < topic_count; i++) {
                printf("%s: %d questions\n", topic_list[i], topic_counts[i]);
            }
        }
        
        printf("\nEnter topics to select (format: topic1:count1 topic2:count2 ...)\n");
        printf("Example: database:3 oop:2 network_programming:5\n");
        printf("Enter '#' to skip topic filter.\n\n");
        
        // Single-line input for topic selection
        printf("Topic selection: ");
        fflush(stdout);
        char input[512];
        fgets(input, sizeof(input), stdin);
        trim_input_newline(input);
        
        if (strcmp(input, "#") != 0 && strlen(input) > 0) {
            // Remove trailing '#' if present
            char *hash_pos = strchr(input, '#');
            if (hash_pos) {
                *hash_pos = '\0';
            }
            
            // Trim trailing spaces
            int len = strlen(input);
            while (len > 0 && isspace(input[len - 1])) {
                input[--len] = '\0';
            }
            
            // Convert input to lowercase
            for (int j = 0; input[j]; j++) {
                input[j] = tolower(input[j]);
            }
            strcpy(topic_selection, input);
        }
        
        // Free allocated memory
        for (int i = 0; i < topic_count; i++) {
            free(topic_list[i]);
        }
    } else {
        printf("\n====== AVAILABLE TOPICS ======\n");
        printf("(Could not retrieve topic list)\n\n");
        printf("Enter topics to select (format: topic1:count1 topic2:count2 ...)\n");
        printf("Example: database:3 oop:2 network_programming:5\n");
        printf("Enter '#' to skip topic filter.\n\n");
        
        // Single-line input for manual topic entry
        printf("Topic selection: ");
        fflush(stdout);
        char input[512];
        fgets(input, sizeof(input), stdin);
        trim_input_newline(input);
        
        if (strcmp(input, "#") != 0 && strlen(input) > 0) {
            // Remove trailing '#' if present
            char *hash_pos = strchr(input, '#');
            if (hash_pos) {
                *hash_pos = '\0';
            }
            
            // Trim trailing spaces
            int len = strlen(input);
            while (len > 0 && isspace(input[len - 1])) {
                input[--len] = '\0';
            }
            
            // Convert input to lowercase
            for (int j = 0; input[j]; j++) {
                input[j] = tolower(input[j]);
            }
            strcpy(topic_selection, input);
        }
    }
    
    // Get difficulty counts from server based on selected topics
    if (strlen(topic_selection) > 0) {
        // Send topics to server and get available difficulties
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "GET_DIFFICULTIES_FOR_TOPICS %s", topic_selection);
        send_message(cmd);
        recv_message(buffer, sizeof(buffer));
    } else {
        // No topic filter, get all difficulties
        send_message("GET_DIFFICULTIES");
        recv_message(buffer, sizeof(buffer));
    }
    
    printf("\n====== SELECT DIFFICULTIES AND DISTRIBUTION ======\n");
    
    char difficulty_selection[512] = "";
    
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        char *diffData = buffer + 8;
        
        // Parse difficulties with counts from format: difficulty1:count1|difficulty2:count2|...
        char diff_copy[512];
        strcpy(diff_copy, diffData);
        
        // Extract individual difficulties
        char *diff_list[10];
        int diff_counts[10];
        int diff_count = 0;
        char *saveptr;
        char *token = strtok_r(diff_copy, "|", &saveptr);
        
        while (token && diff_count < 10) {
            // Parse format: "difficulty_name:count"
            char *colon = strchr(token, ':');
            if (colon) {
                // Extract difficulty name
                int name_len = colon - token;
                diff_list[diff_count] = malloc(name_len + 1);
                strncpy(diff_list[diff_count], token, name_len);
                diff_list[diff_count][name_len] = '\0';
                
                // Extract count
                diff_counts[diff_count] = atoi(colon + 1);
                diff_count++;
            }
            token = strtok_r(NULL, "|", &saveptr);
        }
        
        // Display available difficulties
        printf("\n====== AVAILABLE DIFFICULTIES ======\n");
        if (diff_count > 0) {
            for (int i = 0; i < diff_count; i++) {
                printf("%s: %d questions\n", diff_list[i], diff_counts[i]);
            }
        }
        
        printf("\nEnter difficulties to select (format: easy:count medium:count hard:count)\n");
        printf("Example: easy:3 medium:4 hard:2\n");
        printf("Enter '#' to skip difficulty filter.\n\n");
        
        // Single-line input for difficulty selection
        printf("Difficulty selection: ");
        fflush(stdout);
        char input[512];
        fgets(input, sizeof(input), stdin);
        trim_input_newline(input);
        
        if (strcmp(input, "#") != 0 && strlen(input) > 0) {
            // Remove trailing '#' if present
            char *hash_pos = strchr(input, '#');
            if (hash_pos) {
                *hash_pos = '\0';
            }
            
            // Trim trailing spaces
            int len = strlen(input);
            while (len > 0 && isspace(input[len - 1])) {
                input[--len] = '\0';
            }
            
            // Convert input to lowercase
            for (int j = 0; input[j]; j++) {
                input[j] = tolower(input[j]);
            }
            strcpy(difficulty_selection, input);
        }
        
        // Free allocated memory
        for (int i = 0; i < diff_count; i++) {
            free(diff_list[i]);
        }
    } else {
        printf("\n====== AVAILABLE DIFFICULTIES ======\n");
        printf("(Could not retrieve difficulty list)\n\n");
        printf("Enter difficulties to select (format: easy:count medium:count hard:count)\n");
        printf("Example: easy:3 medium:4 hard:2\n");
        printf("Enter '#' to skip difficulty filter.\n\n");
        
        // Single-line input for manual difficulty entry
        printf("Difficulty selection: ");
        fflush(stdout);
        char input[512];
        fgets(input, sizeof(input), stdin);
        trim_input_newline(input);
        
        if (strcmp(input, "#") != 0 && strlen(input) > 0) {
            // Remove trailing '#' if present
            char *hash_pos = strchr(input, '#');
            if (hash_pos) {
                *hash_pos = '\0';
            }
            
            // Trim trailing spaces
            int len = strlen(input);
            while (len > 0 && isspace(input[len - 1])) {
                input[--len] = '\0';
            }
            
            // Convert to lowercase
            for (int j = 0; input[j]; j++) {
                input[j] = tolower(input[j]);
            }
            strcpy(difficulty_selection, input);
        }
    }
    
    char cmd[1024];
    if (strlen(topic_selection) == 0 && strlen(difficulty_selection) == 0) {
        // Use all questions mode
        snprintf(cmd, sizeof(cmd), "CREATE %s %d %d", room, total_num, dur);
    } else {
        // Use distributed selection with topic and difficulty filters
        snprintf(cmd, sizeof(cmd), "CREATE %s %d %d TOPICS %s DIFFICULTIES %s", 
                 room, total_num, dur, topic_selection, difficulty_selection);
    }
    
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        printf("\nRoom created successfully!\n");
    } else {
        printf("\nError: %s\n", buffer);
    }
}

void handle_list_rooms() {
    send_message("LIST");
    char buffer[BUFFER_SIZE];
    recv_message(buffer, sizeof(buffer));
    printf("\n====== AVAILABLE ROOMS ======\n");
    printf("%s", strncmp(buffer, "SUCCESS", 7) == 0 ? buffer + 8 : buffer);
    printf("=============================\n");
}

void handle_join_room() {
    char room[100], buffer[BUFFER_SIZE];
    printf("Enter room name: ");
    fgets(room, sizeof(room), stdin); trim_input_newline(room);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "JOIN %s", room);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("%s\n", buffer);

    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        strcpy(currentRoom, room);
        int totalQ, duration;
        sscanf(buffer, "SUCCESS Joined %d %d", &totalQ, &duration);
        printf("Starting test: %d questions, %d seconds.\n", totalQ, duration);
        inTest = 1;
        handle_start_test(totalQ, duration);
    }
}

void handle_preview_room() {
    if (strcmp(currentRole, "admin") != 0) {
        printf("Only admin can preview.\n"); return;
    }
    char room[100], buffer[BUFFER_SIZE];
    printf("Room name to preview: ");
    fgets(room, sizeof(room), stdin); trim_input_newline(room);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "PREVIEW %s", room);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("\n====== PREVIEW: %s ======\n", room);
    printf("%s", strncmp(buffer, "SUCCESS", 7) == 0 ? buffer + 8 : buffer);
    printf("=============================\n");
}

void handle_delete_room() {
    if (strcmp(currentRole, "admin") != 0) {
        printf("Only admin can delete.\n"); return;
    }
    char room[100], buffer[BUFFER_SIZE];
    printf("Room name to delete: ");
    fgets(room, sizeof(room), stdin); trim_input_newline(room);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "DELETE %s", room);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

// --- HÀM SỬA LOGIC HIỂN THỊ CÂU HỎI ---
void handle_start_test(int totalQ, int duration) {
    if (totalQ == 0) { printf("No questions.\n"); return; }

    char *answers = malloc(totalQ + 1);
    memset(answers, '.', totalQ); answers[totalQ] = '\0';
    
    // Cấp phát bộ nhớ cho các câu hỏi
    char **questions = malloc(sizeof(char*) * totalQ);
    for(int i=0; i<totalQ; i++) questions[i] = malloc(BUFFER_SIZE);

    // Tải sơ bộ câu hỏi (lần đầu)
    char cmd[256], buffer[BUFFER_SIZE];
    for (int i = 0; i < totalQ; i++) {
        snprintf(cmd, sizeof(cmd), "GET_QUESTION %s %d", currentRoom, i);
        send_message(cmd);
        if (recv_message(questions[i], BUFFER_SIZE) <= 0) {
            printf("Error loading question %d.\n", i+1);
            return;
        }
    }

    time_t start_time = time(NULL);
    while (1) {
        system("clear");
        int remaining = duration - (int)(time(NULL) - start_time);
        if (remaining < 0) remaining = 0;
        
        printf("=== TEST: %s | Time left: %ds ===\n", currentRoom, remaining);
        
        if (remaining == 0) {
            printf("\nTIME UP! Auto submitting...\n");
            sleep(2);
            break;
        }
        // In danh sách câu hỏi
        for (int i = 0; i < totalQ; i++) {
            char q_line[300];
            char *start = questions[i];
            char *space = strchr(start, ' ');
            if (space) start = space + 1;
            char *nl = strchr(start, '\n');
            if (nl) {
                strncpy(q_line, start, nl - start);
                q_line[nl - start] = 0;
            } else strcpy(q_line, start);
            
            printf("%d. %s [%c]\n", i+1, q_line, answers[i] == '.' ? ' ' : answers[i]);
        }
        printf("\nEnter question number (1-%d) or 's' to submit: ", totalQ);
        
        char input[32];
        fgets(input, sizeof(input), stdin); trim_input_newline(input);

        if (input[0] == 's' || input[0] == 'S') {
            printf("Submit? (y/n): ");
            fgets(input, sizeof(input), stdin);
            if (input[0] == 'y' || input[0] == 'Y') break;
            continue;
        }

        int q = atoi(input);
        if (q < 1 || q > totalQ) { printf("Invalid.\n"); sleep(1); continue; }

        // --- ĐOẠN QUAN TRỌNG: TẢI LẠI CÂU HỎI TỪ SERVER ĐỂ CẬP NHẬT ĐÁP ÁN ---
        // Gửi lệnh lấy câu hỏi lại lần nữa
        snprintf(cmd, sizeof(cmd), "GET_QUESTION %s %d", currentRoom, q-1);
        send_message(cmd);
        // Nhận nội dung mới (có chứa dòng [Your Selection: ...])
        recv_message(questions[q-1], BUFFER_SIZE);
        // --------------------------------------------------------------------

        system("clear");
        printf("%s", questions[q-1]);
        
        char ans[10];
        while (1) {
            printf("Your answer (A/B/C/D): ");
            fgets(ans, sizeof(ans), stdin); trim_input_newline(ans);
            if (strlen(ans) == 1 && strchr("ABCDabcd", ans[0])) {
                answers[q-1] = toupper(ans[0]);
                
                char updateCmd[256];
                snprintf(updateCmd, sizeof(updateCmd), "ANSWER %s %d %c", 
                         currentRoom, q-1, answers[q-1]);
                send_message(updateCmd);
                break;
            }
            printf("Invalid answer!\n");
        }
    }

    snprintf(cmd, sizeof(cmd), "SUBMIT %s %s", currentRoom, answers);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("\n%s\n", buffer);

    free(answers);
    for (int i = 0; i < totalQ; i++) free(questions[i]);
    free(questions);
    strcpy(currentRoom, "");
    inTest = 0;
}

void handle_view_results() {
    char room[100], buffer[BUFFER_SIZE];
    printf("Room name: ");
    fgets(room, sizeof(room), stdin); trim_input_newline(room);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "RESULTS %s", room);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("\n====== RESULTS: %s ======\n", room);
    printf("%s", strncmp(buffer, "SUCCESS", 7) == 0 ? buffer + 8 : buffer);
    printf("=============================\n");
}

void handle_leaderboard() {
    // Ask user which room to view leaderboard for
    char room_name[64];
    printf("\nEnter room name to view leaderboard: ");
    if (fgets(room_name, sizeof(room_name), stdin) == NULL) return;
    trim_input_newline(room_name);
    
    if (strlen(room_name) == 0) {
        printf("Cancelled.\n");
        return;
    }
    
    // Request leaderboard for this room
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "LEADERBOARD %s", room_name);
    send_message(cmd);
    
    char buffer[BUFFER_SIZE];
    if (recv_message(buffer, sizeof(buffer)) <= 0) {
        printf("No response from server\n");
        return;
    }
    
    printf("\n%s\n", buffer);
}

void handle_practice() {
    // Step 1: Get list of topics
    send_message("PRACTICE");
    char buffer[BUFFER_SIZE];
    if (recv_message(buffer, sizeof(buffer)) <= 0) {
        printf("No response from server\n");
        return;
    }
    
    if (strncmp(buffer, "TOPICS ", 7) != 0) {
        printf("%s\n", buffer);
        return;
    }
    
    // Parse topics: "TOPICS topic1:count|topic2:count|..."
    const char *topics_str = buffer + 7;
    printf("\n========== PRACTICE MODE ==========\n");
    printf("Available topics:\n");
    
    // Display topics nicely
    char topics_copy[BUFFER_SIZE];
    strcpy(topics_copy, topics_str);
    
    int topic_num = 1;
    char *token = strtok(topics_copy, "|");
    while (token) {
        printf("  %d. %s\n", topic_num, token);
        topic_num++;
        token = strtok(NULL, "|");
    }
    
    // Step 2: Loop practice until user quits
    while (1) {
        printf("\nEnter topic name (or 'quit' to exit): ");
        char topic_name[64];
        fgets(topic_name, sizeof(topic_name), stdin);
        trim_input_newline(topic_name);
        
        if (strcasecmp(topic_name, "quit") == 0 || strlen(topic_name) == 0) {
            printf("Practice mode ended.\n");
            break;
        }
        
        // Step 3: Request question for this topic
        char request[256];
        snprintf(request, sizeof(request), "PRACTICE %s", topic_name);
        send_message(request);
        
        if (recv_message(buffer, sizeof(buffer)) <= 0) {
            printf("No response from server\n");
            break;
        }
        
        // Check if question returned or error
        if (strncmp(buffer, "FAIL", 4) == 0) {
            printf("❌ %s\n", buffer);
            continue;
        }
        
        // Parse question: "PRACTICE_Q id|text|optA|optB|optC|optD|correct|topic"
        if (strncmp(buffer, "PRACTICE_Q ", 11) != 0) {
            printf("Invalid response: %s\n", buffer);
            continue;
        }
        
        // Parse pipe-separated fields
        char question_data[BUFFER_SIZE];
        strcpy(question_data, buffer + 11);
        
        char *fields[8];
        int field_count = 0;
        char *p = strtok(question_data, "|");
        while (p && field_count < 8) {
            fields[field_count++] = p;
            p = strtok(NULL, "|");
        }
        
        if (field_count < 8) {
            printf("Invalid question data\n");
            continue;
        }
        
        // Extract fields
        // int q_id = atoi(fields[0]);
        char *q_text = fields[1];
        char *opt_a = fields[2];
        char *opt_b = fields[3];
        char *opt_c = fields[4];
        char *opt_d = fields[5];
        char correct_ans = toupper(fields[6][0]);
        // char *q_topic = fields[7];
        
        // Step 4: Display question
        printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("Question: %s\n\n", q_text);
        printf("A) %s\n", opt_a);
        printf("B) %s\n", opt_b);
        printf("C) %s\n", opt_c);
        printf("D) %s\n", opt_d);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        
        // Step 5: Get user answer
        char user_answer[10];
        while (1) {
            printf("Your answer (A/B/C/D): ");
            fgets(user_answer, sizeof(user_answer), stdin);
            trim_input_newline(user_answer);
            
            if (strlen(user_answer) == 1 && strchr("ABCDabcd", user_answer[0])) {
                user_answer[0] = toupper(user_answer[0]);
                break;
            }
            printf("❌ Invalid input! Please enter A, B, C, or D\n");
        }
        
        // Step 6: Send answer to server
        char answer_cmd[256];
        snprintf(answer_cmd, sizeof(answer_cmd), "ANSWER %c", user_answer[0]);
        send_message(answer_cmd);
        // Step 7: Get result
        memset(buffer, 0, sizeof(buffer));
        if (recv_message(buffer, sizeof(buffer)) <= 0) {
            printf("No response from server\n");
            break;
        }
        
        // Trim any newlines
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        printf("\n");
        if (strncmp(buffer, "CORRECT", 7) == 0) {
            printf("CORRECT! The answer is: %c\n", correct_ans);
        } else if (strncmp(buffer, "WRONG", 5) == 0) {
            // Parse: "WRONG|C" format
            char correct_from_server = correct_ans;
            if (strchr(buffer, '|')) {
                char *pipe = strchr(buffer, '|');
                correct_from_server = pipe[1];
            }
            printf("WRONG! The correct answer is: %c\n", correct_from_server);
        } else {
            printf("Response: %s\n", buffer);
        }
    }
}

void handle_add_question() {
    if (!loggedIn || strcmp(currentRole, "admin") != 0) {
        printf("Only admin can add questions.\n");
        return;
    }
    
    printf("\n====== ADD NEW QUESTION ======\n");
    
    char text[256], A[128], B[128], C[128], D[128];
    char correct_str[2], topic[64], difficulty[32];
    
    printf("Question text (min 10 chars): ");
    fgets(text, sizeof(text), stdin);
    trim_input_newline(text);
    
    printf("Option A: ");
    fgets(A, sizeof(A), stdin);
    trim_input_newline(A);
    
    printf("Option B: ");
    fgets(B, sizeof(B), stdin);
    trim_input_newline(B);
    
    printf("Option C: ");
    fgets(C, sizeof(C), stdin);
    trim_input_newline(C);
    
    printf("Option D: ");
    fgets(D, sizeof(D), stdin);
    trim_input_newline(D);
    
    printf("Correct answer (A/B/C/D): ");
    fgets(correct_str, sizeof(correct_str), stdin);
    trim_input_newline(correct_str);
    clear_stdin();  // 🔧 Clear input buffer to prevent empty topic error
    
    // Get topic counts from server
    char buffer[BUFFER_SIZE];
    send_message("GET_TOPICS");
    recv_message(buffer, sizeof(buffer));
    
    printf("\n====== SELECT TOPIC ======\n");
    printf("Current topics:\n");
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        char *topicData = buffer + 8;
        printf("%s\n", topicData);
    } else {
        printf("Programming(0)\nGeography(0)\nMath(0)\nArt(0)\n");
    }
    
    // Topic is required - loop until user enters something
    while (1) {
        printf("Topic name (create new or select existing - REQUIRED): ");
        fgets(topic, sizeof(topic), stdin);
        trim_input_newline(topic);
        
        if (strlen(topic) > 0) {
            break;
        }
        printf("ERROR: Topic cannot be empty. Please enter a topic.\n");
    }
    
    // Get difficulty counts from server
    send_message("GET_DIFFICULTIES");
    recv_message(buffer, sizeof(buffer));
    
    printf("\n====== SELECT DIFFICULTY ======\n");
    printf("Current difficulties:\n");
    if (strncmp(buffer, "SUCCESS", 7) == 0) {
        char *diffData = buffer + 8;
        printf("%s\n", diffData);
    } else {
        printf("Easy(0)\nMedium(0)\nHard(0)\n");
    }
    
    // Difficulty is required - loop until user enters valid value
    while (1) {
        printf("Difficulty (easy/medium/hard - REQUIRED): ");
        fgets(difficulty, sizeof(difficulty), stdin);
        trim_input_newline(difficulty);
        
        if (strlen(difficulty) > 0) {
            // Convert to lowercase for validation
            char diff_lower[32];
            strcpy(diff_lower, difficulty);
            to_lowercase_client(diff_lower);
            
            if (strcmp(diff_lower, "easy") == 0 || 
                strcmp(diff_lower, "medium") == 0 || 
                strcmp(diff_lower, "hard") == 0) {
                strcpy(difficulty, diff_lower);
                break;
            }
        }
        printf("ERROR: Difficulty must be 'easy', 'medium', or 'hard'.\n");
    }
    
    // Show review
    printf("\n====== REVIEW QUESTION ======\n");
    printf("Question: %s\n", text);
    printf("A) %s\n", A);
    printf("B) %s\n", B);
    printf("C) %s\n", C);
    printf("D) %s\n", D);
    printf("Correct: %s\n", correct_str);
    printf("Topic: %s\n", topic);
    printf("Difficulty: %s\n", difficulty);
    printf("==============================\n");
    
    printf("Add this question? (y/n): ");
    char confirm[10];
    fgets(confirm, sizeof(confirm), stdin);
    
    if (confirm[0] != 'y' && confirm[0] != 'Y') {
        printf("Cancelled.\n");
        return;
    }
    
    // Send to server
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), 
        "ADD_QUESTION %s|%s|%s|%s|%s|%s|%s|%s",
        text, A, B, C, D, correct_str, topic, difficulty);
    
    send_message(cmd);
    
    recv_message(buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void handle_delete_question() {
    if (!loggedIn || strcmp(currentRole, "admin") != 0) {
        printf("Only admin can delete questions.\n");
        return;
    }
    
    printf("\n====== DELETE QUESTION ======\n");
    printf("Search by:\n");
    printf("1. Question ID\n");
    printf("2. Topic\n");
    printf("3. Difficulty\n");
    printf("0. Cancel\n");
    printf("Select option: ");
    
    char choice[10];
    fgets(choice, sizeof(choice), stdin);
    trim_input_newline(choice);
    
    char filter_type[32], search_value[256];
    char buffer[BUFFER_SIZE];
    
    if (strcmp(choice, "1") == 0) {
        printf("Enter Question ID: ");
        fgets(search_value, sizeof(search_value), stdin);
        trim_input_newline(search_value);
        strcpy(filter_type, "id");
        
        // Search for the question
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "SEARCH_QUESTIONS %s %s", filter_type, search_value);
        send_message(cmd);
        recv_message(buffer, sizeof(buffer));
        
        if (strncmp(buffer, "SUCCESS", 7) != 0) {
            printf("%s\n", buffer);
            return;
        }
        
        // Parse and display the found question
        char *data = buffer + 8;
        printf("\n====== FOUND QUESTION ======\n");
        printf("%s\n", data);
        printf("==============================\n");
        
        printf("Delete this question? (y/n): ");
        fgets(choice, sizeof(choice), stdin);
        
        if (choice[0] != 'y' && choice[0] != 'Y') {
            printf("Cancelled.\n");
            return;
        }
        
        // Send delete command
        snprintf(cmd, sizeof(cmd), "DELETE_QUESTION %s", search_value);
        send_message(cmd);
        recv_message(buffer, sizeof(buffer));
        printf("%s\n", buffer);
        
    } else if (strcmp(choice, "2") == 0) {
        printf("Enter Topic name: ");
        fgets(search_value, sizeof(search_value), stdin);
        trim_input_newline(search_value);
        strcpy(filter_type, "topic");
        
        // Search for questions by topic
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "SEARCH_QUESTIONS %s %s", filter_type, search_value);
        send_message(cmd);
        recv_message(buffer, sizeof(buffer));
        
        if (strncmp(buffer, "SUCCESS", 7) != 0) {
            printf("%s\n", buffer);
            return;
        }
        
        // Display search results
        printf("\n====== QUESTIONS FOUND ======\n");
        char *data = buffer + 8;
        printf("%s\n", data);
        printf("==============================\n");
        
        printf("Enter Question ID to delete: ");
        char id_str[32];
        fgets(id_str, sizeof(id_str), stdin);
        trim_input_newline(id_str);
        
        printf("Delete question ID %s? (y/n): ", id_str);
        fgets(choice, sizeof(choice), stdin);
        
        if (choice[0] != 'y' && choice[0] != 'Y') {
            printf("Cancelled.\n");
            return;
        }
        
        // Send delete command
        snprintf(cmd, sizeof(cmd), "DELETE_QUESTION %s", id_str);
        send_message(cmd);
        recv_message(buffer, sizeof(buffer));
        printf("%s\n", buffer);
        
    } else if (strcmp(choice, "3") == 0) {
        printf("Enter Difficulty (easy/medium/hard): ");
        fgets(search_value, sizeof(search_value), stdin);
        trim_input_newline(search_value);
        
        // Convert to lowercase
        to_lowercase_client(search_value);
        strcpy(filter_type, "difficulty");
        
        // Search for questions by difficulty
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "SEARCH_QUESTIONS %s %s", filter_type, search_value);
        send_message(cmd);
        recv_message(buffer, sizeof(buffer));
        
        if (strncmp(buffer, "SUCCESS", 7) != 0) {
            printf("%s\n", buffer);
            return;
        }
        
        // Display search results
        printf("\n====== QUESTIONS FOUND ======\n");
        char *data = buffer + 8;
        printf("%s\n", data);
        printf("==============================\n");
        
        printf("Enter Question ID to delete: ");
        char id_str[32];
        fgets(id_str, sizeof(id_str), stdin);
        trim_input_newline(id_str);
        
        printf("Delete question ID %s? (y/n): ", id_str);
        fgets(choice, sizeof(choice), stdin);
        
        if (choice[0] != 'y' && choice[0] != 'Y') {
            printf("Cancelled.\n");
            return;
        }
        
        // Send delete command
        snprintf(cmd, sizeof(cmd), "DELETE_QUESTION %s", id_str);
        send_message(cmd);
        recv_message(buffer, sizeof(buffer));
        printf("%s\n", buffer);
        
    } else if (strcmp(choice, "0") == 0) {
        printf("Cancelled.\n");
    } else {
        printf("Invalid option.\n");
    }
}

int main(int argc, char *argv[]) {
    // Use command-line argument for SERVER_IP, or default to "127.0.0.1"
    const char *server_ip = "127.0.0.1";
    if (argc > 1) {
        server_ip = argv[1];
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = { .sin_family = AF_INET, .sin_port = htons(SERVER_PORT) };
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        return 1;
    }
    connected = 1;
    printf("Connected! Press Enter...");
    getchar();

    while (connected) {
        print_banner();
        char choice[10];
        fgets(choice, sizeof(choice), stdin); trim_input_newline(choice);

        if (!loggedIn) {
            if (strcmp(choice, "1") == 0) handle_login();
            else if (strcmp(choice, "2") == 0) handle_register();
            else if (strcmp(choice, "0") == 0) break;
        } else if (strcmp(currentRole, "admin") == 0) {
            if (strcmp(choice, "3") == 0) handle_create_room();
            else if (strcmp(choice, "4") == 0) handle_list_rooms();
            else if (strcmp(choice, "5") == 0) handle_preview_room();
            else if (strcmp(choice, "6") == 0) handle_delete_room();
            else if (strcmp(choice, "7") == 0) handle_join_room();
            else if (strcmp(choice, "8") == 0) handle_view_results();
            else if (strcmp(choice, "9") == 0) handle_leaderboard();
            else if (strcmp(choice, "10") == 0) handle_practice();
            else if (strcmp(choice, "11") == 0) handle_add_question();
            else if (strcmp(choice, "12") == 0) handle_delete_question();
            else if (strcmp(choice, "0") == 0) break;
        } else {
            if (strcmp(choice, "3") == 0) handle_list_rooms();
            else if (strcmp(choice, "4") == 0) handle_join_room();
            else if (strcmp(choice, "5") == 0) handle_view_results();
            else if (strcmp(choice, "6") == 0) handle_leaderboard();
            else if (strcmp(choice, "7") == 0) handle_practice();
            else if (strcmp(choice, "0") == 0) break;
        }

        if (connected && !inTest) {
            printf("\nPress Enter...");
            getchar();
        }
    }

    if (loggedIn) send_message("EXIT");
    close(sockfd);
    return 0;
}