#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ===== UTILITY FUNCTIONS =====

void to_lowercase(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void to_uppercase(char *str) {
    if (!str) return;
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

// ===== ENUMERATION FUNCTIONS =====

int get_all_topics_with_counts(char *output) {
    if (!output) return -1;
    
    FILE *fp = fopen("data/questions.txt", "r");
    if (!fp) {
        output[0] = '\0';
        return -1;
    }

    typedef struct {
        char name[64];
        int count;
    } TopicCount;

    TopicCount topics[50];
    int topic_count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        char temp[1024];
        strcpy(temp, line);

        // Skip first 7 fields to get to topic
        for (int i = 0; i < 7; i++) {
            if (!strtok(i == 0 ? temp : NULL, "|")) {
                goto next_line;
            }
        }

        char *topic_str = strtok(NULL, "|");
        if (!topic_str) goto next_line;

        to_lowercase(topic_str);

        // Find or add topic
        int found = 0;
        for (int i = 0; i < topic_count; i++) {
            if (strcmp(topics[i].name, topic_str) == 0) {
                topics[i].count++;
                found = 1;
                break;
            }
        }
        if (!found && topic_count < 50) {
            strcpy(topics[topic_count].name, topic_str);
            topics[topic_count].count = 1;
            topic_count++;
        }

next_line:
        continue;
    }
    fclose(fp);

    // Format output
    output[0] = '\0';
    for (int i = 0; i < topic_count; i++) {
        char buf[128];
        sprintf(buf, "%s:%d", topics[i].name, topics[i].count);
        if (i > 0) strcat(output, "|");
        strcat(output, buf);
    }

    return 0;
}

int get_all_difficulties_with_counts(char *output) {
    if (!output) return -1;

    FILE *fp = fopen("data/questions.txt", "r");
    if (!fp) {
        output[0] = '\0';
        return -1;
    }

    int easy_count = 0, medium_count = 0, hard_count = 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        char temp[1024];
        strcpy(temp, line);

        // Skip first 8 fields to get to difficulty
        for (int i = 0; i < 8; i++) {
            if (!strtok(i == 0 ? temp : NULL, "|")) {
                goto next_diff;
            }
        }

        char *diff_str = strtok(NULL, "|");
        if (!diff_str) goto next_diff;

        char diff_lower[32];
        strcpy(diff_lower, diff_str);
        to_lowercase(diff_lower);

        if (strcmp(diff_lower, "easy") == 0) easy_count++;
        else if (strcmp(diff_lower, "medium") == 0) medium_count++;
        else if (strcmp(diff_lower, "hard") == 0) hard_count++;

next_diff:
        continue;
    }
    fclose(fp);

    sprintf(output, "easy:%d|medium:%d|hard:%d", easy_count, medium_count, hard_count);
    return 0;
}

// ===== QUESTION VALIDATION =====

int validate_question_input(const QItem *q, char *error_msg) {
    if (!q || !error_msg) return 0;

    if (strlen(q->text) < 10) {
        strcpy(error_msg, "ERROR: Question text too short (min 10 chars)");
        return 0;
    }

    if (strlen(q->A) == 0) {
        strcpy(error_msg, "ERROR: Option A is empty");
        return 0;
    }
    if (strlen(q->B) == 0) {
        strcpy(error_msg, "ERROR: Option B is empty");
        return 0;
    }
    if (strlen(q->C) == 0) {
        strcpy(error_msg, "ERROR: Option C is empty");
        return 0;
    }
    if (strlen(q->D) == 0) {
        strcpy(error_msg, "ERROR: Option D is empty");
        return 0;
    }

    char correct_upper = toupper(q->correct);
    if (correct_upper != 'A' && correct_upper != 'B' && correct_upper != 'C' && correct_upper != 'D') {
        strcpy(error_msg, "ERROR: Correct answer must be A, B, C, or D");
        return 0;
    }

    if (strlen(q->topic) == 0) {
        strcpy(error_msg, "ERROR: Topic cannot be empty");
        return 0;
    }

    char diff_lower[32];
    strcpy(diff_lower, q->difficulty);
    to_lowercase(diff_lower);

    if (strcmp(diff_lower, "easy") != 0 && strcmp(diff_lower, "medium") != 0 && strcmp(diff_lower, "hard") != 0) {
        strcpy(error_msg, "ERROR: Difficulty must be: easy, medium, or hard");
        return 0;
    }

    return 1;
}

// ===== FILE OPERATIONS =====

