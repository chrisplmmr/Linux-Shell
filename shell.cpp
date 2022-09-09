// Written by Christopher Plummer
// Help from Dr. Tanzir Ahmed's lecture videos and slides
// Help from Feras lab videos
// Help from wen lab videos
// Web sources listed in design report
// CSCE 313-512

#include <iostream>
#include <fstream>
#include <ostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <vector>
#include <string>
#include <ctime>
#include <iomanip>

using namespace std;

// File I/O implemented with file descriptiors
// Piping implemented with dup2

vector<string> split (string cmdLine, char separator){   
    
    vector<string> commands;
    int pos = 0;
    int prev = 0;
    
    //Check for no separators to begin with
    if(cmdLine.substr(prev).find(separator) == std::string::npos){
        commands.push_back(cmdLine);
    }
    
    while(cmdLine.substr(prev).find(separator) != std::string::npos){
        pos = cmdLine.substr(prev).find(separator);
        commands.push_back(cmdLine.substr(prev, pos));
        pos = pos+prev+1;
        prev = pos;
        
        if (cmdLine.substr(prev).find(separator) == std::string::npos){ 
            commands.push_back(cmdLine.substr(prev));
        }    
    }
    
    for(int i = 0; i < commands.size(); i++){
        if (commands[i] == ""){
            commands.erase(commands.begin()+i);
            --i;
        }
    }

    //Look for quotes. Manipulate the vector accordinginly
    string copy = "";
    string d = "";
    size_t found = 0;
    
    for(int i = 0; i < commands.size(); i++){
        found = 0;
        if((found = commands[i].find('\'')) != std::string::npos || (found = commands[i].find('\"')) != std::string::npos){ //found a quote, find second, copt, append, and delete.
            //cout << "i = " << i << endl;
            for(int p = i; p < commands.size(); p++){
                if(commands[p].find('\'', found+1) != std::string::npos || commands[p].find('\"', found+1) != std::string::npos){
                    //cout << "p = " << p << endl;
                    //Copy all piece inbetween first quote and second quote and append them
                    if(p == i){
                        break;
                    } else {
                        for(int piece = i+1; piece <= p; piece++){
                            copy = commands[piece]; 
                            //cout << "piece = " << piece << " copy = " << copy << endl;
                            string d(1, separator);
                            commands[i].append(d);
                            commands[i].append(copy);
                            commands.erase(commands.begin()+piece);
                            piece--;
                            p--;
                        }    
                    }
                    break;
                }
            }
        }   
    }
    
    return commands;
}

char** vec_to_char_array(vector<string>& commands){

    char** result = new char * [commands.size() + 1];
    for(int i = 0; i < commands.size(); i++){
        result[i] = (char *)commands[i].c_str();  
    }
    result[commands.size()] = NULL;
    return result;
}

// Trim spaced off front and back of commands
string trim (string input){
    
    string result = input;
    
    //Trim front
    for(int i = 0; i < result.size(); i++){
        if(result[i] == ' '){
            result = result.substr(i+1);
            i--;
        } else {
            break;
        }
    }
    
    //Trim back
    for(int i = result.size()-1; i > 0; i--){
        if(result[i] == ' '){
            result = result.substr(0, i);
        } else {
            break;
        }
    }
    
    return result;
}

// Find quotes within a command
int findq(string input){

    //cout << "Input: " << input << endl;
    int position = -1;

    if(input.find('\'') != std::string::npos){
        position = input.find('\'');
    } else if (input.find('\"') != std::string::npos){
        position = input.find('\"');
    }

    return position;
}

vector<string> removeAllQ(vector<string> input){
    
    vector<string> result;
    size_t found = 0;
    for(int i = 0; i < input.size(); i++){
        if(input[i].find('\'') == 0 || input[i].find('\"') == 0){
            input[i].replace(0, 1, "");
        }
        if(input[i].find('\'') == input[i].length()-1 || input[i].find('\"') == input[i].length()-1){
            input[i].replace(input[i].length()-1, 1, "");
        }
    }
    return input;
}

