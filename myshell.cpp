#include <iostream>
#include <limits>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <filesystem>
#include "hilfsfunktionen.h"
#include <setjmp.h>

/*
My-Shell: A UNIX Shell implementation in C++
1- register signals (SIGTSTP, SIGCHLD, SIGINT) with their handlers
2- implement signal handlers: SIGTSTP to stop foreground process anytime with CTRL+Z,
   SIGCHLD to remove zombie processes and print a message when a process ends
   and SIGINT to end foreground process with CTRL+C
2- the functions sigsetjmp and siglongjmp are used to return to the main loop after receiving signals without an input,
   the jump has to be set before siglongjmp is called, therefore a bool flag jump_active
3- implement user input loop and print current path as prompt
3- implement parsing of user input into arguments, handle quoted strings as single arguments
6- parse the user input , check if the command ends with & (background), handle errors, save it in a vector
6- check if the first argument is "!" to invert the exit status of the command
3- implement parsing of user input into several commands separated by " ", ;, && and ||
4- implement command "logout" to exit the shell with error handling
5- implement command "stop <pid>" and "cont <pid>" to stop and continue processes by their PID
7- implement command "cd <path>"
8- translate the vector to a C-Array for execvp
9- forking
10- print the list of background processes and stopped processes
*/

std::vector<Prozess> prozesse;
pid_t vordergrund_pid = -1;
static sigjmp_buf env;
static volatile sig_atomic_t jump_active = 0;
int last_exit_status = 0;
bool logout_request = false;

void sigchld_handler(int){
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
        if (WIFEXITED(status) || WIFSIGNALED(status)){
            prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                return p.pid == pid;
            }), prozesse.end());
            std::cout<<"Process "<<pid<<" finished."<<std::endl;
        }
    }
}

void sigtstp_handler(int){
    if (vordergrund_pid > 0){
        kill(vordergrund_pid, SIGTSTP);
        for (auto &prozess : prozesse){
            if (prozess.pid == vordergrund_pid){
                prozess.gestoppt = true;
            }
        }
        std::cout<<" Process "<<vordergrund_pid<<" stopped with ^Z."<<std::endl;
        vordergrund_pid = -1;
        return;
    }
    if (!jump_active) return;
    siglongjmp(env, 42);
}

void sigint_handler(int){
    if (vordergrund_pid > 0){
        kill(vordergrund_pid, SIGINT);
        std::cout<<" Process "<<vordergrund_pid<<" terminated with ^C.";
    }
    if (!jump_active) return;
    siglongjmp(env, 42);
}

int main() {
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    while (true) {
        if (sigsetjmp(env, 1) == 42){
            std::cout<<std::endl;
            continue;   
        }
        jump_active = 1;
        std::string input;
        std::string currentPath = std::filesystem::current_path().string();
        std::cout << (last_exit_status != 0 ? "\033[31m" : "") << currentPath << "$ " << "harishell> " << "\033[0m";
        if (!std::getline(std::cin, input)){
            std::cout<<std::endl;
            break;
        }

        while (true){
            std::string firstLine = input;
            if (!firstLine.empty() && firstLine.back() == ' '){
                firstLine.pop_back();
            }
            if (firstLine.empty() || firstLine.back() != '\\') break;
            input = firstLine.substr(0, firstLine.size() - 1);
            std::cout<<"> ";
            std::string nextLine;
            if (!std::getline(std::cin, nextLine)) break;
            input += " " + nextLine;
        }

        std::stringstream word(input);
        std::vector<std::string> zeile;
        std::string arg;
        while (word >> arg) zeile.push_back(arg);
        if (zeile.empty()) continue;

        std::vector<std::string> merged;
        for (size_t i = 0; i<zeile.size(); i++){
            if (zeile[i].front() == '\'' || zeile[i].front() == '"'){
                char q = zeile[i].front();
                std::string combined = zeile[i].substr(1);
                if (!combined.empty() && combined.back() == q){
                    combined.pop_back();
                }
                else{
                    while (++i < zeile.size()){
                        combined += " " + zeile[i];
                        if (zeile[i].back() == q){
                            combined.pop_back();
                            break;
                        }
                    }
                }
                merged.push_back(combined);
            }
            else{
                merged.push_back(zeile[i]);
            }
        }
        zeile = merged;

        std::vector<Segment> segments;
        std::vector<std::string> current_segment;
        int parent_depth = 0;
        for (const auto &token : zeile){
            if (token == "("){
                parent_depth++;
                current_segment.push_back(token);
            }
            else if (token == ")"){
                parent_depth--;
                current_segment.push_back(token);
            }
            else if ((token == ";" || token == "&&" || token == "||") && parent_depth == 0){
                if (!current_segment.empty()){
                    segments.push_back({current_segment, token});
                    current_segment.clear();
                }
            }
            else{
                current_segment.push_back(token);
            }
        }
        if (!current_segment.empty()){
            segments.push_back({current_segment, ""});
        }

        executeSegments(segments,
                        prozesse,
                        last_exit_status,
                        logout_request,
                        vordergrund_pid);

        if (logout_request) break;

        std::cout<<"[ Background and stopped processes: ";
        if (prozesse.empty()){
            std::cout<<"No processes";
        }
        for (const auto &prozess : prozesse){
            std::cout<<prozess.pid;
            if(prozess.gestoppt) std::cout<<" (STOP)";
            std::cout<<" ";
        }
        std::cout<<" ]"<<std::endl;
    }   

    return 0; 
}