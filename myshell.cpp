#include <iostream>
#include <limits>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <algorithm>
#include <signal.h>

struct Prozess{
    pid_t pid;
    bool hintergrund;
    bool gestoppt;
};

std::vector<Prozess> prozesse;
pid_t vordergrund_pid = -1;

void sigchld_handler(int){
    //Zombie entfernen
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
        prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
            return p.pid == pid;
        }), prozesse.end());
        std::cout<<"Prozess "<<pid<<" beendet."<<std::endl;
    }
}

void sigtstp_handler(int){
    //mit CTRL+Z Vordergrundprozess stoppen
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

int main() {
    //Initialisierung der Signale
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);
    
    while (true) {
        //input der Befehle
        std::string input;
        std::cout << "myshell> ";
        std::getline(std::cin, input);

        //logout zum Beenden
        if (input == "logout") {
            //Überprüfen, ob Hintergrundprozesse laufen
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

        //Befehl stop
        if (input.rfind("stop ", 0) == 0){
            pid_t pid = std::stoi(input.substr(5));
            kill(pid, SIGTSTP);
            for (auto &prozess : prozesse){
                if (prozess.pid == pid) prozess.gestoppt = true;
            }
            continue;
        }

        //Befehl cont
        if (input.rfind("cont ", 0) == 0){
            pid_t pid = std::stoi(input.substr(5));
            kill(pid, SIGCONT);
            for (auto &prozess : prozesse){
                if (prozess.pid == pid){
                    prozess.gestoppt = false;
                    if (!prozess.hintergrund){
                        vordergrund_pid = pid;
                        waitpid(pid, nullptr, WUNTRACED);
                        vordergrund_pid = -1;
                    }
                }
            }
            continue;
        }

        //Befehle zerlegen und in einem Vector speichern
        // wenn & am Ende ist, dann hintergrund = true und & entfernen
        std::stringstream word(input);
        std::vector<std::string> args;
        std::string arg;
        while (word >> arg) args.push_back(arg);
        if (args.empty()) continue; //wenn kein Befehl

        bool hintergrund = false;
        if (args.back() == "&"){
            hintergrund = true;
            args.pop_back();
        }

        //execcp erwartet char* array, dann erst string -> *char
        std::vector<char*> c_args;
        for (auto &arg : args){
            c_args.push_back(arg.data());
        }
        c_args.push_back(nullptr); //mit nullptr am Ende wegen execvp

        //forken: child führt execvp aus und parent waitpid
        pid_t pid = fork();
        if (pid<0){
            std::cerr<<"Fehler"<<std::endl;
            continue;
        }
        else if(pid == 0){
            setpgid(0, 0);
            signal(SIGHUP, SIG_IGN);
            execvp(c_args[0], c_args.data());
            perror("Fehler bei execvp"); //parsing error
            exit(EXIT_FAILURE);
        }
        else{
            Prozess prozess;
            prozess.pid = pid;
            prozess.hintergrund = hintergrund;
            prozess.gestoppt = false;
            prozesse.push_back(prozess);

            if (hintergrund){
                std::cout<<"[Hintergrund: "<<pid<<"]"<<std::endl;
            }
            else{
                vordergrund_pid = pid;
                waitpid(pid, nullptr, WUNTRACED);
                vordergrund_pid = -1;
            }
        }

        //Prozessliste
        std::cout<<"[Prozesse: ";
        if (prozesse.empty()){
            std::cout<<"Keine Prozesse"<<std::endl;
        }
        for (const auto &prozess : prozesse){
            std::cout<<prozess.pid;
            if(prozess.gestoppt) std::cout<<" (STOP)";
            std::cout<<", ";
        }
        std::cout<<"]"<<std::endl;
    }   

    return 0; 
}