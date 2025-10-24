// ==============================
// server.cpp — main server
// - Start test returns all questions; client answers inline and sends SUBMIT with answersCSV
// - Rooms persisted to data/rooms.txt with participants list
// - Results appended to data/results.txt
// ==============================

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <sstream>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include "nlohmann/json.hpp"
#include "logger.h"

using json = nlohmann::json;
using namespace std;

struct UserSession {
    string username;
    bool loggedIn;
    int socket;
};

struct Question {
    string question;
    vector<string> options; // size 4
    char correct; // 'A','B','C','D'
};

struct TestRoom {
    string name;
    string owner;
    int numQuestions;
    int duration; // seconds
    vector<Question> questions;
    map<string, int> scores; // username -> score (if submitted)
    map<string, vector<char>> answers; // username -> answers (size numQuestions)
    vector<string> participants; // list of usernames
    bool started = false;
};

// Globals
map<int, UserSession> clients;
map<string, TestRoom> rooms;
mutex mtx;
string dataDir = "data";

void appendResultToFile(const string &room, const string &user, int score) {
    filesystem::create_directories(dataDir);
    string path = dataDir + "/results.txt";
    ofstream fout(path, ios::app);
    if (!fout.is_open()) return;
    fout << room << "," << user << "," << score << "\n";
    fout.close();
}

vector<Question> loadQuestionsFromFile(int numQ) {
    vector<Question> out;
    filesystem::create_directories(dataDir);
    string path = dataDir + "/questions.txt";
    ifstream fin(path);
    if (!fin.is_open()) return out;
    string line;
    vector<string> allLines;
    while (getline(fin, line)) {
        if (!line.empty()) allLines.push_back(line);
    }
    fin.close();
    for (size_t i = 0; i < allLines.size() && (int)out.size() < numQ; ++i) {
        string ln = allLines[i];
        vector<string> parts;
        string cur;
        stringstream ss(ln);
        while (getline(ss, cur, '|')) parts.push_back(cur);
        if (parts.size() < 7) continue;
        Question q;
        q.question = parts[1];
        auto stripLabel = [](const string &s)->string {
            if (s.size() > 3 && s[1] == ')') return s.substr(3);
            return s;
        };
        q.options.push_back(stripLabel(parts[2]));
        q.options.push_back(stripLabel(parts[3]));
        q.options.push_back(stripLabel(parts[4]));
        q.options.push_back(stripLabel(parts[5]));
        q.correct = (!parts[6].empty()) ? parts[6][0] : 'A';
        out.push_back(q);
    }
    return out;
}

// Persist all rooms to data/rooms.txt in format:
// room_name,owner,num_questions,duration,member1;member2;member3
void persistRoomsToFile() {
    filesystem::create_directories(dataDir);
    string path = dataDir + "/rooms.txt";
    ofstream fout(path, ios::trunc);
    if (!fout.is_open()) return;
    for (auto &kv : rooms) {
        const TestRoom &r = kv.second;
        fout << r.name << "," << r.owner << "," << r.numQuestions << "," << r.duration << ",";
        for (size_t i = 0; i < r.participants.size(); ++i) {
            if (i) fout << ";";
            fout << r.participants[i];
        }
        fout << "\n";
    }
    fout.close();
}

void loadRoomsFromFile() {
    filesystem::create_directories(dataDir);
    string path = dataDir + "/rooms.txt";
    ifstream fin(path);
    if (!fin.is_open()) return;
    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;
        // room_name,owner,num_questions,duration,member1;member2
        stringstream ss(line);
        string name, owner, numStr, durStr, membersStr;
        if (!getline(ss, name, ',')) continue;
        if (!getline(ss, owner, ',')) continue;
        if (!getline(ss, numStr, ',')) continue;
        if (!getline(ss, durStr, ',')) continue;
        getline(ss, membersStr); // remainder
        try {
            int numQ = stoi(numStr);
            int dur = stoi(durStr);
            TestRoom r;
            r.name = name;
            r.owner = owner;
            r.numQuestions = numQ;
            r.duration = dur;
            r.questions = loadQuestionsFromFile(numQ);
            r.participants.clear();
            if (!membersStr.empty()) {
                stringstream ms(membersStr);
                string member;
                while (getline(ms, member, ';')) {
                    if (!member.empty()) {
                        r.participants.push_back(member);
                        r.scores[member] = 0;
                        r.answers[member].resize(numQ);
                    }
                }
            }
            rooms[name] = r;
        } catch (...) { continue; }
    }
    fin.close();
}

