#include "hilfsfunktionen.h"
#include <fcntl.h>

int cd(const std::string &path){
    return chdir(path.c_str());
}

void executeSegments(const std::vector<Segment> &segments,
                    std::vector<Prozess> &prozesse,
                    int &last_exit_status,
                    bool &logout_request,
                    pid_t &vordergrund_pid){
    std::string last_operator = "";
    int prev_read = -1;
        for (auto &segment : segments){
            bool invert = false;
            bool run_command = false;
            if (last_operator == "") run_command = true;
            else if (last_operator == ";") run_command = true;
            else if (last_operator == "&&") run_command = (last_exit_status == 0);
            else if (last_operator == "||") run_command = (last_exit_status != 0);
            else if (last_operator == "|") run_command = true;
            else run_command = false;

            if (run_command){
                std::vector<std::string> args = segment.args;

                std::vector<Redirection> redirections;
                std::vector<std::string> clean_args;
                for (size_t i = 0; i<args.size(); i++){
                    const std::string &tok = args[i];
                    size_t j = 0;
                    while (j<tok.size() && isdigit(tok[j])) j++;
                    std::string rest = tok.substr(j);
                    int explizit_fd = (j>0) ? std::stoi(tok.substr(0, j)) : -1;

                    std::string op = "";
                    std::string inline_target = "";
                    for (const std::string &o : {"<>", "<&", ">&", ">>", "<", ">"}){
                        if (rest.substr(0, o.size()) == o){
                            op = o;
                            inline_target = rest.substr(o.size());
                            break;
                        }
                    }

                    if (!op.empty()){
                        std::string target = (!inline_target.empty()) ? inline_target : args[++i];
                        int default_fd = (op == ">" || op == ">&" || op == ">>") ? 1 : 0;
                        int new_fd = (explizit_fd != -1) ? explizit_fd : default_fd;
                        redirections.push_back({new_fd, op, target});
                    }
                    else{
                        clean_args.push_back(tok);
                    }
                }
                args = clean_args;

                int pipefd[2] = {-1, -1};
                if (segment.op == "|"){
                    pipe(pipefd);
                }

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
                    if (prev_read != -1){
                        dup2(prev_read, STDIN_FILENO);
                        close(prev_read);
                    }
                    if (pipefd[1] != -1){
                        close(pipefd[0]);
                        dup2(pipefd[1], STDOUT_FILENO);
                        close(pipefd[1]);
                    }

                    for (const auto &redir : redirections){
                        if (redir.op == "<&" || redir.op == ">&"){
                            if (redir.target == "-") close(redir.fd);
                            else{
                                dup2(std::stoi(redir.target), redir.fd);
                                continue;
                            } 
                        }

                        int flags = redir.op == ">" ? O_CREAT | O_TRUNC  | O_WRONLY
                                    : redir.op == "<" ? O_RDONLY
                                    : redir.op == ">>" ? O_CREAT | O_APPEND | O_WRONLY
                                    :                    O_CREAT | O_RDWR;
                        int new_fd = open(redir.target.c_str(), flags, 0644);
                        if (new_fd < 0){
                            perror(redir.target.c_str());
                            exit(EXIT_FAILURE);
                        }
                        dup2(new_fd, redir.fd);
                        close(new_fd);
                    }
                    execvp(c_args[0], c_args.data());
                    perror("execvp failed");
                    exit(EXIT_FAILURE);
                }
                else{
                    if (prev_read != -1) close(prev_read);
                    Prozess prozess;
                    prozess.pid = pid;
                    prozess.hintergrund = hintergrund;
                    prozess.gestoppt = false;
                    prozesse.push_back(prozess);

                    if (hintergrund){
                        std::cout<<"Process "<<pid<<" started in the background."<<std::endl;
                    }
                    else{
                        if (segment.op == "|"){
                            close(pipefd[1]);
                            prev_read = pipefd[0];
                        }
                        else{
                            prev_read = -1;
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
            }

            if (invert) last_exit_status = (last_exit_status == 0) ? 1 : 0;  
            last_operator = segment.op;
        }
}