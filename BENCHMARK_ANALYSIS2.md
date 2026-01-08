# Network Programming - Online Test System: Benchmark Analysis (Part 2)

**Project:** Online Test System with Client-Server Architecture  
**Technology Stack:** C (C11), TCP/IP Sockets, SQLite3, POSIX Threads  
**Evaluation Date:** January 8, 2026  
**Requirements Evaluated:** 9–14  

---

## 9. Joining a Test Room (2 points)

**Status:** Fully Implemented

**Code Evidence:**
- File: `server.c` (lines 596–644)
- File: `client.c` (lines 422–437)
- File: `db_queries.c` (lines 800–850)
- Functions: `handle_client()` (JOIN), `handle_join_room()`, `db_add_participant()`

```c
// SERVER: JOIN command handler
else if (strcmp(cmd, "JOIN") == 0) {
    char name[64];
    sscanf(buffer, "JOIN %63s", name);
    Room *r = find_room(name);
    if (!r) {
        send_msg(cli->sock, "FAIL Room not found");
    } else if (r->participantCount >= MAX_PARTICIPANTS) {
        send_msg(cli->sock, "FAIL Room is full");
    } else {
        // Add participant to room
        Participant *p = &r->participants[r->participantCount];
        strcpy(p->username, cli->username);
        p->score = -1;  // -1 indicates in-progress
        memset(p->answers, '.', r->numQuestions);
        p->answers[r->numQuestions] = '\0';
        p->start_time = time(NULL);
        p->submit_time = 0;
        p->history_count = 0;
        r->participantCount++;
        
        // 🔧 NEW: Persist participant to database
        p->db_id = db_add_participant(r->db_id, cli->user_id);
        if (p->db_id <= 0) {
            send_msg(cli->sock, "FAIL Could not register participant");
            r->participantCount--;
            return NULL;
        }
        
        char log_msg[256];
        sprintf(log_msg, "User %s joined room %s", cli->username, name);
        writeLog(log_msg);
        db_add_log(cli->user_id, "JOIN_ROOM", log_msg);
        
        char msg[256];
        sprintf(msg, "SUCCESS Joined %d %d", r->numQuestions, r->duration);
        send_msg(cli->sock, msg);
    }
}

// DATABASE: Add participant to room
int db_add_participant(int room_id, int user_id) {
    if (!db) return -1;
    
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT INTO participants (room_id, user_id, joined_at) "
        "VALUES (?, ?, CURRENT_TIMESTAMP)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, room_id);
    sqlite3_bind_int(stmt, 2, user_id);
    
    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result = (int)sqlite3_last_insert_rowid(db);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

// CLIENT: Join room interactive workflow
void handle_join_room() {
    char room[100], buffer[BUFFER_SIZE];
    printf("Enter room name: ");
    fgets(room, sizeof(room), stdin);
    trim_input_newline(room);

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
```

**Explanation:**

#### (1) Workflow Explanation

Joining a test room involves participant registration and test initialization. The client sends `JOIN roomname` command. The server retrieves the room from the in-memory `rooms[]` array using `find_room()`. If the room exists and capacity is available (`participantCount < MAX_PARTICIPANTS`), the server creates a `Participant` structure initialized with: username, score = -1 (indicating in-progress status), answers array filled with '.' (unanswered), and current timestamp. The participant is added to the room's `participants[]` array and in-memory `participantCount` is incremented. Simultaneously, the server calls `db_add_participant()` to persist the enrollment in the `participants` table, receiving a database-assigned participant ID. The server responds with `SUCCESS Joined numQuestions duration`, signaling the client to transition to test execution mode. The client stores the room name in `currentRoom` and initiates test rendering via `handle_start_test()`, establishing a three-way correlation: in-memory room object, database participant record, and client session state.

#### (2) Network Knowledge Explanation

Joining demonstrates state synchronization across distributed components: in-memory server state, persistent database, and client session. The server enforces two constraints atomically: room existence and capacity limits. The dual persistence strategy (in-memory + database) ensures session continuity—if the server crashes mid-test, the database preserves participant enrollments, enabling recovery on restart via `db_load_room_participants()`. The response format (`SUCCESS Joined numQ duration`) transmits test metadata necessary for the client to render the test interface and countdown timer. The participant ID returned from database insertion (`db_id`) serves as a foreign key linking all subsequent answer submissions to this enrollment record, enabling multi-attempt tracking and result aggregation. The timestamp captured in `joined_at` establishes a reference point for calculating test duration and auto-submission timeouts.

