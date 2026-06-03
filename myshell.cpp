#include <iostream>
#include <limits>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <filesystem>

/*
My-Shell: A UNIX Shell implementation in C++
1- register signals (SIGTSTP, SIGCHLD) with their handlers
2- implement signal handlers: SIGTSTP to stop foreground process anytime with CTRL+Z
   and SIGCHLD to remove zombie processes and print a message when a process ends
3- implement user input loop and print current path as prompt
4- implement command "logout" to exit the shell with error handling
5- implement command "stop <pid>" and "cont <pid>" to stop and continue processes by their PID
6- parse the user input , check if the command ends with & (background), handle errors, save it in a vector
7- implement command "cd <path>"
8- translate the vector to a C-Array for execvp
9- forking
10- print the list of processes
*/

struct Prozess{
    pid_t pid;
    bool hintergrund;
    bool gestoppt;
};

std::vector<Prozess> prozesse;
pid_t vordergrund_pid = -1;

void sigchld_handler(int){
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
        if (WIFEXITED(status) || WIFSIGNALED(status)){
            prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                return p.pid == pid;
            }), prozesse.end());
            std::cout<<"Prozess "<<pid<<" beendet."<<std::endl;
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
        std::cout<<" [SIGTSTP] an Prozess "<<vordergrund_pid<<" gesendet."<<std::endl;
        vordergrund_pid = -1;
    }
}

void sigint_handler(int){
    if (vordergrund_pid > 0){
        kill(vordergrund_pid, SIGINT);
        std::cout<<" Prozess "<<vordergrund_pid<<" mit ^C beendet."<<std::endl;
    }
}

int cd(const std::string &path){
    return chdir(path.c_str());
}



int main() {
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    while (true) {
        std::string input;
        std::string currentPath = std::filesystem::current_path().string();
        std::cout << currentPath<< "$ "<<"myshell> ";
        std::getline(std::cin, input);

        if (input == "logout") {
            bool hintergrund_da = false;
            for (const auto & prozess : prozesse){
                if (prozess.hintergrund){
                    hintergrund_da = true;
                }
            }
            if (hintergrund_da){
                std::cout<<"logout nicht möglich. Es laufen noch Prozesse im Hintergrund:"<<std::endl;
                for (const auto & prozess : prozesse){
                    if (prozess.hintergrund){
                        std::cout<<"PID: "<<prozess.pid<<std::endl;
                    }
                }
                continue;
            }

            char antwort;
            std::cout<<"Wollen Sie wirklich ausloggen? (J/N) ";
            std::cin >> antwort;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 

            bool beenden = (antwort == 'j' || antwort == 'J') ? true : false;
            if (beenden) break;  
            else continue;
        }

        if (input.rfind("stop ", 0) == 0){
            pid_t pid = std::stoi(input.substr(5));
            kill(pid, SIGTSTP);
            for (auto &prozess : prozesse){
                if (prozess.pid == pid) prozess.gestoppt = true;
            }
            continue;
        }

        if (input.rfind("cont ", 0) == 0){
            pid_t pid = std::stoi(input.substr(5));
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
                            std::cout<<"Vordergrundsprozess "<<pid<<" fertig"<<std::endl;
                        }
                        else if (WIFSIGNALED(status)){
                            int signal = WTERMSIG(status);
                            if (signal == SIGINT){
                                prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                                    return p.pid == pid;
                                }), prozesse.end());
                                std::cout<<"Vordergrundsprozess "<<pid<<" mit ^C beendet."<<std::endl;
                            }
                        }
                    }
                }
            }
            std::cout<<"[ Prozesse: ";
            if (prozesse.empty()){
                std::cout<<"Keine Prozesse";
            }
            for (const auto &prozess : prozesse){
                std::cout<<prozess.pid;
                if(prozess.gestoppt) std::cout<<" (STOP)";
                std::cout<<" ";
            }
            std::cout<<" ]"<<std::endl;
            continue;
        }

        std::stringstream word(input);
        std::vector<std::string> args;
        std::string arg;
        while (word >> arg) args.push_back(arg);
        if (args.empty()) continue; 

        bool hintergrund = false;
        if (args.back() == "&"){
            hintergrund = true;
            args.pop_back();
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
            std::cerr<<"Fehler"<<std::endl;
            continue;
        }
        else if(pid == 0){
            setpgid(0, 0);
            signal(SIGHUP, SIG_IGN);
            execvp(c_args[0], c_args.data());
            perror("Fehler bei execvp"); 
            exit(EXIT_FAILURE);
        }
        else{
            Prozess prozess;
            prozess.pid = pid;
            prozess.hintergrund = hintergrund;
            prozess.gestoppt = false;
            prozesse.push_back(prozess);

            if (hintergrund){
                std::cout<<"[ Hintergrund: "<<pid<<" ]"<<std::endl;
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
                    std::cout<<"Vordergrundsprozess "<<pid<<" fertig"<<std::endl;
                }
                else if (WIFSIGNALED(status)){
                    int signal = WTERMSIG(status);
                    if (signal == SIGINT){
                        prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                        return p.pid == pid;
                    }), prozesse.end());
                    std::cout<<"Vordergrundsprozess "<<pid<<" mit ^C beendet."<<std::endl;
                    }
                }
            }
        }

        std::cout<<"[ Prozesse: ";
        if (prozesse.empty()){
            std::cout<<"Keine Prozesse";
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