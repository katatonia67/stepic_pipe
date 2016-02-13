#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <sstream>
#include <algorithm>
#include <fstream>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

using namespace std;

struct Command{
	string program;
	vector<string> args;

	const vector<const char *> getArgsList(){
		
		vector<const char *> v;
		int list_size = args.size()+2; //Program name + null + args
		v.resize(list_size);

		//Program name
		v[0] = program.c_str();

		//Program arguments
		for(int i = 0; i < args.size(); i++)
			v[i+1] = args[i].c_str();

		//Last element zero
		v[list_size-1] = 0;

		return v;

	}
};

vector<Command> commands;
pid_t master_pid;

/////////////////////////
//Help string functions//
/////////////////////////

bool BothAreSpaces(char lhs, char rhs) { return (lhs == rhs) && (lhs == ' '); }

// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

////////////////////////
////////////////////////
////////////////////////

Command parseCommand(string cmd_str){

	Command cmd;

	string trimmed_string = trim(cmd_str);

	auto first_space = trimmed_string.end();
	first_space = find(trimmed_string.begin(), trimmed_string.end(), ' ');

	if (first_space == trimmed_string.end()){
		//cerr << cmd_str << endl;
		//no spaces all string is a command
		cmd.program = trim(cmd_str);
	}else{
		string left(trimmed_string.begin(), first_space);
		string right(first_space, trimmed_string.end());
		cmd.program = trim(left);
		cmd.args.push_back(trim(right));
		//cerr << cmd.program << endl;
		//cerr << cmd.args[0] << endl;
	}
	
	return cmd;
}

void run(int program_counter){
	int pfd[2];

	int pc = program_counter;

	//cerr << "Program counter " << pc << endl;

	pipe(pfd);
	
	if (fork()){		
		//parent
		
		//redirect std_out to pipe write
		
		close(STDOUT_FILENO);
		dup2(pfd[1], STDOUT_FILENO);
		close(pfd[0]);
		close(pfd[1]);
		
		Command cmd = commands[pc];
		vector<const char *> args = cmd.getArgsList();
		char *const *args_list_head = (char * const *)(&args[0]);
		execvp(cmd.program.c_str(), args_list_head);

	}else{
		//child

		//redirect std_in to pipe read
		//child_branch
		
		close(STDIN_FILENO);
		dup2(pfd[0], STDIN_FILENO);
		close(pfd[1]);
		close(pfd[0]);

		pc++;
		
		if (pc < commands.size()-1){
			run(pc);
		}	
		
		if (pc == commands.size()-1){
			//cerr << "exec last command" << endl;
			Command cmd = commands[pc];
			vector<const char *> args = cmd.getArgsList();
			char *const *args_list_head = (char * const *)(&args[0]);
			
			//Handle last child
			if (fork()){	
				//wait until last children is done
				pid_t child_pid;
				wait(&child_pid);
				//cerr << "last child done send some signal" << endl;
				kill(master_pid, SIGUSR1);
			}else{
				
				//redirect to file
				int fd_out = open("./result.out", O_RDWR | O_CREAT | O_TRUNC, 0666);
				dup2(fd_out, STDOUT_FILENO);
				close(fd_out);
				//Exec last children
				execvp(cmd.program.c_str(), args_list_head);
			}
		}
	}

	return;
}

void usr_signal_handler(int num){
	exit(0);
}

int main(int argc, char **argv){
	string input;

	//all commands
	getline(cin, input);

	//remove duplicate spaces
	//std::string::iterator new_end = std::unique(input.begin(), input.end(), BothAreSpaces);
	//input.erase(new_end, input.end());

	vector<string> commands_str  = split(input, '|');

	for(auto &c: commands_str){
		Command cmd = parseCommand(c);
		commands.push_back(cmd);
	}
	
	int commands_count = commands_str.size();

	if (commands_count == 0) return 1;
	
	if (commands_count == 1){
		Command &cmd = commands[0];
		vector<const char *> args = cmd.getArgsList();
		char *const *args_list_head = (char * const *)(&args[0]);
		execvp(cmd.program.c_str(), args_list_head);
		return 0;
	}

	if (commands_count > 1){
		//PID of master proces
		master_pid = getpid();

		//Fork to hold on proces
		if (fork()){
			//wait for USR  signal
			signal(SIGUSR1, usr_signal_handler);
			pause();
		}else{
			//Do all job
			run(0);
		}
	}
	return 0;
}