int main (){

    int kb = dup(0); // Reset file descriptor for kb.
    int terminal = dup(1); // Reset file descriptor for terminal.

    vector<int> bg_pid_list;

    char buf[1024];
    string lastdirectory = getcwd(buf, sizeof(buf));
    bool inquotes;

    while (true){

        // Check for zombie pids
        for (int i=0; i < bg_pid_list.size(); i++){
           if(waitpid (bg_pid_list[i], 0, WNOHANG) == bg_pid_list[i]){
               cout << "Process: " << bg_pid_list[i] << " erased." << endl;
                bg_pid_list.erase(bg_pid_list.begin() + i);
                i--;
           }
        }

        //Prompt for shell ( ex: chris Mon Oct 15 15:27:04 2020$ )
        char *usr=getenv("USER");
        std::string user(usr);

        time_t now = time(0);
        char* dt = ctime(&now);
        std::string datetime(dt);
        datetime.erase(datetime.length()-1, 1);
        
        cout << user << " "<< datetime << "$ ";
        
        //Get command from user
        string inputline;
        dup2(kb,0);             // Redirect keyboard to stdin
        dup2(terminal, 1);      // Redirect terminal to stdout
        getline(cin, inputline);

        if(inputline == string("exit")){
            cout << "Bye!! End of shell..." << endl;
            break;
        }

        // Split command line into separate commands to be passed to individual processes.
        vector<string> levels = split(inputline, '|'); // "ls | grep soda"

        // Loop through each command, forking a separate process for it.
        for(int i = 0; i < levels.size(); i++){

            levels[i] = trim(levels[i]);
            
            bool bg = false; //Check to see if command is a background process
            if(levels[i][levels[i].size()-1] == '&'){
                cout << "- BG process found - " << endl;
                bg = true;
                levels[i] = levels[i].substr(0, levels[i].size()-1);
            }

            // Some commands are not able to run with exec.

            if(levels[i].find("cd")==0){ // Check for cs command

                string dirname = trim(split(levels[i], ' ')[1]); //cd dirname
                if(dirname == "-"){
                    chdir(lastdirectory.c_str());
                    continue;
                } else {
                    char cwd[1024];
                    lastdirectory = getcwd(cwd, sizeof(cwd));
                    chdir(dirname.c_str());
                    continue;
                }
            }

            int fdp[2]; //Create a pipe to pass result to next process. (IPC method: unnamed pipe)
            pipe (fdp);
            int pid = fork();
            
            if(!pid){ //Child process

                if(levels[i].find("pwd")==0){ // Check for pwd command
                    char cwd[1024];
                    getcwd(cwd, sizeof(cwd));
                    printf("%s", cwd);
                }

                // Check for write to file symbol
                int pos = levels[i].find('>');
                int qpos = findq(levels[i]);
                int qpos2 = findq(levels[i].substr(qpos+1));
                if(pos>=0 && (pos < qpos || pos > qpos2)){
                    //levels[i] = trim(levels[i]); already "ls > a.txt"
                    string command = trim(levels[i].substr(0,pos)); // ls
                    string filename = trim(levels[i].substr(pos+1)); // a.txt

                    levels[i] = command;
                    int fd = open(filename.c_str(), O_WRONLY|O_CREAT, S_IWUSR|S_IRUSR);
                    dup2(fd, 1); //STDOUT REDIRECT
                    close(fd);
                }

                // Check for read to file symbol
                pos = levels[i].find('<');
                if(pos>=0 && (pos < qpos || pos > qpos2)){
                    //levels[i] = trim(levels[i]); already "ls > a.txt"
                    string command = trim(levels[i].substr(0,pos));
                    string filename = trim(levels[i].substr(pos+1));

                    levels[i] = command;
                    int fd = open(filename.c_str(), O_RDONLY|O_CREAT, S_IWUSR|S_IRUSR);
                    dup2(fd, 0); //STDIN REDIRECT
                    close(fd);
                }

                if(i < levels.size()-1){ dup2(fdp[1], 1); } //Redirect stdout to pipeout for this process
                

                vector<string> fullformat = split(levels[i], ' ');
                fullformat = removeAllQ(fullformat);
                vector<string> parts = fullformat;

                char** args = vec_to_char_array(parts);
                execvp(args[0], args); // Pass command to execvp to be handled by kernel

            } else {
                
                if(!bg){
                    if(i==levels.size()-1) waitpid (pid, 0, 0); // Wait for child process of parent to finish before moving onto next level
                }
                
                if(bg){ 
                    bg_pid_list.push_back(pid); // If bg, dont wait for it, but keep track of it by pushing it to a vector
                }
                
                // Get input from child process
                dup2(fdp[0], 0); // Redirect stdin to pipe in
                close (fdp[1]);   
            }

        } // end of for loop

    } // end of while

} //end of main