int add_question_to_file(const QItem *q) {
    if (!q) return -1;

    // Find max ID
    FILE *fp = fopen("data/questions.txt", "r");
    int max_id = 0;
    char line[1024];

    if (fp) {
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) < 2) continue;

            int id = atoi(line);
            if (id > max_id) max_id = id;
        }
        fclose(fp);
    }

    int new_id = max_id + 1;

    // Create normalized question
    QItem new_q = *q;
    new_q.id = new_id;
    to_lowercase(new_q.topic);
    to_lowercase(new_q.difficulty);
    new_q.correct = toupper(new_q.correct);

    // Append to file
    fp = fopen("data/questions.txt", "a");
    if (!fp) return -1;

    fprintf(fp, "%d|%s|%s|%s|%s|%s|%c|%s|%s\n",
            new_q.id, new_q.text, new_q.A, new_q.B, new_q.C, new_q.D,
            new_q.correct, new_q.topic, new_q.difficulty);

    fclose(fp);
    return new_id;
}

// ===== DEDUPLICATION & RANDOMIZATION =====

int remove_duplicate_questions(QItem *questions, int *count) {
    if (!questions || !count || *count <= 0) return 0;

    int seen_ids[200];
    memset(seen_ids, -1, sizeof(seen_ids));

    int write_idx = 0;
    int original_count = *count;

    for (int i = 0; i < original_count; i++) {
        // Check if ID already seen
        int found = 0;
        for (int j = 0; j < write_idx; j++) {
            if (seen_ids[j] == questions[i].id) {
                found = 1;
                break;
            }
        }

        if (!found) {
            seen_ids[write_idx] = questions[i].id;
            questions[write_idx] = questions[i];
            write_idx++;
        }
    }

    int removed = original_count - write_idx;
    *count = write_idx;
    return removed;
}

int shuffle_questions(QItem *questions, int count) {
    if (!questions || count <= 1) return 0;

    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        QItem temp = questions[i];
        questions[i] = questions[j];
        questions[j] = temp;
    }
    return 0;
}

// ===== MAIN LOADING FUNCTION =====

int loadQuestionsTxt(const char *filename, QItem *questions, int maxQ,
                     const char *topic, const char *diff) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), fp) && count < maxQ) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        // Copy line để strtok không phá
        char temp[1024];
        strcpy(temp, line);

        char *token = strtok(temp, "|");
        if (!token) continue;

        QItem q;
        q.id = atoi(token);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.text, token, sizeof(q.text)-1); q.text[sizeof(q.text)-1] = '\0';

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.A, token, sizeof(q.A)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.B, token, sizeof(q.B)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.C, token, sizeof(q.C)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.D, token, sizeof(q.D)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        q.correct = toupper(token[0]);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.topic, token, sizeof(q.topic)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.difficulty, token, sizeof(q.difficulty)-1);

        // Filter
        if (topic && topic[0] && strcasecmp(q.topic, topic) != 0) continue;
        if (diff && diff[0] && strcasecmp(q.difficulty, diff) != 0) continue;

        questions[count++] = q;
    }
    fclose(fp);

    // Deduplicate by question ID
    int removed = remove_duplicate_questions(questions, &count);
    if (removed > 0) {
        char log_msg[128];
        sprintf(log_msg, "Removed %d duplicate questions during load", removed);
        writeLog(log_msg);
    }

    // Randomize question order
    shuffle_questions(questions, count);

    return count;
}