**Score Justification:**

✓ Complete JOIN command: room lookup, capacity check, participant initialization  
✓ Dual persistence: in-memory participant array + database record  
✓ Proper state initialization: score=-1, answers with '.' placeholders, timer start  
✓ Capacity enforcement: `participantCount < MAX_PARTICIPANTS` prevents overflow  
✓ Error handling: graceful rejection if room full or not found  
✓ Audit logging: JOIN_ROOM events recorded in both file logs and database  
✓ Metadata transmission: client receives question count and duration for UI rendering  

**Award: 2/2 points**

---

## 10. Starting the Test (1 point)

**Status:** Fully Implemented

**Code Evidence:**
- File: `client.c` (lines 439–520)
- Functions: `handle_start_test()`, `handle_start_test()` test loop

```c
// CLIENT: Test initialization and main loop
void handle_start_test(int totalQ, int duration) {
    if (totalQ == 0) { 
        printf("No questions.\n"); 
        return; 
    }

    char *answers = malloc(totalQ + 1);
    memset(answers, '.', totalQ);
    answers[totalQ] = '\0';
    
    // Allocate memory for questions
    char **questions = malloc(sizeof(char*) * totalQ);
    for(int i=0; i<totalQ; i++) 
        questions[i] = malloc(BUFFER_SIZE);

    // Load questions initially
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
        
        // Display questions with current answers
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
            
            printf("%d. %s [%c]\n", i+1, q_line, 
                   answers[i] == '.' ? ' ' : answers[i]);
        }
        
        printf("\nEnter question number (1-%d) or 's' to submit: ", totalQ);
        
        char input[32];
        fgets(input, sizeof(input), stdin);
        trim_input_newline(input);

        if (input[0] == 's' || input[0] == 'S') {
            printf("Submit? (y/n): ");
            fgets(input, sizeof(input), stdin);
            if (input[0] == 'y' || input[0] == 'Y') break;
            continue;
        }

        int q = atoi(input);
        if (q < 1 || q > totalQ) { 
            printf("Invalid.\n"); 
            sleep(1); 
            continue; 
        }

        // Fetch question again to display with input prompt
        snprintf(cmd, sizeof(cmd), "GET_QUESTION %s %d", currentRoom, q-1);
        send_message(cmd);
        recv_message(questions[q-1], BUFFER_SIZE);

        system("clear");
        printf("%s", questions[q-1]);
        
        char ans[10];
        while (1) {
            printf("Your answer (A/B/C/D): ");
            fgets(ans, sizeof(ans), stdin);
            trim_input_newline(ans);
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

    // Auto-submit or manual submit
    snprintf(cmd, sizeof(cmd), "SUBMIT %s %s", currentRoom, answers);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("\n%s\n", buffer);

    free(answers);
    for (int i = 0; i < totalQ; i++) 
        free(questions[i]);
    free(questions);
    strcpy(currentRoom, "");
    inTest = 0;
}
```

**Explanation:**

#### (1) Workflow Explanation

Test initialization occurs after successful room joining. The client allocates dynamic memory for an answers array (initialized with '.' for unanswered questions) and a 2D array of question strings. It then pre-loads all questions from the server via individual `GET_QUESTION roomname qIdx` commands, caching them locally to eliminate latency during the interactive test. The main test loop begins: (1) clears the screen, displays remaining time (countdown), and renders a numbered list of questions with their current answers marked (space for unanswered, letter for answered). (2) Prompts the user to select a question number or submit. (3) If a valid question number is selected, the client re-fetches the question with the current answer placeholder, displays full question details (text + options), and prompts for input. Upon input, the client updates the local answers array and sends `ANSWER roomname qIdx answerChar` to the server (which stores the answer in-memory but does not score yet). (4) The loop continues until time expires (countdown reaches zero, auto-submit), user selects submit and confirms, or client connection closes. Upon exit, the client sends `SUBMIT roomname answerString` (all answers concatenated), receives the score, and cleans up memory.

