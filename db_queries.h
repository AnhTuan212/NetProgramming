#ifndef DB_QUERIES_H
#define DB_QUERIES_H

// Database structures
typedef struct {
    int id;
    char text[256];
    char option_a[128];
    char option_b[128];
    char option_c[128];
    char option_d[128];
    char correct_option;
    int topic_id;
    int difficulty_id;
    char topic[64];           // Added: topic name
    char difficulty[32];      // Added: difficulty name
} DBQuestion;

typedef struct {
    int id;
    char name[128];
    int owner_id;
    int duration_minutes;
    int is_started;
    int is_finished;
} DBRoom;

// 🔧 Participant info for loading from database
typedef struct {
    int db_id;
    char username[64];
    int score;
    char answers[50];  // MAX_QUESTIONS_PER_ROOM = 50
} DBParticipantInfo;

// ==================== QUESTIONS ====================
int db_add_question(const char *text, const char *opt_a, const char *opt_b,
                   const char *opt_c, const char *opt_d, char correct,
                   const char *topic, const char *difficulty, int created_by_id);
int db_get_question(int id, DBQuestion *q);
int db_delete_question(int id);
int db_get_all_questions(DBQuestion *questions, int max_count);
int db_get_questions_by_topic(const char *topic, DBQuestion *questions, int max_count);
int db_get_questions_by_difficulty(const char *difficulty, DBQuestion *questions, int max_count);
int db_get_questions_with_distribution(const char *topic_filter, const char *diff_filter,
                                       DBQuestion *questions, int max_count);
int db_get_all_topics(char *output);
int db_get_all_difficulties(char *output);

// ==================== USERS ====================
int db_add_user(const char *username, const char *password, const char *role);
int db_validate_user(const char *username, const char *password);
int db_get_user_role(const char *username, char *role);
int db_username_exists(const char *username);

// ==================== ROOMS ====================
int db_create_room(const char *name, int owner_id, int duration_minutes);
int db_add_question_to_room(int room_id, int question_id, int order_num);
int db_get_room_questions(int room_id, DBQuestion *questions, int max_count);
int db_get_room(int room_id, DBRoom *room);
int db_get_room_id_by_name(const char *room_name);  // 🔧 Get room ID for deletion
int db_delete_room(int room_id);  // 🔧 Delete room from database
int db_load_all_rooms(DBRoom *rooms, int max_count);  // 🔧 Load all non-finished rooms from database

// ==================== PARTICIPANTS & ANSWERS ====================
int db_add_participant(int room_id, int user_id);
int db_record_answer(int participant_id, int question_id, char selected_option, int is_correct);

// ==================== RESULTS ====================
int db_add_result(int participant_id, int room_id, int score, int total, int correct);
int db_get_leaderboard(int room_id, char *output, int max_size);

// ==================== RESULTS PERSISTENCE ====================
int db_save_participant(int room_id, int user_id);  // 🔧 Save participant when joining room
int db_save_answer(int participant_id, int question_id, char selected_option, int is_correct);  // 🔧 Save answer when submitting
int db_save_result(int participant_id, int room_id, int score, int total_questions, int correct_answers);  // 🔧 Save result when finishing
int db_get_room_results(int room_id, char *output, int max_size);  // 🔧 Get all results for a room

// ==================== LOGS ====================
int db_add_log(int user_id, const char *event_type, const char *description);

// ==================== ROOM PERSISTENCE ====================
int db_load_all_rooms(DBRoom *rooms, int max_count);  // 🔧 Load all non-finished rooms from database
int db_get_username_by_id(int user_id, char *username, int max_len);  // 🔧 Helper for loading rooms
int db_get_random_filtered_questions(const char *topic_ids, int difficulty_id, int limit, DBQuestion *questions);  // 🔧 Get questions with topic+difficulty filter
char* db_parse_topic_filter(const char *filter_str, int *topic_counts, int max_topics);  // 🔧 Parse "topic:count ..." format
void db_parse_difficulty_filter(const char *filter_str, int *difficulty_counts);  // 🔧 Parse "easy:count medium:count ..." format
void db_count_difficulties_for_topics(const char *topic_ids, int *difficulty_counts);  // 🔧 Count difficulties available for selected topics

// ==================== LOAD PARTICIPANTS FOR SERVER RESTART ====================
int db_load_participant_answers(int participant_id, char *answers, int max_len);  // 🔧 Load answers from database
int db_load_room_participants(int room_id, DBParticipantInfo *participants, int max_count);  // 🔧 Load participants + answers from database

#endif // DB_QUERIES_H