// user validation: data/users.txt format username,password,role OR whitespace separated
bool validateUser(const string &username, const string &password) {
    filesystem::create_directories(dataDir);
    string path = dataDir + "/users.txt";
    ifstream fin(path);
    if (!fin.is_open()) return false;
    string line;
    while (getline(fin, line)) {
        if (line.empty()) continue;
        if (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        string u, p, role;
        if (line.find(',') != string::npos) {
            stringstream ss(line);
            getline(ss, u, ',');
            getline(ss, p, ',');
            getline(ss, role);
        } else {
            stringstream ss(line);
            ss >> u >> p;
        }
        // trim
        auto trim = [](string s)->string {
            s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch){ return !isspace(ch); }));
            s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !isspace(ch); }).base(), s.end());
            return s;
        };
        u = trim(u); p = trim(p);
        if (u == username && p == password) {
            fin.close();
            return true;
        }
    }
    fin.close();
    return false;
}

void sendJSON(int sock, const json &j) {
    string msg = j.dump();
    send(sock, msg.c_str(), msg.size(), 0);
}

void handleClient(int clientSock) {
    char buffer[4096];
    string username;

    json welcome;
    welcome["status"] = "info";
    welcome["message"] = "Welcome to Online Testing Server!";
    sendJSON(clientSock, welcome);
    writeLog("CLIENT_CONNECTED");

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes = recv(clientSock, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) break;

        string input(buffer, bytes);
        while (!input.empty() && (input.back() == '\n' || input.back() == '\r')) input.pop_back();

        stringstream ss(input);
        string cmd;
        ss >> cmd;

        lock_guard<mutex> lock(mtx);

        if (cmd == "LOGIN") {
            string uname, pass;
            ss >> uname >> pass;
            json resp;
            if (validateUser(uname, pass)) {
                clients[clientSock] = {uname, true, clientSock};
                username = uname;
                resp["status"] = "success";
                resp["user"] = uname;
                resp["message"] = "Login success.";
                writeLog(uname + " LOGIN");
            } else {
                resp["status"] = "fail";
                resp["message"] = "Invalid username or password.";
                writeLog("FAILED_LOGIN:" + uname);
            }
            sendJSON(clientSock, resp);
        }

        else if (cmd == "CREATE") {
            string roomName;
            int numQ = 0, dur = 0;
            ss >> roomName >> numQ >> dur;
            json resp;
            if (rooms.count(roomName)) {
                resp["status"] = "fail";
                resp["message"] = "Room already exists.";
            } else {
                TestRoom room;
                room.name = roomName;
                room.owner = username;
                room.numQuestions = numQ;
                room.duration = dur;
                room.questions = loadQuestionsFromFile(numQ);
                rooms[roomName] = room;
                persistRoomsToFile();
                resp["status"] = "success";
                resp["message"] = "Room created successfully.";
                resp["room"] = roomName;
                writeLog(username + " CREATED_ROOM:" + roomName);
            }
            sendJSON(clientSock, resp);
        }

        else if (cmd == "LIST") {
            json resp;
            resp["status"] = "ok";
            for (auto &[name, room] : rooms) {
                json r;
                r["name"] = name;
                r["questions"] = room.numQuestions;
                r["duration"] = room.duration;
                r["participants"] = (int)room.participants.size();
                resp["rooms"].push_back(r);
            }
            sendJSON(clientSock, resp);
        }

        else if (cmd == "JOIN") {
            string roomName, user;
            ss >> roomName >> user; // client sends JOIN room username
            json resp;
            if (rooms.count(roomName)) {
                TestRoom &rm = rooms[roomName];
                // add participant if not exists
                if (find(rm.participants.begin(), rm.participants.end(), user) == rm.participants.end()) {
                    rm.participants.push_back(user);
                    rm.scores[user] = 0;
                    rm.answers[user].resize(rm.numQuestions);
                    persistRoomsToFile();
                }
                resp["status"] = "ok";
                resp["message"] = "Joined room " + roomName;
                resp["participants"] = (int)rm.participants.size();
                writeLog(user + " JOINED " + roomName);
            } else {
                resp["status"] = "fail";
                resp["message"] = "Room not found.";
            }
            sendJSON(clientSock, resp);
        }

        else if (cmd == "START") {
            string roomName;
            ss >> roomName;
            json msg;
            if (!rooms.count(roomName)) {
                json resp;
                resp["status"] = "fail";
                resp["message"] = "Room not found.";
                sendJSON(clientSock, resp);
            } else {
                auto &room = rooms[roomName];
                room.started = true;
                msg["status"] = "start";
                msg["message"] = "Test started!";
                json qarr = json::array();
                for (size_t i = 0; i < room.questions.size(); ++i) {
                    json q;
                    q["id"] = (int)(i+1);
                    q["text"] = room.questions[i].question;
                    if (room.questions[i].options.size() >= 4) {
                        q["A"] = room.questions[i].options[0];
                        q["B"] = room.questions[i].options[1];
                        q["C"] = room.questions[i].options[2];
                        q["D"] = room.questions[i].options[3];
                    }
                    qarr.push_back(q);
                }
                msg["questions"] = qarr;
                // send to starter (client will receive and answer inline)
                sendJSON(clientSock, msg);
                writeLog(username + " STARTED " + roomName);
            }
        }

        else if (cmd == "ANSWER") {
            // kept for backward compatibility
            string roomName;
            int qNum;
            char ans;
            ss >> roomName >> qNum >> ans;
            auto &room = rooms[roomName];
            room.answers[username].resize(room.numQuestions);
            room.answers[username][qNum - 1] = ans;
            json resp = {{"status", "ok"}, {"message", "Answer saved."}};
            sendJSON(clientSock, resp);
        }

        else if (cmd == "SUBMIT") {
            // Accept two forms:
            // 1) SUBMIT room user answersCSV  (answersCSV: A,B,C,...)
            // 2) SUBMIT room user               (use previously recorded answers)
            string roomName, user, answersCSV;
            ss >> roomName >> user;
            if (!(ss >> answersCSV)) answersCSV = ""; // may be empty

            json resp;
            if (!rooms.count(roomName)) {
                resp["status"] = "fail";
                resp["message"] = "Room not found.";
                sendJSON(clientSock, resp);
            } else {
                auto &room = rooms[roomName];
                int score = 0;
                vector<char> given;
                if (!answersCSV.empty()) {
                    // parse CSV
                    string tmp;
                    stringstream s2(answersCSV);
                    while (getline(s2, tmp, ',')) {
                        if (!tmp.empty()) {
                            char c = toupper(tmp[0]);
                            given.push_back(c);
                        } else {
                            given.push_back(' ');
                        }
                    }
                } else {
                    // use recorded answers
                    given = room.answers[user];
                }
                for (int i = 0; i < room.numQuestions; ++i) {
                    if (i < (int)given.size() && given[i] == room.questions[i].correct) score++;
                }
                room.scores[user] = score;
                appendResultToFile(roomName, user, score);

                resp["status"] = "done";
                resp["score"] = score;
                resp["total"] = room.numQuestions;
                resp["message"] = "Test submitted.";
                writeLog(user + " SUBMITTED " + roomName + " SCORE:" + to_string(score));
                sendJSON(clientSock, resp);
            }
        }

        else if (cmd == "RESULTS") {
            string roomName;
            ss >> roomName;
            json resp;
            resp["status"] = "ok";
            resp["room"] = roomName;
            if (!rooms.count(roomName)) {
                resp["results"] = json::array();
                resp["total"] = 0;
            } else {
                auto &room = rooms[roomName];
                resp["total"] = room.numQuestions;
                json results = json::array();
                for (auto &[user, s] : room.scores) {
                    json r;
                    r["user"] = user;
                    r["score"] = s;
                    results.push_back(r);
                }
                // sort results by score desc
                sort(results.begin(), results.end(), [](const json &a, const json &b){
                    return a.value("score",0) > b.value("score",0);
                });
                resp["results"] = results;
            }
            sendJSON(clientSock, resp);
        }

        else if (cmd == "EXIT") {
            writeLog(username + " DISCONNECTED");
            break;
        } else {
            json resp;
            resp["status"] = "fail";
            resp["message"] = "Unknown command.";
            sendJSON(clientSock, resp);
        }
        writeLog("CMD from " + (username.empty() ? string("unknown") : username) + ": " + input);
    }

    close(clientSock);
    lock_guard<mutex> lock(mtx);
    clients.erase(clientSock);
    cout << username << " disconnected.\n";
    writeLog((username.empty() ? string("unknown") : username) + " connection closed");
}

int main() {
    filesystem::create_directories(dataDir);
    loadRoomsFromFile();

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{}, clientAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Bind failed.\n";
        return 1;
    }
    listen(serverSock, 10);
    cout << "Server running on port 9000...\n";
    writeLog("SERVER_START");

    while (true) {
        socklen_t clientSize = sizeof(clientAddr);
        int clientSock = accept(serverSock, (sockaddr*)&clientAddr, &clientSize);
        if (clientSock >= 0) {
            cout << "New client connected.\n";
            thread(handleClient, clientSock).detach();
        }
    }
    close(serverSock);
    return 0;
}