#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
using namespace std;

struct QItem {
    int id;
    string text;
    string A, B, C, D;
    char correct; // 'A'..'D'
};

// parse questions from data/questions.txt
// each line: id|question text|A) optionA|B) optionB|C) optionC|D) optionD|A
vector<QItem> loadQuestionsTxt(const string &filename = "data/questions.txt") {
    vector<QItem> qs;
    filesystem::create_directories("data");
    ifstream fin(filename);
    if (!fin.is_open()) return qs;

    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;
        vector<string> parts;
        string cur;
        stringstream ss(line);
        while (getline(ss, cur, '|')) {
            parts.push_back(cur);
        }
        if (parts.size() < 7) continue;
        QItem q;
        try {
            q.id = stoi(parts[0]);
        } catch (...) { q.id = 0; }
        q.text = parts[1];

        // strip leading "A) " etc from options if present
        auto stripLabel = [](const string &s)->string {
            if (s.size() > 3 && s[1] == ')') return s.substr(3);
            return s;
        };

        q.A = stripLabel(parts[2]);
        q.B = stripLabel(parts[3]);
        q.C = stripLabel(parts[4]);
        q.D = stripLabel(parts[5]);
        q.correct = !parts[6].empty() ? parts[6][0] : 'A';
        qs.push_back(q);
    }
    fin.close();
    return qs;
}