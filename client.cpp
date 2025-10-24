#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <nlohmann/json.hpp>
#include <limits>

using json = nlohmann::json;
using namespace std;

// === Color & format helpers ===
#define RESET   "\033[0m"
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

// === State ===
string currentUser = "";
string currentRoom = "";
bool loggedIn = false;
bool connected = false;

int sockfd;

// === Helper functions ===
void sendMessage(const string &msg) {
    send(sockfd, msg.c_str(), msg.size(), 0);
}

string recvMessage() {
    char buffer[8192];
    memset(buffer, 0, sizeof(buffer));
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    if (n <= 0) return string();
    return string(buffer, n);
}

void printBanner() {
    system("clear");
    cout << BOLD << CYAN;
    cout << "====== ONLINE TEST CLIENT ======\n";
    if (loggedIn) {
        cout << "👤 Logged in as: " << GREEN << currentUser << RESET << "\n";
        if (!currentRoom.empty()) cout << "🏠 Current Room: " << YELLOW << currentRoom << RESET << "\n";
    } else {
        cout << "👤 Not logged in\n";
    }
    cout << CYAN << "================================" << RESET << "\n";
    cout << "1. Login\n";
    cout << "2. Create Test Room\n";
    cout << "3. View Room List\n";
    cout << "4. Join Room\n";
    cout << "5. Start Test (answer questions inline)\n";
    cout << "6. Submit Test (if needed)\n";
    cout << "7. View Results\n";
    cout << "8. Exit\n";
    cout << CYAN << "================================" << RESET << "\n>> ";
}

// === Menu functions ===

void handleLogin() {
    string username, password;
    cout << "Enter username: ";
    getline(cin, username);
    cout << "Enter password: ";
    getline(cin, password);

    string cmd = "LOGIN " + username + " " + password;
    sendMessage(cmd);
    string res = recvMessage();
    if (res.empty()) { cout << RED << "No response from server.\n" << RESET; return; }
    json rsp = json::parse(res);

    if (rsp.value("status", "") == "success") {
        loggedIn = true;
        currentUser = username;
        cout << GREEN << "✅ Login successful! Welcome, " << username << "!\n" << RESET;
    } else {
        cout << RED << "❌ Login failed: " << rsp.value("message", "Invalid username or password.") << "\n" << RESET;
    }
}

void handleCreateRoom() {
    if (!loggedIn) { cout << RED << "⚠️ Please login first.\n" << RESET; return; }
    string room;
    int num, dur;
    cout << "Room name: "; getline(cin, room);
    cout << "Number of questions: "; cin >> num; cin.ignore();
    cout << "Duration (seconds): "; cin >> dur; cin.ignore();

    stringstream ss;
    ss << "CREATE " << room << " " << num << " " << dur;
    sendMessage(ss.str());
    string res = recvMessage();
    if (res.empty()) { cout << RED << "No response from server.\n" << RESET; return; }
    json rsp = json::parse(res);

    if (rsp.value("status", "") == "success") {
        cout << GREEN << "✅ Room '" << room << "' created successfully!\n" << RESET;
    } else {
        cout << YELLOW << "⚠️ " << rsp.value("message", "unknown") << "\n" << RESET;
    }
}

void handleViewRooms() {
    sendMessage("LIST");
    string res = recvMessage();
    if (res.empty()) { cout << RED << "No response from server.\n" << RESET; return; }
    json rsp = json::parse(res);

    cout << CYAN << "\n====== AVAILABLE ROOMS ======\n";
    if (!rsp.contains("rooms") || rsp["rooms"].empty()) {
        cout << YELLOW << "No rooms available.\n";
    } else {
        for (auto &r : rsp["rooms"]) {
            cout << "🏠 " << r.value("name", string("unknown"))
                 << " | 👥 " << r.value("participants", 0)
                 << " | 🕒 " << r.value("duration", 0) << "s\n";
        }
    }
    cout << CYAN << "=============================\n" << RESET;
}

void handleJoinRoom() {
    if (!loggedIn) { cout << RED << "⚠️ Please login first.\n" << RESET; return; }
    string room;
    cout << "Enter room name to join: ";
    getline(cin, room);

    string cmd = "JOIN " + room + " " + currentUser;
    sendMessage(cmd);
    string res = recvMessage();
    if (res.empty()) { cout << RED << "No response from server.\n" << RESET; return; }
    json rsp = json::parse(res);

    if (rsp.value("status", "") == "ok" || rsp.value("status", "") == "success") {
        currentRoom = room;
        cout << GREEN << "✅ Joined room '" << room << "' successfully!\n";
        cout << "👥 Participants: " << rsp.value("participants", 0) << "\n" << RESET;
    } else {
        cout << RED << "❌ " << rsp.value("message", "unknown") << "\n" << RESET;
    }
}