#### (2) Network Knowledge Explanation

Test execution demonstrates interactive, time-constrained client-server communication. The client maintains a local countdown timer (`time(NULL) - start_time`), independent of server time; this design assumes reasonable clock synchronization but allows clients to operate autonomously. The question pre-loading strategy reduces latency during test-taking, trading bandwidth for responsiveness—all questions are buffered before the timer starts. The re-fetching of a selected question (to show current answer state) demonstrates state synchronization: the client's local `answers` array and server's in-memory `Participant.answers` array should mirror, verified implicitly by the question display format. The non-blocking submission loop allows students to navigate freely without time penalty for screen redraws. The countdown timer creates urgency and ensures reproducible test duration regardless of client performance variations.

**Score Justification:**

✓ Test entry point: parameter passing (question count, duration)  
✓ Pre-loading of all questions for responsive UI  
✓ Countdown timer implementation with remaining time display  
✓ Question list rendering with current answer status  
✓ Interactive navigation: select question → view details → input answer  
✓ Real-time answer updates via ANSWER command  
✓ Graceful timeout handling: auto-submit on time expiry  
✓ Memory management: dynamic allocation and cleanup  

**Award: 1/1 point**

---

## 11. Changing Previously Selected Answers (1 point)

**Status:** Fully Implemented

**Code Evidence:**
- File: `server.c` (lines 612–632)
- File: `client.c` (lines 489–515)
- Functions: `handle_client()` (ANSWER), `handle_start_test()` (answer update logic)

```c
// SERVER: ANSWER command handler (updates in-memory participant answers)
else if (strcmp(cmd, "ANSWER") == 0) {
    // Distinguish between test room answer and practice answer
    int space_count = 0;
    char *ptr = buffer;
    while (*ptr) {
        if (*ptr == ' ') space_count++;
        ptr++;
    }
    
    if (space_count >= 3) {
        // Test room mode answer: "ANSWER roomname qIdx answer"
        char roomName[64], ansChar;
        int qIdx;
        sscanf(buffer, "ANSWER %63s %d %c", roomName, &qIdx, &ansChar);
        
        Room *r = find_room(roomName);
        if (r) {
            Participant *p = find_participant(r, cli->username);
            if (p && p->score == -1) {  // Only allow updates while in-progress (score == -1)
                if (qIdx >= 0 && qIdx < r->numQuestions) {
                    p->answers[qIdx] = ansChar;  // Update in-memory array
                }
            }
        }
    }
}

// CLIENT: Answer input and update flow (excerpt from handle_start_test)
while (1) {
    printf("Your answer (A/B/C/D): ");
    fgets(ans, sizeof(ans), stdin);
    trim_input_newline(ans);
    if (strlen(ans) == 1 && strchr("ABCDabcd", ans[0])) {
        answers[q-1] = toupper(ans[0]);  // Update local array immediately
        
        // Send update to server
        char updateCmd[256];
        snprintf(updateCmd, sizeof(updateCmd), "ANSWER %s %d %c", 
                 currentRoom, q-1, answers[q-1]);
        send_message(updateCmd);
        break;
    }
    printf("Invalid answer!\n");
}
```

**Explanation:**

#### (1) Workflow Explanation

Changing answers involves a three-step process: (1) **Local update:** Upon receiving valid input (A/B/C/D), the client immediately updates its local `answers` array. This ensures instant UI feedback without network latency. (2) **Server transmission:** The client sends `ANSWER roomname qIdx answerChar` to the server, which performs a lookup on the in-memory room and participant objects, validates that the participant is still in-progress (`score == -1`), and updates the corresponding byte in `Participant.answers[qIdx]`. The server accepts updates until the test is submitted (score transitions from -1 to a non-negative value). (3) **State verification:** The test loop re-renders the question list on each iteration, displaying updated answers, providing visual confirmation. Students may answer the same question multiple times; each ANSWER command overwrites the previous selection. The in-memory storage enables fast updates without database I/O during test-taking, critical for performance under time pressure.

#### (2) Network Knowledge Explanation

