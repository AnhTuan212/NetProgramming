#include "logger.h"
#include <fstream>
#include <ctime>
#include <filesystem>
#include <string>

using namespace std;

static const string logsPath = "data/logs.txt";

void writeLog(const std::string &event) {
    // ensure data dir exists
    std::filesystem::create_directories("data");

    ofstream fout(logsPath, ios::app);
    if (!fout.is_open()) return;

    time_t now = time(0);
    char buf[80];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fout << buf << " - " << event << "\n";
    fout.close();
}