// New behavior: Start test prompts questions one-by-one and collects answers
void handleStartTest() {
    if (currentRoom.empty()) { cout << RED << "⚠️ Join a room first.\n" << RESET; return; }

    string cmd = "START " + currentRoom;
    sendMessage(cmd);
    string res = recvMessage();
    if (res.empty()) { cout << RED << "No response from server.\n" << RESET; return; }
    json rsp = json::parse(res);

    if (!rsp.contains("questions") || rsp.value("status", "") != "start") {
        cout << YELLOW << "Could not start test: " << rsp.value("message", "unknown") << "\n" << RESET;
        return;
    }

    cout << CYAN << "\n🎯 Starting test for room [" << currentRoom << "]...\n" << RESET;
    vector<string> answers;
    for (auto &q : rsp["questions"]) {
        cout << "Q" << q.value("id", 0) << ": " << q.value("text", string("")) << "\n";
        cout << "A. " << q.value("A", string("")) << "\n";
        cout << "B. " << q.value("B", string("")) << "\n";
        cout << "C. " << q.value("C", string("")) << "\n";
        cout << "D. " << q.value("D", string("")) << "\n";
        string ans;
        while (true) {
            cout << "Answer (A/B/C/D): ";
            getline(cin, ans);
            if (ans.empty()) { cout << "Please enter A/B/C/D.\n"; continue; }
            char c = toupper(ans[0]);
            if (c=='A' || c=='B' || c=='C' || c=='D') { ans = string(1,c); break; }
            cout << "Invalid. Enter A, B, C, or D.\n";
        }
        answers.push_back(ans);
        cout << "\n";
    }

    // join answers to CSV
    string answersCSV;
    for (size_t i = 0; i < answers.size(); ++i) {
        if (i) answersCSV += ",";
        answersCSV += answers[i];
    }

    // send SUBMIT room user answersCSV
    stringstream ss;
    ss << "SUBMIT " << currentRoom << " " << currentUser << " " << answersCSV;
    sendMessage(ss.str());
    string res2 = recvMessage();
    if (res2.empty()) { cout << RED << "No response from server (submit failed).\n" << RESET; return; }
    json rsp2 = json::parse(res2);

    if (rsp2.value("status","") == "done") {
        cout << GREEN << "✅ Test submitted!\nYour score: "
             << rsp2.value("score", 0) << "/" << rsp2.value("total", 0) << "\n" << RESET;
    } else {
        cout << RED << "Submit failed: " << rsp2.value("message", "unknown") << "\n" << RESET;
    }
}

void handleSubmitManual() {
    if (currentRoom.empty()) { cout << RED << "⚠️ Join a room first.\n" << RESET; return; }
    // kept for compatibility: allow manual submit without answers (will use recorded answers if any)
    stringstream ss;
    ss << "SUBMIT " << currentRoom << " " << currentUser;
    sendMessage(ss.str());
    string res = recvMessage();
    if (res.empty()) { cout << RED << "No response from server.\n" << RESET; return; }
    json rsp = json::parse(res);
    cout << GREEN << "✅ Test submitted!\nYour score: "
         << rsp.value("score", 0) << "/" << rsp.value("total", 0) << "\n" << RESET;
}

void handleViewResults() {
    string room;
    cout << "Room name to view results: ";
    getline(cin, room);
    string cmd = "RESULTS " + room;
    sendMessage(cmd);
    string res = recvMessage();
    if (res.empty()) { cout << RED << "No response from server.\n" << RESET; return; }
    json rsp = json::parse(res);

    cout << CYAN << "\n====== RESULTS (" << room << ") ======\n";
    if (rsp.contains("results")) {
        for (auto &r : rsp["results"]) {
            cout << r.value("user", string("")) << " : "
                 << r.value("score", 0) << "/" << rsp.value("total", 0) << "\n";
        }
    } else {
        cout << YELLOW << "No results.\n";
    }
    cout << CYAN << "=============================\n" << RESET;
}

void handleExit() {
    sendMessage("EXIT");
    cout << YELLOW << "Disconnecting... Goodbye " << currentUser << " 👋\n" << RESET;
    close(sockfd);
    connected = false;
}

// === Main ===
int main() {
    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << RED << "❌ Connection failed.\n" << RESET;
        return 1;
    }
    connected = true;
    cout << GREEN << "Connected to Online Test Server.\n" << RESET;

    while (connected) {
        printBanner();
        string choice;
        getline(cin, choice);
        if (choice == "1") handleLogin();
        else if (choice == "2") handleCreateRoom();
        else if (choice == "3") handleViewRooms();
        else if (choice == "4") handleJoinRoom();
        else if (choice == "5") handleStartTest();
        else if (choice == "6") handleSubmitManual();
        else if (choice == "7") handleViewResults();
        else if (choice == "8") { handleExit(); break; }
        else cout << RED << "Invalid choice!\n" << RESET;

        cout << "\nPress Enter to continue...";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    return 0;
}