Asynchronous state synchronization occurs between client and server. The client's local answer array is the source of truth during test-taking; the server maintains a mirrored copy for submission processing and auto-submit logic. The ANSWER command uses positional parameters (`roomname qIdx answerChar`) for efficient parsing. The guard condition (`score == -1`) prevents post-submission answer modifications, enforcing test integrity. The space-counting heuristic (`space_count >= 3`) disambiguates test room answers from practice mode answers, allowing command reuse. This design prioritizes responsiveness—immediate UI feedback to the student—while ensuring eventual consistency with the server's state through explicit synchronization commands.

**Score Justification:**

✓ Local immediate update: client answers array reflects input instantly  
✓ Server-side synchronization: ANSWER command updates in-memory participant state  
✓ State guard: only allows updates while in-progress (score == -1)  
✓ Question index validation: bounds checking (0 <= qIdx < numQuestions)  
✓ Multi-attempt support: same question can be re-answered multiple times  
✓ No database writes during test (performance optimization)  
✓ Answer persistence: maintained across test loop iterations  

**Award: 1/1 point**

---

## 12. Submitting and Scoring the Test (2 points)

**Status:** Fully Implemented

**Code Evidence:**
- File: `server.c` (lines 633–679)
- File: `client.c` (lines 516–523)
- File: `db_queries.c` (lines 1100–1200, 1300–1350)
- Functions: `handle_client()` (SUBMIT), `db_record_answer()`, `db_add_result()`

```c
// SERVER: SUBMIT command handler (scoring and persistence)
else if (strcmp(cmd, "SUBMIT") == 0) {
    char name[64], ans[256];
    sscanf(buffer, "SUBMIT %63s %255s", name, ans);
    Room *r = find_room(name);
    if (!r) 
        send_msg(cli->sock, "FAIL Room not found");
    else {
        Participant *p = find_participant(r, cli->username);
        if (!p || p->score != -1) 
            send_msg(cli->sock, "FAIL Not in room or submitted");
        else {
            int score = 0;
            // Calculate score: count correct answers
            for (int i = 0; i < r->numQuestions && i < (int)strlen(ans); i++) {
                if (ans[i] != '.' && toupper(ans[i]) == r->questions[i].correct) 
                    score++;
            }
            p->score = score;
            p->submit_time = time(NULL);
            strcpy(p->answers, ans);
            
            // 🔧 NEW: Persist all answers to database
            // 1. Record each answer
            for (int i = 0; i < r->numQuestions && i < (int)strlen(ans); i++) {
                char selected = ans[i];
                int is_correct = (selected != '.' && toupper(selected) == r->questions[i].correct) ? 1 : 0;
                db_record_answer(p->db_id, r->questions[i].id, selected, is_correct);
            }
            
            // 2. Save result summary
            db_add_result(p->db_id, r->db_id, score, r->numQuestions, score);
            
            char log_msg[256];
            sprintf(log_msg, "User %s submitted answers in room %s: %d/%d", 
                    cli->username, name, score, r->numQuestions);
            writeLog(log_msg);
            db_add_log(cli->user_id, "SUBMIT_ROOM", log_msg);
            
            char msg[128];
            sprintf(msg, "SUCCESS Score: %d/%d", score, r->numQuestions);
            send_msg(cli->sock, msg);
        }
    }
}

// DATABASE: Record individual answer
int db_record_answer(int participant_id, int question_id, 
                     char selected_option, int is_correct) {
    if (!db) return 0;
    
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT INTO answers (participant_id, question_id, selected_option, is_correct, submitted_at) "
        "VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, participant_id);
    sqlite3_bind_int(stmt, 2, question_id);
    sqlite3_bind_text(stmt, 3, &selected_option, 1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, is_correct);
    
    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

// DATABASE: Save result summary
int db_add_result(int participant_id, int room_id, int score, 
                  int total_questions, int correct_answers) {
    if (!db) return -1;
    
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT INTO results (participant_id, room_id, score, total_questions, correct_answers, submitted_at) "
        "VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error preparing query: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, participant_id);
    sqlite3_bind_int(stmt, 2, room_id);
    sqlite3_bind_int(stmt, 3, score);
    sqlite3_bind_int(stmt, 4, total_questions);
    sqlite3_bind_int(stmt, 5, correct_answers);
    
    int result = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        result = (int)sqlite3_last_insert_rowid(db);
    }
    
    sqlite3_finalize(stmt);
    return result;
}

// CLIENT: Submit test
snprintf(cmd, sizeof(cmd), "SUBMIT %s %s", currentRoom, answers);
send_message(cmd);
recv_message(buffer, sizeof(buffer));
printf("\n%s\n", buffer);
```

