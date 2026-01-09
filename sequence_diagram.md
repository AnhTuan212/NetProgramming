# Online Test System - Sequence Diagrams for All Functions

**Purpose:** Visual guide for understanding how data flows between Client, Server, and Database

---

## Table of Contents

1. [Connection & Authentication](#connection--authentication)
2. [Test Room Management](#test-room-management)
3. [Test Execution Flow](#test-execution-flow)
4. [Results & Leaderboard](#results--leaderboard)
5. [Practice Mode](#practice-mode)
6. [Question Management](#question-management)
7. [Database Operations](#database-operations)

---

## Connection & Authentication

### 1. Client Connects to Server

```
CLIENT                           NETWORK                    SERVER
═══════════════════════════════════════════════════════════════════════════

User runs ./client
         │
         ├─ socket() creates socket
         ├─ connect(SERVER_PORT 9000)
         │
         │      ═════════════════════════════════════════
         │      TCP 3-way handshake
         │      ═════════════════════════════════════════
         │                                                   ▼
         │                                         accept() blocks
         │                                         New connection accepted
         │                                         pthread_create(handle_client)
         │
         ├─ Connected! (sockfd ready)
         │                                                   │
         │                                                   ▼
         │                                         Client *cli = malloc()
         │                                         cli->sock = socket
         │                                         cli->loggedIn = 0
         │
         ▼
Menu displayed
```

---

### 2. User Registration (REGISTER)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_register()
│
├─ Input: username, password
├─ Select role: admin or student
├─ If admin: Enter admin secret code
│
└─ REGISTER alice pass123 admin network_programming
    │
    │        ════════════════════════════════════════════════
    │        send_message("REGISTER alice pass123 admin ...")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            recv() in handle_client()
    │                                            Parse command
    │                                                        │
    │                                                        ├─ Verify admin code
    │                                                        │  "network_programming" == ADMIN_CODE?
    │                                                        │
    │                                                        ├─ YES → validate role
    │                                                        │       (admin/student)
    │                                                        │
    │                                                        └─ Call db_add_user()
    │                                                            │
    │                                                            │  ────────────────────────────────
    │                                                            │  INSERT INTO users
    │                                                            │  (username, password, role)
    │                                                            │  ────────────────────────────────
    │                                                            │                  │
    │                                                            │                  ▼
    │                                                            │          users table updated
    │                                                            │          user_id auto-increment
    │                                                            │
    │                                                            ├─ db_add_log()
    │                                                            │  INSERT INTO logs
    │                                                            │
    │                                                            └─ send_msg(SUCCESS)
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS Registered"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf("SUCCESS Registered")
Menu shown again
```

---

### 3. User Login (LOGIN)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_login()
│
├─ Input: username, password
│
└─ LOGIN alice pass123
    │
    │        ════════════════════════════════════════════════
    │        send_message("LOGIN alice pass123")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Call db_validate_user()
    │                                                        │
    │                                                        ├─ Query database
    │                                                        │  SELECT * FROM users
    │                                                        │  WHERE username='alice'
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          Found user record
    │                                                        │          Compare password
    │                                                        │
    │                                                        ├─ Password matches?
    │                                                        │  ├─ YES: Get role, user_id
    │                                                        │  └─ NO: Return error
    │                                                        │
    │                                                        ├─ Set Client struct
    │                                                        │  cli->loggedIn = 1
    │                                                        │  strcpy(cli->username, "alice")
    │                                                        │  strcpy(cli->role, role)
    │                                                        │  cli->user_id = user_id
    │                                                        │
    │                                                        ├─ db_add_log()
    │                                                        │  Login event recorded
    │                                                        │
    │                                                        └─ send_msg("SUCCESS admin")
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS admin"  (or "student")
    │    ════════════════════════════════════════════════════════
    │
    ▼
loggedIn = 1
currentUser = "alice"
currentRole = "admin"
Menu updated based on role
```

---

## Test Room Management

### 4. Admin Creates Test Room (CREATE)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_create_room()
│
├─ Input: room_name, num_questions, duration
├─ Topic distribution: "programming:5 database:5"
├─ Difficulty dist: "easy:5 medium:3 hard:2"
│
└─ CREATE exam01 10 300 programming:5 database:5 easy:5 medium:3 hard:2
    │
    │        ════════════════════════════════════════════════
    │        send_message("CREATE ...")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex (thread-safe)
    │                                                        │
    │                                                        ├─ Parse command
    │                                                        │  Extract: room_name, num_q,
    │                                                        │           duration, filters
    │                                                        │
    │                                                        ├─ Call db_get_random_filtered_questions()
    │                                                        │  Filter by topic & difficulty
    │                                                        │  ────────────────────────────────
    │                                                        │  SELECT * FROM questions
    │                                                        │  WHERE topic_id IN (1,2)
    │                                                        │  AND difficulty_id IN (1,2,3)
    │                                                        │  ORDER BY RANDOM()
    │                                                        │  LIMIT 10
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          10 random questions
    │                                                        │          returned to struct
    │                                                        │
    │                                                        ├─ Add to in-memory rooms[] struct
    │                                                        │  rooms[roomCount].name = "exam01"
    │                                                        │  rooms[roomCount].owner = "admin"
    │                                                        │  rooms[roomCount].questions[] = loaded
    │                                                        │  roomCount++
    │                                                        │
    │                                                        ├─ Call db_create_room()
    │                                                        │  ────────────────────────────────
    │                                                        │  INSERT INTO rooms
    │                                                        │  (name, owner_id, duration_minutes)
    │                                                        │  VALUES ('exam01', 1, 300)
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          room_id = auto_increment
    │                                                        │          rooms.db_id = room_id
    │                                                        │
    │                                                        ├─ For each question:
    │                                                        │  Call db_add_question_to_room()
    │                                                        │  ────────────────────────────────
    │                                                        │  INSERT INTO room_questions
    │                                                        │  (room_id, question_id, order_num)
    │                                                        │  ────────────────────────────────
    │                                                        │
    │                                                        ├─ db_add_log()
    │                                                        │  Event: ROOM_CREATED
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ send_msg("SUCCESS Room created")
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS Room created"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf("Room created!")
Menu shown
```

---

### 5. View Room List (LIST)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_list_rooms()
│
└─ LIST
    │
    │        ════════════════════════════════════════════════
    │        send_message("LIST")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ Loop through rooms[0..roomCount-1]
    │                                                        │  (READ FROM STRUCT - FAST!)
    │                                                        │
    │                                                        ├─ For each room:
    │                                                        │  Format: "- exam01 (Owner: alice, Q: 10, Time: 300s)"
    │                                                        │
    │                                                        ├─ Build response message
    │                                                        │  msg = "SUCCESS Rooms:\n"
    │                                                        │  msg += "- exam01 (...)\n"
    │                                                        │  msg += "- exam02 (...)\n"
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ send_msg(msg)
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS Rooms:\n- exam01 (...)\n- exam02 (...)\n"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf(response)
Display room list
```

---

### 6. Preview Test (PREVIEW)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_preview_room()
│
├─ Input: room_name
│
└─ PREVIEW exam01
    │
    │        ════════════════════════════════════════════════
    │        send_message("PREVIEW exam01")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ find_room("exam01")
    │                                                        │  Search in rooms[] struct
    │                                                        │
    │                                                        ├─ If found:
    │                                                        │  For each question in room:
    │                                                        │    Get: text, options A/B/C/D, correct
    │                                                        │
    │                                                        ├─ Build response:
    │                                                        │  "[1/10] Question text?"
    │                                                        │  "A) Option A  B) Option B"
    │                                                        │  "C) Option C  D) Option D"
    │                                                        │  "Correct: C"
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ send_msg(preview_text)
    │    ════════════════════════════════════════════════════════
    │    Preview text (all questions + answers)
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf(preview)
Show all questions with answers
```

---

### 7. Delete Test Room (DELETE)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_delete_room()
│
├─ Input: room_name
├─ Verify: admin only
├─ Verify: only your room
│
└─ DELETE exam01
    │
    │        ════════════════════════════════════════════════
    │        send_message("DELETE exam01")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ find_room("exam01")
    │                                                        │
    │                                                        ├─ Check: cli->username == room->owner?
    │                                                        │  ├─ NO: send error
    │                                                        │  └─ YES: continue
    │                                                        │
    │                                                        ├─ Get room db_id
    │                                                        │
    │                                                        ├─ Call db_delete_room(db_id)
    │                                                        │  ────────────────────────────────
    │                                                        │  DELETE FROM rooms
    │                                                        │  WHERE id = room_id
    │                                                        │  (cascade deletes room_questions)
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          Database updated
    │                                                        │
    │                                                        ├─ Remove from in-memory struct
    │                                                        │  (mark for deletion or shift array)
    │                                                        │
    │                                                        ├─ db_add_log()
    │                                                        │  Event: ROOM_DELETED
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ send_msg("SUCCESS Room deleted")
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS Room deleted"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf("Room deleted!")
```

---

## Test Execution Flow

### 8. Join Room (JOIN)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_join_room()
│
├─ Input: room_name
├─ Send JOIN command
│
└─ JOIN exam01
    │
    │        ════════════════════════════════════════════════
    │        send_message("JOIN exam01")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ find_room("exam01")
    │                                                        │  Search in rooms[] struct
    │                                                        │
    │                                                        ├─ Check capacity:
    │                                                        │  if (participantCount < MAX_PARTICIPANTS)
    │                                                        │
    │                                                        ├─ Add to in-memory struct:
    │                                                        │  rooms[i].participants[n].username = cli->username
    │                                                        │  rooms[i].participants[n].score = -1 (in progress)
    │                                                        │  rooms[i].participants[n].answers = "......."
    │                                                        │  rooms[i].participants[n].start_time = time(NULL)
    │                                                        │  rooms[i].participantCount++
    │                                                        │
    │                                                        ├─ Call db_add_participant()
    │                                                        │  ────────────────────────────────
    │                                                        │  INSERT INTO participants
    │                                                        │  (room_id, user_id, joined_at)
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          participant_id = auto_increment
    │                                                        │
    │                                                        ├─ Save participant_id in struct:
    │                                                        │  rooms[i].participants[n].db_id = participant_id
    │                                                        │
    │                                                        ├─ db_add_log()
    │                                                        │  Event: JOINED_ROOM
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        ├─ Calculate remaining time:
    │                                                        │  remaining = duration - (current_time - start_time)
    │                                                        │
    │                                                        └─ send_msg("SUCCESS 10 300")
    │                                                           (numQuestions, duration)
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS 10 300"
    │    ════════════════════════════════════════════════════════
    │
    ▼
currentRoom = "exam01"
inTest = 1
handle_start_test() called
```

---

### 9. Get Question During Test (GET_QUESTION)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

During handle_start_test() loop:

├─ GET_QUESTION 0
    │
    │        ════════════════════════════════════════════════
    │        send_message("GET_QUESTION 0")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ find_room(currentRoom)
    │                                                        │
    │                                                        ├─ Extract question_index = 0
    │                                                        │
    │                                                        ├─ Get from in-memory struct:
    │                                                        │  QItem q = rooms[i].questions[0]
    │                                                        │  (NO DATABASE ACCESS - FAST!)
    │                                                        │
    │                                                        ├─ Format response:
    │                                                        │  "[1/10] What is 2+2?"
    │                                                        │  "A) 1    B) 2    C) 3    D) 4"
    │                                                        │  "[Your Selection: ]"
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ send_msg(question_text)
    │    ════════════════════════════════════════════════════════
    │    "[1/10] What is 2+2?\nA) 1  B) 2  C) 3  D) 4"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf(question)
Display question
User selects answer: "A"
```

---

### 10. Record Answer During Test (ANSWER)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

User enters: A
│
├─ ANSWER 0 A
    │
    │        ════════════════════════════════════════════════
    │        send_message("ANSWER 0 A")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ find_room(currentRoom)
    │                                                        │
    │                                                        ├─ find_participant(room, username)
    │                                                        │
    │                                                        ├─ Update in-memory struct:
    │                                                        │  participant->answers[0] = 'A'
    │                                                        │  (ONLY IN STRUCT - NOT DATABASE!)
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ (NO RESPONSE - silent operation)
    │    ════════════════════════════════════════════════════════
    │    (nothing returned)
    │    ════════════════════════════════════════════════════════
    │
    ▼
Continue to next question
(Answer stored only in memory)
```

---

### 11. Submit Test (SUBMIT) - Critical Data Persistence Point

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

User finished answering all questions
│
├─ SUBMIT exam01 ADBC.D.ABCD
    │
    │        ════════════════════════════════════════════════
    │        send_message("SUBMIT exam01 ADBC.D.ABCD")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ find_room("exam01")
    │                                                        │
    │                                                        ├─ find_participant(room, username)
    │                                                        │
    │                                                        ├─ Calculate score:
    │                                                        │  score = 0
    │                                                        │  for each answer in "ADBC.D.ABCD":
    │                                                        │    if (answer == correct_answer) score++
    │                                                        │  Result: score = 6 (out of 10)
    │                                                        │
    │                                                        ├─ Update struct:
    │                                                        │  participant->score = 6
    │                                                        │  participant->submit_time = time(NULL)
    │                                                        │
    │                                                        ├─ *** START WRITING TO DATABASE ***
    │                                                        │
    │                                                        ├─ For each answer[i]:
    │                                                        │  Call db_record_answer()
    │                                                        │  ────────────────────────────────
    │                                                        │  INSERT INTO answers
    │                                                        │  (participant_id, question_id,
    │                                                        │   selected_option, is_correct)
    │                                                        │  VALUES (pid, qid, 'A', 1)
    │                                                        │  ────────────────────────────────
    │                                                        │  (Repeat for all 10 answers)
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          answers table: 10 new rows
    │                                                        │
    │                                                        ├─ Call db_add_result()
    │                                                        │  ────────────────────────────────
    │                                                        │  INSERT INTO results
    │                                                        │  (participant_id, room_id, score,
    │                                                        │   total_questions, correct_answers,
    │                                                        │   submitted_at)
    │                                                        │  VALUES (pid, rid, 6, 10, 6, NOW())
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          results table: 1 new row
    │                                                        │
    │                                                        ├─ db_add_log()
    │                                                        │  Event: TEST_SUBMITTED
    │                                                        │  Details: participant, score, room
    │                                                        │
    │                                                        ├─ *** DATA NOW PERSISTENT IN DB ***
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ send_msg("SUCCESS Score: 6/10")
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS Score: 6/10"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf("Score: 6/10")
Test completed
inTest = 0
Return to menu
```

---

## Results & Leaderboard

### 12. View Results (RESULTS)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_view_results()
│
├─ RESULTS exam01
    │
    │        ════════════════════════════════════════════════
    │        send_message("RESULTS exam01")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Lock mutex
    │                                                        │
    │                                                        ├─ find_room("exam01")
    │                                                        │
    │                                                        ├─ Build response from in-memory struct:
    │                                                        │  For each participant in room:
    │                                                        │    if (score == -1) display "Doing..."
    │                                                        │    else display: "alice: 6/10"
    │                                                        │
    │                                                        ├─ Format response:
    │                                                        │  "SUCCESS Results:"
    │                                                        │  "- alice: 6/10"
    │                                                        │  "- bob: 8/10"
    │                                                        │  "- charlie: Doing..."
    │                                                        │
    │                                                        ├─ Unlock mutex
    │                                                        │
    │                                                        └─ send_msg(results)
    │    ════════════════════════════════════════════════════════
    │    Results from struct (not database)
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf(results)
Display participant scores
```

---

### 13. View Leaderboard (LEADERBOARD)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_leaderboard()
│
├─ LEADERBOARD exam01
    │
    │        ════════════════════════════════════════════════
    │        send_message("LEADERBOARD exam01")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Call db_get_leaderboard()
    │                                                        │
    │                                                        ├─ Query database
    │                                                        │  ────────────────────────────────
    │                                                        │  SELECT u.username, r.score,
    │                                                        │         COUNT(*) as attempts,
    │                                                        │         AVG(r.score) as avg_score
    │                                                        │  FROM results r
    │                                                        │  JOIN participants p ON r.participant_id = p.id
    │                                                        │  JOIN users u ON p.user_id = u.id
    │                                                        │  WHERE r.room_id = room_id
    │                                                        │  GROUP BY u.username
    │                                                        │  ORDER BY MAX(r.score) DESC
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          Aggregated results
    │                                                        │          with rankings
    │                                                        │
    │                                                        ├─ Format leaderboard:
    │                                                        │  Rank | Username | Score | Attempts
    │                                                        │  ─────┼──────────┼───────┼──────────
    │                                                        │    1  │  alice   │ 9/10  │    2
    │                                                        │    2  │  bob     │ 8/10  │    1
    │                                                        │    3  │  charlie │ 7/10  │    3
    │                                                        │
    │                                                        └─ send_msg(leaderboard)
    │    ════════════════════════════════════════════════════════
    │    Leaderboard from DATABASE (not struct)
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf(leaderboard)
Display rankings
```

---

## Practice Mode

### 14. Practice Mode Flow (PRACTICE)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_practice()
│
├─ No room selected (practice mode is independent)
│
├─ Ask: Select topic (or all)
│
├─ PRACTICE programming
    │
    │        ════════════════════════════════════════════════
    │        send_message("PRACTICE programming")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Call db_get_random_question_by_topic()
    │                                                        │
    │                                                        ├─ Query database
    │                                                        │  ────────────────────────────────
    │                                                        │  SELECT * FROM questions
    │                                                        │  WHERE topic_id = (SELECT id FROM topics
    │                                                        │                    WHERE name = 'programming')
    │                                                        │  ORDER BY RANDOM()
    │                                                        │  LIMIT 1
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          Random question from DB
    │                                                        │
    │                                                        ├─ Store in Client struct:
    │                                                        │  cli->current_question_id = q_id
    │                                                        │  cli->current_question_correct = 'A'
    │                                                        │
    │                                                        ├─ Format response:
    │                                                        │  "PRACTICE_Q 5|Question text?|OptA|OptB|OptC|OptD|A|programming"
    │                                                        │  (NO correct answer in response!)
    │                                                        │
    │                                                        └─ send_msg(question)
    │    ════════════════════════════════════════════════════════
    │    Practice question (correct answer hidden)
    │    ════════════════════════════════════════════════════════
    │
    ▼
Display question without answer
User selects: "A"
│
├─ ANSWER A
    │
    │        ════════════════════════════════════════════════
    │        send_message("ANSWER A")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                                        │
    │                                                        ├─ Compare: 'A' == cli->current_question_correct?
    │                                                        │
    │                                                        ├─ If CORRECT:
    │                                                        │  └─ send_msg("CORRECT")
    │                                                        │
    │                                                        ├─ If WRONG:
    │                                                        │  └─ send_msg("WRONG Correct answer: B")
    │                                                        │
    │                                                        ├─ db_add_log() - optional practice logging
    │                                                        │
    │                                                        └─ (NOT saved to results - just feedback)
    │    ════════════════════════════════════════════════════════
    │    "CORRECT" or "WRONG Correct: B"
    │    ════════════════════════════════════════════════════════
    │
    ▼
Display feedback
Loop: Get next practice question (repeat from PRACTICE step)
```

---

## Question Management

### 15. Add Questions (ADD_QUESTION)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_add_question()
│
├─ Admin only
├─ Input question details:
│  - Text: "What is Python?"
│  - Options: A, B, C, D
│  - Correct: A
│  - Topic: "programming"
│  - Difficulty: "easy"
│
├─ ADD_QUESTION What is Python?|opt_a|opt_b|opt_c|opt_d|A|programming|easy
    │
    │        ════════════════════════════════════════════════
    │        send_message("ADD_QUESTION ...")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Call db_add_question()
    │                                                        │
    │                                                        ├─ Verify topic exists:
    │                                                        │  SELECT id FROM topics WHERE name = 'programming'
    │                                                        │  If not exists: create topic
    │                                                        │
    │                                                        ├─ Verify difficulty exists:
    │                                                        │  SELECT id FROM difficulties WHERE name = 'easy'
    │                                                        │  (Should exist - defaults created)
    │                                                        │
    │                                                        ├─ Insert question:
    │                                                        │  ────────────────────────────────
    │                                                        │  INSERT INTO questions
    │                                                        │  (text, option_a, option_b, option_c, option_d,
    │                                                        │   correct_option, topic_id, difficulty_id,
    │                                                        │   created_by, created_at)
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          question_id = auto_increment
    │                                                        │          Question now available in DB
    │                                                        │
    │                                                        ├─ db_add_log()
    │                                                        │  Event: QUESTION_ADDED
    │                                                        │
    │                                                        └─ send_msg("SUCCESS Question added ID: 42")
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS Question added ID: 42"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf("Question added!")
Now available for room creation
```

---

### 16. Delete Questions (DELETE_QUESTION)

```
CLIENT                              SERVER                      DATABASE
═══════════════════════════════════════════════════════════════════════════

handle_delete_question()
│
├─ Admin only
├─ Input: question_id
│
├─ DELETE_QUESTION 42
    │
    │        ════════════════════════════════════════════════
    │        send_message("DELETE_QUESTION 42")
    │        ════════════════════════════════════════════════
    │                                                        ▼
    │                                            handle_client() receives
    │                                            Call db_delete_question()
    │                                                        │
    │                                                        ├─ Query database:
    │                                                        │  SELECT * FROM questions WHERE id = 42
    │                                                        │  (Verify exists)
    │                                                        │
    │                                                        ├─ Delete from database:
    │                                                        │  ────────────────────────────────
    │                                                        │  DELETE FROM questions WHERE id = 42
    │                                                        │  (Cascade deletes from room_questions
    │                                                        │   if in any room)
    │                                                        │  ────────────────────────────────
    │                                                        │                  │
    │                                                        │                  ▼
    │                                                        │          Question removed from DB
    │                                                        │          Not available for new rooms
    │                                                        │
    │                                                        ├─ db_add_log()
    │                                                        │  Event: QUESTION_DELETED
    │                                                        │
    │                                                        └─ send_msg("SUCCESS Question deleted")
    │    ════════════════════════════════════════════════════════
    │    "SUCCESS Question deleted"
    │    ════════════════════════════════════════════════════════
    │
    ▼
printf("Question deleted!")
```

---

## Database Operations

### 17. Background Monitor Thread (Auto-Submit)

```
SERVER THREAD 1                MONITOR THREAD              DATABASE
═══════════════════════════════════════════════════════════════════════════

Main server running
│
├─ pthread_create(monitor_exam_thread)
│
└─ Monitor thread runs every 1 second:
    │
    ├─ while (1) {
    │     sleep(1);
    │     pthread_mutex_lock(&lock);
    │
    │     For each room in rooms[]:
    │       For each participant:
    │         if (participant->score == -1) {  // In progress
    │           elapsed = difftime(now, start_time)
    │
    │           if (elapsed >= duration + 2) {  // Timeout!
    │             ├─ Calculate score from answers[]
    │             │
    │             ├─ participant->score = calculated
    │
    │             ├─ Call db_record_answer() for each answer
    │             │  ────────────────────────────────
    │             │  INSERT INTO answers
    │             │  ────────────────────────────────
    │             │                  │
    │             │                  ▼
    │             │          Database updated
    │             │
    │             ├─ Call db_add_result()
    │             │  ────────────────────────────────
    │             │  INSERT INTO results
    │             │  ────────────────────────────────
    │             │                  │
    │             │                  ▼
    │             │          Result persisted
    │             │
    │             └─ db_add_log()
    │                Event: AUTO_SUBMITTED
    │           }
    │         }
    │
    │     pthread_mutex_unlock(&lock);
    │   }
    │
    └─ Continuous monitoring...

(Ensures no test is left hanging)
```

---

### 18. Server Startup - Load Rooms from Database

```
SERVER STARTUP
═══════════════════════════════════════════════════════════════════════════

main()
│
├─ db_init("test_system.db")
│  ├─ sqlite3_open()
│  ├─ db_create_tables() if not exists
│  └─ db_init_default_difficulties()
│
├─ load_rooms()
│  │
│  ├─ Call db_load_all_rooms()
│  │  ────────────────────────────────
│  │  SELECT * FROM rooms
│  │  WHERE is_finished = 0
│  │  ────────────────────────────────
│  │                  │
│  │                  ▼
│  │          Get all active rooms from DB
│  │
│  ├─ For each room:
│  │  ├─ Create in-memory Room struct
│  │  ├─ Get room owner name from users table
│  │  ├─ Load questions from room_questions table
│  │  ├─ Populate rooms[] array
│  │  └─ roomCount++
│  │
│  └─ In-memory state restored from database
│
├─ Listen on port 9000
│
├─ pthread_create(monitor_exam_thread)
│
└─ Accept client connections

(Server ready to serve clients with previous state recovered)
```

---

## Data Flow Summary

```
┌─────────────────────────────────────────────────────────────┐
│                    CLIENT OPERATIONS                        │
├─────────────────────────────────────────────────────────────┤
│  LOGIN/REGISTER ─────────────────────────────────────────┐  │
│                                                           │  │
│                                                    DATABASE │  │
│                                                           │  │
│  CREATE ROOM ───> STRUCT (in-memory)                    │  │
│                       │                                 │  │
│  LIST ──────────────> STRUCT (read)                    │  │
│                       │                                 │  │
│  PREVIEW ───────────> STRUCT (read)                    │  │
│                       │                                 │  │
│  JOIN ──────────────> STRUCT (add participant)         │  │
│                       │                                 │  │
│  GET_QUESTION ──────> STRUCT (read)                    │  │
│                       │                                 │  │
│  ANSWER ────────────> STRUCT (update answers[])        │  │
│                       │                                 │  │
│  SUBMIT ────────────> STRUCT (calculate score)         │  │
│                           ↓                             │  │
│                    ─────────────────────────────────────→  │
│                    PERSIST ALL TO DATABASE               │  │
│                    (answers + results)                  │  │
│                                                           │  │
│  LEADERBOARD ───────────────────────────────────────────→  │
│                    READ FROM DATABASE                   │  │
│                                                           │  │
│  PRACTICE ──────────────────────────────────────────────→  │
│                    GET RANDOM QUESTION                  │  │
│                                                           │  │
│  ADD_QUESTION ──────────────────────────────────────────→  │
│                    INSERT QUESTION                      │  │
│                                                           │  │
│  DELETE_QUESTION ───────────────────────────────────────→  │
│                    DELETE QUESTION                      │  │
└─────────────────────────────────────────────────────────────┘
```

---

## Key Takeaways for Beginners

1. **STRUCT = Fast, Real-time, Temporary**
   - Used during active tests for speed
   - Gets lost if server crashes before SUBMIT
   - Examples: participants, answers, questions during test

2. **DATABASE = Slow, Persistent, Permanent**
   - Only writes happen at SUBMIT time
   - Data survives server crashes
   - Used for permanent records (users, questions, results)

3. **HYBRID APPROACH**
   - Read from database (questions, user data)
   - Write to struct during test (fast interaction)
   - Flush struct to database at SUBMIT (persist)
   - Read from database for results/leaderboard

4. **MUTEX = Thread Safety**
   - Protects shared struct from concurrent access
   - Acquired before reading/writing struct
   - Released after operation completes
   - Prevents race conditions

5. **TWO-PHASE SYSTEM**
   - Phase 1: During test (struct only)
   - Phase 2: At submit (struct → database)
   - Recovery: Database → struct on server restart

---

End of Sequence Diagrams Document
