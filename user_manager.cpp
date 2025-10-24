#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <filesystem>
#include <algorithm>
#include <cctype>
using namespace std;

static inline string trim_str(string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
    return s;
}

class UserManager {
    map<string, string> users;
    string filePath = "data/users.txt";

public:
    UserManager() { loadUsers(); }

    void loadUsers() {
        users.clear();
        filesystem::create_directories("data");
        ifstream file(filePath);
        if (!file.is_open()) return;
        string line;
        while (getline(file, line)) {
            if (line.empty()) continue;
            if (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            string uname, pass, role;
            if (line.find(',') != string::npos) {
                stringstream ss(line);
                getline(ss, uname, ',');
                getline(ss, pass, ',');
                getline(ss, role);
            } else {
                stringstream ss(line);
                ss >> uname >> pass;
            }
            uname = trim_str(uname);
            pass = trim_str(pass);
            if (!uname.empty()) users[uname] = pass;
        }
        file.close();
    }

    bool validateUser(const string &username, const string &password) {
        loadUsers();
        return users.count(username) && users[username] == password;
    }

    bool registerUser(const string &username, const string &password, const string &role = "student") {
        loadUsers();
        if (users.count(username)) return false;
        filesystem::create_directories("data");
        ofstream file(filePath, ios::app);
        if (!file.is_open()) return false;
        file << username << "," << password << "," << role << "\n";
        file.close();
        return true;
    }
};

UserManager userManager;