// Load questions with topic and difficulty distribution filters
// topic_filter format: "topic1:count1 topic2:count2 ..."
// diff_filter format: "difficulty1:count1 difficulty2:count2 ..."
int loadQuestionsWithFilters(const char *filename, QItem *questions, int maxQ,
                             const char *topic_filter, const char *diff_filter) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    // Parse topic requirements
    char topics_requested[32][64];
    int topic_counts[32];
    int topic_req_count = 0;
    
    if (topic_filter && strlen(topic_filter) > 0) {
        char topic_copy[256];
        strcpy(topic_copy, topic_filter);
        
        char *token = strtok(topic_copy, " ");
        while (token && topic_req_count < 32) {
            char *colon = strchr(token, ':');
            if (colon) {
                int name_len = colon - token;
                strncpy(topics_requested[topic_req_count], token, name_len);
                topics_requested[topic_req_count][name_len] = '\0';
                topic_counts[topic_req_count] = atoi(colon + 1);
                topic_req_count++;
            }
            token = strtok(NULL, " ");
        }
    }
    
    // Parse difficulty requirements
    char diffs_requested[10][32];
    int diff_counts[10];
    int diff_req_count = 0;
    
    if (diff_filter && strlen(diff_filter) > 0) {
        char diff_copy[256];
        strcpy(diff_copy, diff_filter);
        
        char *token = strtok(diff_copy, " ");
        while (token && diff_req_count < 10) {
            char *colon = strchr(token, ':');
            if (colon) {
                int name_len = colon - token;
                strncpy(diffs_requested[diff_req_count], token, name_len);
                diffs_requested[diff_req_count][name_len] = '\0';
                diff_counts[diff_req_count] = atoi(colon + 1);
                diff_req_count++;
            }
            token = strtok(NULL, " ");
        }
    }
    
    // Track how many we've collected from each topic/difficulty combo
    int topic_collected[32];
    int diff_collected[10];
    for (int i = 0; i < 32; i++) topic_collected[i] = 0;
    for (int i = 0; i < 10; i++) diff_collected[i] = 0;
    
    char line[1024];
    QItem *temp_questions = malloc(sizeof(QItem) * MAX_QUESTIONS_PER_ROOM * 2);
    int temp_count = 0;
    int total_required = 0;
    
    // Calculate total questions required
    if (topic_req_count > 0) {
        for (int i = 0; i < topic_req_count; i++) {
            total_required += topic_counts[i];
        }
    }
    if (diff_req_count > 0) {
        for (int i = 0; i < diff_req_count; i++) {
            total_required += diff_counts[i];
        }
    }
    if (total_required == 0) total_required = maxQ;
    
    // Read file and collect matching questions
    while (fgets(line, sizeof(line), fp) && temp_count < total_required * 2) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        char temp[1024];
        strcpy(temp, line);

        char *token = strtok(temp, "|");
        if (!token) continue;

        QItem q;
        q.id = atoi(token);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.text, token, sizeof(q.text)-1); q.text[sizeof(q.text)-1] = '\0';

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.A, token, sizeof(q.A)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.B, token, sizeof(q.B)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.C, token, sizeof(q.C)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.D, token, sizeof(q.D)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        q.correct = toupper(token[0]);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.topic, token, sizeof(q.topic)-1);

        token = strtok(NULL, "|"); if (!token) continue;
        strncpy(q.difficulty, token, sizeof(q.difficulty)-1);

        // Check if this question matches topic requirements
        int topic_match = 0;
        int topic_idx = -1;
        if (topic_req_count > 0) {
            for (int i = 0; i < topic_req_count; i++) {
                if (strcasecmp(q.topic, topics_requested[i]) == 0) {
                    if (topic_collected[i] < topic_counts[i]) {
                        topic_match = 1;
                        topic_idx = i;
                        break;
                    }
                }
            }
            if (!topic_match) continue;
        }

        // Check if this question matches difficulty requirements
        int diff_match = 0;
        int diff_idx = -1;
        if (diff_req_count > 0) {
            for (int i = 0; i < diff_req_count; i++) {
                if (strcasecmp(q.difficulty, diffs_requested[i]) == 0) {
                    if (diff_collected[i] < diff_counts[i]) {
                        diff_match = 1;
                        diff_idx = i;
                        break;
                    }
                }
            }
            if (!diff_match) continue;
        }

        // Add question
        temp_questions[temp_count++] = q;
        if (topic_idx >= 0) topic_collected[topic_idx]++;
        if (diff_idx >= 0) diff_collected[diff_idx]++;

        // Check if we have enough
        if (topic_req_count > 0) {
            int all_satisfied = 1;
            for (int i = 0; i < topic_req_count; i++) {
                if (topic_collected[i] < topic_counts[i]) {
                    all_satisfied = 0;
                    break;
                }
            }
            if (all_satisfied && diff_req_count == 0) break;
        }
        if (diff_req_count > 0 && topic_req_count == 0) {
            int all_satisfied = 1;
            for (int i = 0; i < diff_req_count; i++) {
                if (diff_collected[i] < diff_counts[i]) {
                    all_satisfied = 0;
                    break;
                }
            }
            if (all_satisfied) break;
        }
    }
    fclose(fp);

    // Copy to output (limited to maxQ)
    int count = 0;
    for (int i = 0; i < temp_count && count < maxQ; i++) {
        questions[count++] = temp_questions[i];
    }
    free(temp_questions);

    if (count == 0) return 0;

    // Deduplicate by question ID
    int removed = remove_duplicate_questions(questions, &count);
    if (removed > 0) {
        char log_msg[128];
        sprintf(log_msg, "Removed %d duplicate questions during filtered load", removed);
        writeLog(log_msg);
    }

    // Randomize question order
    shuffle_questions(questions, count);

    return count;
}