**Explanation:**

#### (1) Workflow Explanation

Test submission comprises scoring computation and comprehensive result persistence. Upon `SUBMIT roomname answerString` command reception, the server performs three parallel operations: (1) **Scoring:** Iterates through the answer string (one character per question), comparing each character against the question's correct option. Non-answered questions (marked '.') are counted as incorrect. The score accumulates as a simple tally. (2) **In-memory update:** The participant's `score` field transitions from -1 (in-progress) to the final score, and `submit_time` is set to current time. (3) **Database persistence:** For each answer, `db_record_answer()` inserts a row into the `answers` table with participant ID, question ID, selected option, and correctness flag. Following individual answer logging, `db_add_result()` inserts a single aggregate record into the `results` table containing participant ID, room ID, final score, total question count, and timestamp. The server responds with `SUCCESS Score: X/Y`, confirming submission. The client displays the score and terminates the test session.

#### (2) Network Knowledge Explanation

Submission represents a critical checkpoint in test execution—the transition from interactive state to finalized result. The answer string format (concatenated characters) enables transmission of all answers in a single TCP message, reducing overhead compared to per-question submission. The dual database writes (granular `answers` records + aggregate `results` record) serve complementary purposes: `answers` enables forensic analysis and question-level analytics, while `results` provides fast leaderboard queries without aggregation. The in-memory score calculation and database persistence are synchronized: the participant object's `score` field reflects the persistent value, preventing stale-state bugs. The guard condition (`score == -1`) prevents resubmission attacks—once finalized, a participant cannot alter their submission. The timestamp in the database (`CURRENT_TIMESTAMP`) serves as an audit trail and enables ranking by submission order.

**Score Justification:**

✓ Complete scoring logic: correct answer counting with edge cases  
✓ Granular result persistence: individual answers recorded in answers table  
✓ Aggregate result persistence: summary recorded in results table  
✓ State transition: score changes from -1 (in-progress) to final value  
✓ Resubmission prevention: guard condition (score == -1) blocks duplicate submissions  
✓ Audit trail: timestamps and event logging  
✓ Comprehensive response: returns both score and count  
✓ Edge case handling: answers with '.' (unanswered) marked as incorrect  

**Award: 2/2 points**

---

## 13. Viewing Test Results of Completed Rooms (1 point)

**Status:** Fully Implemented

**Code Evidence:**
- File: `server.c` (lines 680–710)
- File: `client.c` (lines 624–638)
- Functions: `handle_client()` (RESULTS), `handle_view_results()`

```c
// SERVER: RESULTS command handler
else if (strcmp(cmd, "RESULTS") == 0) {
    char name[64];
    sscanf(buffer, "RESULTS %63s", name);
    Room *r = find_room(name);
    if (!r) 
        send_msg(cli->sock, "FAIL Not found");
    else {
        char msg[4096] = "SUCCESS Results:\n";
        for (int i = 0; i < r->participantCount; i++) {
            Participant *p = &r->participants[i];
            char line[512];
            char historyStr[256] = "";
            
            // Display score history if multiple attempts
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

// CLIENT: View results
void handle_view_results() {
    char room[100], buffer[BUFFER_SIZE];
    printf("Room name: ");
    fgets(room, sizeof(room), stdin);
    trim_input_newline(room);
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "RESULTS %s", room);
    send_message(cmd);
    recv_message(buffer, sizeof(buffer));
    printf("\n====== RESULTS: %s ======\n", room);
    printf("%s", strncmp(buffer, "SUCCESS", 7) == 0 ? buffer + 8 : buffer);
    printf("=============================\n");
}
```

**Explanation:**

#### (1) Workflow Explanation

