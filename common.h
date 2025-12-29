#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <sqlite3.h>
#include <math.h>

#define DB_PATH "test_system.db"
#define MAX_QUESTIONS_PER_ROOM 50

typedef struct {
    int id;
    char text[256];
    char A[128];
    char B[128];
    char C[128];
    char D[128];
    char correct;
    char topic[64];
    char difficulty[32];
} QItem;

typedef struct {
    char username[64];
    int score; 
    char answers[MAX_QUESTIONS_PER_ROOM];
    int score_history[10]; 
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
    Participant participants[50];
    int participantCount;
    int started;
    time_t start_time;
} Room;

typedef struct {
    int sock;
    char username[64];
    int user_id;
    int loggedIn;
    char role[32];
} Client;

// Function prototypes
void writeLog(const char *event);
int register_user_with_role(const char *username, const char *password, const char *role);
int validate_user(const char *username, const char *password, char *role_out);
int db_init(const char *db_path);
int db_create_tables(void);
int db_init_default_difficulties(void);
void db_close(void);
int db_get_user_id(const char *username);
int db_add_question(const char *text, const char *A, const char *B, const char *C, const char *D, 
                    char correct, const char *topic, const char *difficulty, int creator_id);
int search_questions_by_id(int id, QItem *q);
int search_questions_by_topic(const char *topic, char *output);
int search_questions_by_difficulty(const char *difficulty, char *output);
int migrate_from_text_files(const char *data_dir);
int verify_migration(void);
void show_leaderboard(const char *output_file);
void show_leaderboard_for_room(const char *room_name, const char *output_file);
void to_lowercase(char *str);
void to_uppercase(char *str);
void init_max_question_id(void);
int get_next_question_id(void);
int add_question_to_file(const QItem *q);
int delete_question_by_id(int id);
int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff);
int loadQuestionsWithFilters(const char *filename, QItem *questions, int maxQ,
                             const char *topic_filter, const char *diff_filter);
int loadQuestionsWithMultipleFilters(QItem *questions, int maxQ,
                                      const char *topic_filter, const char *diff_filter);
int validate_question_input(const QItem *q, char *error_msg);
int get_all_topics_with_counts(char *output);
int get_all_difficulties_with_counts(char *output);
int get_difficulties_for_topics(const char *topic_filter, char *output);
int loadQuestionsFromDB(QItem *questions, int maxQ,
                        const char *topic_filter, const char *diff_filter);
int shuffle_questions(QItem *questions, int count);
int remove_duplicate_questions(QItem *questions, int *count);

#endif