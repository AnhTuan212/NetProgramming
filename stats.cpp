#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <algorithm>
#include <filesystem>
using namespace std;

struct Record {
    string username;
    string room;
    int score;
};

vector<Record> loadResultsTxt(const string &filename = "data/results.txt") {
    vector<Record> recs;
    filesystem::create_directories("data");
    ifstream fin(filename);
    if (!fin.is_open()) return recs;
    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;
        // format: room,username,score
        stringstream ss(line);
        string room, user, scoreStr;
        if (!getline(ss, room, ',')) continue;
        if (!getline(ss, user, ',')) continue;
        if (!getline(ss, scoreStr)) continue;
        try {
            int sc = stoi(scoreStr);
            recs.push_back({user, room, sc});
        } catch (...) { continue; }
    }
    fin.close();
    return recs;
}

void showLeaderboard() {
    auto recs = loadResultsTxt();
    if (recs.empty()) {
        cout << "No data.\n";
        return;
    }

    map<string, pair<int, int>> userStats;
    for (auto &r : recs) {
        userStats[r.username].first += r.score;
        userStats[r.username].second++;
    }

    vector<pair<string, double>> avg;
    for (auto &[user, stat] : userStats)
        avg.push_back({user, (double)stat.first / stat.second});

    sort(avg.begin(), avg.end(),
         [](auto &a, auto &b) { return a.second > b.second; });

    cout << "\n=== Leaderboard ===\n";
    for (auto &[user, avgScore] : avg)
        cout << user << " : " << avgScore << endl;
}