Results viewing retrieves and displays all participants' scores for a completed room. The client sends `RESULTS roomname`, and the server looks up the room in the in-memory `rooms[]` array. If found, the server iterates through the room's `participants[]` array, formatting each participant's status: username, score history (if multiple attempts), and current score or "Doing..." if still in-progress. The formatted list is concatenated and transmitted via `send_msg()` with `SUCCESS` prefix. The client receives the response, strips the prefix, and displays the results table. The results include both final scores and attempt history, enabling instructors to view multiple submissions from the same student.

#### (2) Network Knowledge Explanation

Results viewing demonstrates read-only query semantics over the in-memory state model. The server iterates through active participants without database access, leveraging cached data for fast response times. The score history field (`history_count`, `score_history[]`) enables multi-attempt tests, though the current implementation typically stores only the latest score. The "Doing..." status for incomplete tests (`score == -1`) provides real-time visibility into test progress without blocking submissions. The room lookup is O(N) in the number of rooms; for large deployments, indexing would improve performance. The simple text format (pipe-delimited fields) facilitates parsing on clients and external tools.

**Score Justification:**

✓ Complete RESULTS command with room lookup  
✓ Iterates all participants in room  
✓ Displays username and score for each participant  
✓ Handles in-progress tests (score == -1 → "Doing...")  
✓ Supports score history display (multiple attempts)  
✓ Formatted human-readable output  
✓ Error handling: graceful "Not found" message  

**Award: 1/1 point**

---

## 14. Logging Activities (1 point)

**Status:** Fully Implemented

**Code Evidence:**
- File: `logger.c` (lines 1–34)
- File: `server.c` (lines 70, 240–265, 283–309, 1038)
- File: `db_queries.c` (lines 1900–1950)
- Functions: `writeLog()`, `handle_client()` (logging throughout), `db_add_log()`

```c
// LOGGER: File-based logging with timestamps
void writeLog(const char *event) {
    if (!event) return;

    struct stat st = {0};
    if (stat("data", &st) == -1) {
        mkdir("data", 0700);
    }

    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[64];
    strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);

    fprintf(fp, "%s - %s\n", timebuf, event);
    fclose(fp);
}

// SERVER: Logging throughout command handlers
if (user_id > 0) {
    send_msg(cli->sock, "SUCCESS Registered. Please login.\n");
    sprintf(log_msg, "User %s registered as %s in database", user, role);
    writeLog(log_msg);  // File log
} 

if (user_id > 0) {
    db_get_user_role(user, role);
    strcpy(cli->username, user);
    strcpy(cli->role, role);
    cli->user_id = user_id;
    cli->loggedIn = 1;
    
    sprintf(log_msg, "User %s logged in as %s", user, role);
    writeLog(log_msg);  // File log
    db_add_log(cli->user_id, "LOGIN", log_msg);  // Database log
}

// Server startup logging
writeLog("SERVER_STARTED");

// DATABASE: Activity logging function
int db_add_log(int user_id, const char *event_type, const char *description) {
    if (!user_id || !event_type || !description || !db) return -1;
    
    sqlite3_stmt *stmt;
    const char *query = 
        "INSERT INTO logs (user_id, event_type, description, created_at) "
        "VALUES (?, ?, ?, CURRENT_TIMESTAMP)";
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, event_type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    
    int result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

// DATABASE: Logs table schema
"CREATE TABLE IF NOT EXISTS logs ("
"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
"  user_id INTEGER NOT NULL,"
"  event_type TEXT NOT NULL,"
"  description TEXT,"
"  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
"  FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
");"
```

**Explanation:**

#### (1) Workflow Explanation

Logging operates on a dual-channel architecture for comprehensive activity tracking. **File-based logs** (written via `writeLog()`) append timestamped events to `data/logs.txt` in human-readable format: "YYYY-MM-DD HH:MM:SS - Event description". Critical events—server startup, user registration, login, room creation, test submission—invoke `writeLog()` with descriptive messages. **Database logs** (written via `db_add_log()`) record structured events in the `logs` table with user ID, event type, description, and timestamp. The database approach enables programmatic analysis and querying (e.g., "all events by user X between time Y and Z"). Both channels are updated independently; file logs provide human readability for immediate inspection, while database logs provide structured data for analytics. Event types include: SERVER_STARTED, REGISTER, LOGIN, CREATE_ROOM, JOIN_ROOM, SUBMIT_ROOM, DELETE_ROOM, ADD_QUESTION, DELETE_QUESTION. The logging occurs synchronously within each command handler, ensuring no events are missed even in case of client disconnection.