// Search questions by ID
int search_questions_by_id(int id, QItem *result) {
    FILE *fp = fopen("data/questions.txt", "r");
    if (!fp) return 0;

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        char temp[1024];
        strcpy(temp, line);

        char *token = strtok(temp, "|");
        if (!token) continue;

        int q_id = atoi(token);
        if (q_id == id) {
            // Parse the full question
            strcpy(temp, line);
            token = strtok(temp, "|");
            result->id = atoi(token);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            strncpy(result->text, token, sizeof(result->text)-1);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            strncpy(result->A, token, sizeof(result->A)-1);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            strncpy(result->B, token, sizeof(result->B)-1);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            strncpy(result->C, token, sizeof(result->C)-1);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            strncpy(result->D, token, sizeof(result->D)-1);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            result->correct = toupper(token[0]);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            strncpy(result->topic, token, sizeof(result->topic)-1);

            token = strtok(NULL, "|"); if (!token) { fclose(fp); return 0; }
            strncpy(result->difficulty, token, sizeof(result->difficulty)-1);

            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

// Search questions by topic (returns formatted string)
int search_questions_by_topic(const char *topic, char *output) {
    FILE *fp = fopen("data/questions.txt", "r");
    if (!fp) return 0;

    char line[1024];
    int count = 0;
    strcpy(output, "");

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        char temp[1024];
        strcpy(temp, line);

        char *token = strtok(temp, "|");
        if (!token) continue;
        int q_id = atoi(token);

        token = strtok(NULL, "|"); if (!token) continue;
        char text[256];
        strncpy(text, token, sizeof(text)-1);

        // Skip A, B, C, D
        for (int i = 0; i < 4; i++) {
            token = strtok(NULL, "|");
            if (!token) break;
        }

        token = strtok(NULL, "|"); if (!token) continue; // correct
        token = strtok(NULL, "|"); if (!token) continue; // topic
        char q_topic[64];
        strncpy(q_topic, token, sizeof(q_topic)-1);

        if (strcasecmp(q_topic, topic) == 0) {
            char entry[512];
            snprintf(entry, sizeof(entry), "%d|%s\n", q_id, text);
            if (strlen(output) + strlen(entry) < 8000) {
                strcat(output, entry);
                count++;
            }
        }
    }
    fclose(fp);
    return count;
}

// Search questions by difficulty (returns formatted string)
int search_questions_by_difficulty(const char *difficulty, char *output) {
    FILE *fp = fopen("data/questions.txt", "r");
    if (!fp) return 0;

    char line[1024];
    int count = 0;
    strcpy(output, "");

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) < 10) continue;

        char temp[1024];
        strcpy(temp, line);

        char *token = strtok(temp, "|");
        if (!token) continue;
        int q_id = atoi(token);

        token = strtok(NULL, "|"); if (!token) continue;
        char text[256];
        strncpy(text, token, sizeof(text)-1);

        // Skip A, B, C, D, correct, topic
        for (int i = 0; i < 6; i++) {
            token = strtok(NULL, "|");
            if (!token) break;
        }

        token = strtok(NULL, "|"); if (!token) continue; // difficulty
        char q_difficulty[32];
        strncpy(q_difficulty, token, sizeof(q_difficulty)-1);

        if (strcasecmp(q_difficulty, difficulty) == 0) {
            char entry[512];
            snprintf(entry, sizeof(entry), "%d|%s\n", q_id, text);
            if (strlen(output) + strlen(entry) < 8000) {
                strcat(output, entry);
                count++;
            }
        }
    }
    fclose(fp);
    return count;
}

// Delete question by ID from file
int delete_question_by_id(int id) {
    FILE *fp = fopen("data/questions.txt", "r");
    if (!fp) return 0;

    FILE *temp_fp = fopen("data/questions.txt.tmp", "w");
    if (!temp_fp) {
        fclose(fp);
        return 0;
    }

    char line[1024];
    int found = 0;

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        
        if (strlen(line) < 10) {
            fprintf(temp_fp, "%s\n", line);
            continue;
        }

        char temp[1024];
        strcpy(temp, line);
        char *token = strtok(temp, "|");
        if (!token) {
            fprintf(temp_fp, "%s\n", line);
            continue;
        }

        int q_id = atoi(token);
        if (q_id == id) {
            found = 1;
            // Skip this line (don't write it)
        } else {
            fprintf(temp_fp, "%s\n", line);
        }
    }

    fclose(fp);
    fclose(temp_fp);

    if (found) {
        // Replace original file with temp file
        remove("data/questions.txt");
        rename("data/questions.txt.tmp", "data/questions.txt");
        return 1;
    } else {
        // Remove temp file if question not found
        remove("data/questions.txt.tmp");
        return 0;
    }
}