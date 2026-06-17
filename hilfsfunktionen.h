#ifndef HILFSFUNKTIONEN_H
#define HILFSFUNKTIONEN_H

#include <vector>
#include <iostream>
#include <limits>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <algorithm>
#include <signal.h>
#include <filesystem>
#include <setjmp.h>

struct Segment{
    std::vector<std::string> args;
    std::string op;
};

struct Prozess{
    pid_t pid;
    bool hintergrund;
    bool gestoppt;
};

int cd(const std::string &path);

void executeSegments(const std::vector<Segment> &segments,
                    std::vector<Prozess> &prozesse,
                    int &last_exit_status,
                    bool &logout_request,
                    pid_t &vordergrund_pid);

#endif // HILFSFUNKTIONEN_H