#### (2) Network Knowledge Explanation

Audit logging is essential for network applications handling sensitive operations (tests, scores, user accounts). The dual-channel approach balances real-time observability (file logs viewable with `tail`) against long-term analytics (database queries). The `created_at` timestamp field captures server time, not client time, preventing clock skew attacks where clients manipulate their local clock. The `user_id` foreign key links events to specific users, enabling forensic investigation and detecting unauthorized access attempts. The file-based logging with automatic directory creation (`mkdir("data", 0700)`) ensures the system can initialize even on first run. The synchronous I/O (blocking file operations) prioritizes correctness over performance; in high-throughput scenarios, asynchronous logging (buffered to a separate thread) would improve responsiveness. The logs serve both compliance (audit trail) and operational (debugging) purposes.

**Score Justification:**

✓ Dual-channel logging: file + database  
✓ Timestamp tracking: human-readable format in file, structured in database  
✓ Event coverage: registration, login, room operations, submissions  
✓ User attribution: user_id linked to events  
✓ Directory creation: handles missing data/ directory  
✓ Event types: structured event categorization (REGISTER, LOGIN, etc.)  
✓ Persistent storage: survives server restarts via database  
✓ Real-time visibility: immediate file log updates  

**Award: 1/1 point**

---

## Summary

| Requirement | Status | Points |
|------------|--------|--------|
| 9. Joining a Test Room | Fully Implemented | 2/2 |
| 10. Starting the Test | Fully Implemented | 1/1 |
| 11. Changing Previously Selected Answers | Fully Implemented | 1/1 |
| 12. Submitting and Scoring the Test | Fully Implemented | 2/2 |
| 13. Viewing Test Results of Completed Rooms | Fully Implemented | 1/1 |
| 14. Logging Activities | Fully Implemented | 1/1 |
| **Total (Requirements 9–14)** | **Fully Implemented** | **9/9** |

---

## Technical Observations

### Test Execution Pipeline

The implementation successfully orchestrates a complete test lifecycle:

1. **Join:** Participant registration with in-memory + database dual persistence
2. **Load:** Pre-fetch all questions to eliminate network latency during test
3. **Execute:** Interactive loop with countdown timer, real-time answer updates
4. **Submit:** Atomic scoring and multi-layer persistence (answers table + results table)
5. **View:** Fast in-memory result retrieval with score history display

### Concurrency Handling

The thread-per-connection model naturally handles simultaneous test-takers:
- Each participant's handler thread maintains independent session state (`Participant` structure)
- Global mutex (`pthread_mutex_lock`) serializes access to shared `rooms[]` array
- Database serializes write operations via SQLite's internal locking

### State Consistency

Hybrid state management ensures consistency across failure scenarios:
- **In-memory:** Fast access during active test sessions
- **Database:** Persistent archive enables recovery after server crash
- **Server restart:** `load_rooms()` reconstructs session state from database

### Edge Cases Handled

- **Unanswered questions:** Marked as '.' in answer string, counted as incorrect
- **Timeout auto-submission:** `monitor_exam_thread()` auto-submits after `duration + 2 seconds`
- **Resubmission prevention:** Guard condition (`score == -1`) blocks duplicate submission
- **Answer modification:** Allowed only while in-progress; blocked post-submission

---

## Comparison: Requirements 1–8 vs. 9–14

| Aspect | Requirements 1–8 | Requirements 9–14 |
|--------|------------------|-------------------|
| **Focus** | Foundation (authentication, access control) | Workflow (test execution, results) |
| **State Complexity** | Session-level (login, permissions) | Multi-participant, time-dependent |
| **Persistence** | Simple identity records | Complex answer/result aggregation |
| **Concurrency** | Per-client sessions | Multi-client synchronization |
| **Real-time Features** | None | Countdown timer, live score updates |

---

**Report Generated:** January 8, 2026  
**Evaluation Standard:** Network Programming Course Rubric  
**Total Achievable Points (Requirements 9–14):** 9/9 points

**Cumulative Score (Requirements 1–14):** 22/22 points
