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
4- implement command "logout" to exit the shell with error handling
5- implement command "stop <pid>" and "cont <pid>" to stop and continue processes by their PID
6- parse the user input , check if the command ends with & (background), handle errors, save it in a vector
7- implement command "cd <path>"
8- translate the vector to a C-Array for execvp
9- forking
10- print the list of background processes and stopped processes
*/

struct Prozess{
    pid_t pid;
    bool hintergrund;
    bool gestoppt;
};

std::vector<Prozess> prozesse;
pid_t vordergrund_pid = -1;
static sigjmp_buf env;
static volatile sig_atomic_t jump_active = 0;

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
        std::cout<<" Process "<<vordergrund_pid<<" terminated with ^C."<<std::endl;
    }
    if (!jump_active) return;
    siglongjmp(env, 42);
}

int cd(const std::string &path){
    return chdir(path.c_str());
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
        std::cout << currentPath<< "$ "<<"harishell> ";
        if (!std::getline(std::cin, input)){
            std::cout<<std::endl;
            break;
        }

        if (input == "logout") {
            bool hintergrund_da = false;
            for (const auto & prozess : prozesse){
                if (prozess.hintergrund){
                    hintergrund_da = true;
                }
            }
            if (hintergrund_da){
                std::cout<<"logout not possible. There are still processes running in the background"<<std::endl;
                for (const auto & prozess : prozesse){
                    if (prozess.hintergrund){
                        std::cout<<"[ Background: "<<prozess.pid<<" ]"<<std::endl;
                    }
                }
                continue;
            }

            char antwort;
            std::cout<<"Do you really want to logout? (y/n) ";
            std::cin >> antwort;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 

            bool beenden = (antwort == 'y' || antwort == 'Y') ? true : false;
            if (beenden) break;  
            else continue;
        }

        std::stringstream word(input);
        std::vector<std::string> args;
        std::string arg;
        while (word >> arg) args.push_back(arg);
        if (args.empty()) continue;

        std::vector<std::string> merged;
        for (size_t i = 0; i<args.size(); i++){
            if (args[i].front() == '\'' || args[i].front() == '"'){
                char q = args[i].front();
                std::string combined = args[i].substr(1);
                if (!combined.empty() && combined.back() == q){
                    combined.pop_back();
                }
                else{
                    while (++i < args.size()){
                        combined += " " + args[i];
                        if (args[i].back() == q){
                            combined.pop_back();
                            break;
                        }
                    }
                }
                merged.push_back(combined);
            }
            else{
                merged.push_back(args[i]);
            }
        }
        args = merged;

        bool hintergrund = false;
        if (args.back() == "&"){
            hintergrund = true;
            args.pop_back();
        }

        if (args[0] == "stop"){
            pid_t pid = std::stoi(args[1]);
            kill(pid, SIGTSTP);
            for (auto &prozess : prozesse){
                if (prozess.pid == pid) prozess.gestoppt = true;
            }
            std::cout<<" Process "<<pid<<" stopped."<<std::endl;
            std::cout<<"[ Background and stopped processes: ";
            if (prozesse.empty()){
                std::cout<<"No processes";
            }
            for (const auto &prozess : prozesse){
                std::cout<<prozess.pid;
                if(prozess.gestoppt) std::cout<<" (STOP)";
                std::cout<<" ";
            }
            std::cout<<"]"<<std::endl;
            continue;
        }

        if (args[0] == "cont"){
            pid_t pid = std::stoi(args[1]);
            kill(pid, SIGCONT);
            for (auto &prozess : prozesse){
                if (prozess.pid == pid){
                    prozess.gestoppt = false;
                    if (!prozess.hintergrund){
                        vordergrund_pid = pid;
                        int status;
                        waitpid(pid, &status, WUNTRACED);
                        vordergrund_pid = -1;
                        if (WIFEXITED(status)){
                            prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                                return p.pid == pid;
                            }), prozesse.end());
                            std::cout<<"Foreground process "<<pid<<" finished."<<std::endl;
                        }
                        else if (WIFSIGNALED(status)){
                            int signal = WTERMSIG(status);
                            if (signal == SIGINT){
                                prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                                    return p.pid == pid;
                                }), prozesse.end());
                                std::cout<<"Foreground process "<<pid<<" terminated with ^C."<<std::endl;
                            }
                        }
                    }
                }
            }
            std::cout<<"[ Background and stopped processes: ";
            if (prozesse.empty()){
                std::cout<<"No processes";
            }
            for (const auto &prozess : prozesse){
                std::cout<<prozess.pid;
                if(prozess.gestoppt) std::cout<<" (STOP)";
                std::cout<<" ";
            }
            std::cout<<"]"<<std::endl;
            continue;
        }

        if (args[0] == "cd"){
            if (args.size() < 2){
                std::cerr<<"Error: No path provided for cd"<<std::endl;
            }
            else{
                if (cd(args[1]) < 0){
                    perror(args[1].c_str());
                }
            }
            continue;
        }

        std::vector<char*> c_args;
        for (auto &arg : args){
            c_args.push_back(arg.data());
        }
        c_args.push_back(nullptr); 

        pid_t pid = fork();
        if (pid<0){
            std::cerr<<"Forking failed"<<std::endl;
            continue;
        }
        else if(pid == 0){
            setpgid(0, 0);
            signal(SIGHUP, SIG_IGN);
            execvp(c_args[0], c_args.data());
            perror("execvp failed"); 
            exit(EXIT_FAILURE);
        }
        else{
            Prozess prozess;
            prozess.pid = pid;
            prozess.hintergrund = hintergrund;
            prozess.gestoppt = false;
            prozesse.push_back(prozess);

            if (hintergrund){
                std::cout<<"Process "<<pid<<" started in the background."<<std::endl;
            }
            else{
                vordergrund_pid = pid;
                int status;
                waitpid(pid, &status, WUNTRACED);
                vordergrund_pid = -1;
                if (WIFEXITED(status)){
                    prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                        return p.pid == pid;
                    }), prozesse.end());
                    std::cout<<"Foreground process "<<pid<<" finished."<<std::endl;
                }
                else if (WIFSIGNALED(status)){
                    int signal = WTERMSIG(status);
                    if (signal == SIGINT){
                        prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                        return p.pid == pid;
                    }), prozesse.end());
                    std::cout<<"Foreground process "<<pid<<" terminated with ^C."<<std::endl;
                    }
                }
            }
        }

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