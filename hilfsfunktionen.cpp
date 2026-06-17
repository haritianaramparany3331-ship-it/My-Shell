#include "hilfsfunktionen.h"

int cd(const std::string &path){
    return chdir(path.c_str());
}

void executeSegments(const std::vector<Segment> &segments,
                    std::vector<Prozess> &prozesse,
                    int &last_exit_status,
                    bool &logout_request,
                    pid_t &vordergrund_pid){
    std::string last_operator = "";
        for (auto &segment : segments){
            bool invert = false;
            bool run_command = false;
            if (last_operator == "") run_command = true;
            else if (last_operator == ";") run_command = true;
            else if (last_operator == "&&") run_command = (last_exit_status == 0);
            else if (last_operator == "||") run_command = (last_exit_status != 0);
            else run_command = false;

            if (run_command){
                std::vector<std::string> args = segment.args;

                if (args[0] == "!"){
                    invert = true;
                    args.erase(args.begin());
                }

                bool hintergrund = false;
                if (args.back() == "&"){
                    hintergrund = true;
                    args.pop_back();
                }
                
                if (args[0] == "logout") {
                    bool hintergrund_da = false;
                    for (const auto & prozess : prozesse){
                        if (prozess.hintergrund){
                            hintergrund_da = true;
                            break;
                        }
                    }
                    if (hintergrund_da){
                        std::cout<<"logout not possible. There are still processes running in the background"<<std::endl;
                        last_exit_status = 1;
                        for (const auto & prozess : prozesse){
                            if (prozess.hintergrund){
                                std::cout<<"[ Background: "<<prozess.pid<<" ]"<<std::endl;
                                last_operator = segment.op;
                                continue;
                            }
                        }
                    }
                    else{
                        char antwort;
                        std::cout<<"Do you really want to logout? (y/n) ";
                        std::cin >> antwort;
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 

                        bool beenden = (antwort == 'y' || antwort == 'Y') ? true : false;
                        if (beenden) {
                            logout_request = true;  
                            break;
                        }
                        else break;
                    }
                }

                if (args[0] == "stop"){
                    pid_t pid = std::stoi(args[1]);
                    kill(pid, SIGTSTP);
                    for (auto &prozess : prozesse){
                        if (prozess.pid == pid) prozess.gestoppt = true;
                    }
                    std::cout<<" Process "<<pid<<" stopped."<<std::endl;
                    last_operator = segment.op;
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
                                    last_exit_status = WEXITSTATUS(status);
                                    prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                                        return p.pid == pid;
                                    }), prozesse.end());
                                    std::cout<<"Foreground process "<<pid<<" finished."<<std::endl;
                                }
                                else if (WIFSIGNALED(status)){
                                    last_exit_status = 1;
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
                    last_operator = segment.op;
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
                    last_operator = segment.op;
                    continue;
                }

                if (args[0] == "(" && args.back() == ")"){
                    std::vector<std::string> inner(args.begin()+1, args.end()-1);
                    std::vector<Segment> inner_segment;
                    std::vector<std::string> currentSeg;
                    int depth = 0;
                    for (const auto &token : inner){
                        if (token == "("){
                            depth++;
                            currentSeg.push_back(token);
                        }
                        else if (token == ")"){
                            depth--;
                            currentSeg.push_back(token);
                        }
                        else if ((token == ";" || token == "&&" || token == "||") && depth == 0){
                            if (!currentSeg.empty()){
                                inner_segment.push_back({currentSeg, token});
                                currentSeg.clear();
                            }
                        }
                        else{
                            currentSeg.push_back(token);
                        }
                    }
                    if (!currentSeg.empty()){
                        inner_segment.push_back({currentSeg, ""});
                    }

                    pid_t pid = fork();
                    if (pid < 0){
                        std::cerr<<"Forking error"<<std::endl;
                        last_exit_status = 1;
                        if (invert) last_exit_status = (last_exit_status==0) ? 1 : 0;
                        last_operator = segment.op;
                        continue;
                    }
                    else if (pid == 0){
                        if (hintergrund) signal(SIGHUP, SIG_IGN);
                        executeSegments(inner_segment,
                                        prozesse,
                                        last_exit_status,
                                        logout_request,
                                        vordergrund_pid);
                        exit(last_exit_status);
                    }
                    else{
                        if (hintergrund){
                            prozesse.push_back({pid, true, false});
                            std::cout<<"Process "<<pid<<" started in the background."<<std::endl;
                        }
                        else{
                            vordergrund_pid = pid;
                            int status;
                            waitpid(pid, &status, WUNTRACED);
                            vordergrund_pid = -1;
                            if (WIFEXITED(status)){
                                last_exit_status = WEXITSTATUS(status);
                                std::cout<<"Foreground process "<<pid<<" finished."<<std::endl;
                            }
                            else if (WIFSIGNALED(status)){
                                last_exit_status = 1;
                                int signal = WTERMSIG(status);
                                if (signal == SIGINT){
                                    std::cout<<"Foreground process "<<pid<<" terminated with ^C."<<std::endl;
                                }
                            }
                        }
                    }

                    if (invert) last_exit_status = (last_exit_status==0) ? 1 : 0;
                    last_operator = segment.op;
                    continue;
                }

                std::vector<char*> c_args;
                for (auto &arg : args){
                    c_args.push_back(arg.data());
                }
                c_args.push_back(nullptr);

                if (args[0] == "exec"){
                    execvp(c_args[1], c_args.data() + 1);
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                    last_operator = segment.op;
                    continue;
                }

                pid_t pid = fork();
                if (pid<0){
                    std::cerr<<"Forking failed"<<std::endl;
                    continue;
                }
                else if(pid == 0){
                    setpgid(0, 0);
                    if (hintergrund) signal(SIGHUP, SIG_IGN);
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
                            last_exit_status = WEXITSTATUS(status);
                            prozesse.erase(std::remove_if(prozesse.begin(), prozesse.end(), [pid](const Prozess &p){
                                return p.pid == pid;
                            }), prozesse.end());
                            std::cout<<"Foreground process "<<pid<<" finished."<<std::endl;
                        }
                        else if (WIFSIGNALED(status)){
                            last_exit_status = 1;
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

            if (invert) last_exit_status = (last_exit_status == 0) ? 1 : 0;  
            last_operator = segment.op;
        }

}