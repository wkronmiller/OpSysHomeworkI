/*
Homework I, Operating Systems
Fall, 2014
William Rory Kronmiller
RIN: 661033063
*/

//Included Header Files
#include <stdlib.h> //ISO Standard General Functions
#include <stdio.h> //ISO Standard Input/Output Functions
#include <string.h> //ISO Standard String Functions
#include <stdbool.h> //ISO Standard Boolean Values
#include <unistd.h> //POSIX Standard Symbolic Constants
#include <dirent.h> 
#include <sys/types.h> //POSIX Standard Primitive System Datatypes
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

//Pre-Processor Definitions
#define PATHVAR "MYPATH"
#define DEFAULT_PATH "/bin:."
//#define TEST_PATHS "MYPATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games"

#define UI_MAX_LEN 999

#define IN_LOOP true

//Parses path environment variable
int load_path(char *** p_binary_dirs)
{
	//Variable Declarations
	char ** binary_dirs, *env_list, *env_list_buffer, *s_buffer;
	int token_count;

	//Try to load path
	env_list_buffer = getenv(PATHVAR);

	//Check if path exists, otherwise load defaults
	if (env_list_buffer == NULL)
	{
		//Allocat memory for environment list
		if ((env_list = calloc(strlen(DEFAULT_PATH) + 1, sizeof(char))) == NULL)
		{
			perror("ERROR: memory allocation failure!");
			exit(EXIT_FAILURE);
		}

		//Copy in default path
		strncpy(env_list, DEFAULT_PATH, strlen(DEFAULT_PATH));
		env_list[strlen(DEFAULT_PATH)] = 0; //Ensure null-termination
	}
	else
	{
		//Allocate memory for environment list
		if ((env_list = calloc(strlen(env_list_buffer) + 1, sizeof(char))) == NULL)
		{
			perror("ERROR: memory allocation failure!");
			exit(EXIT_FAILURE);
		}

		//Copy in environment paths
		strncpy(env_list, env_list_buffer, strlen(env_list_buffer));
		env_list[strlen(env_list_buffer)] = 0;
	}

	//Tokenize env_list
	s_buffer = env_list_buffer = strsep(&env_list, ":");

	//Initialize binary_dirs array
	if ((binary_dirs = calloc(101, sizeof(char*))) == NULL)
	{
		perror("ERROR: memory allocation failure!");
		exit(EXIT_FAILURE);
	}

	//Allocate memory for elements of binary_dirs array
	for (token_count = 0; token_count < 100; token_count++)
	{
		//Allocate memory
		if ((binary_dirs[token_count] = malloc(sizeof(char)* 255)) == NULL)
		{
			perror("ERROR: memory allocation failure!");
			exit(EXIT_FAILURE);
		}
	}

	//Initialize token count
	token_count = 0;
	
	//Copy over list of paths
	while ((env_list_buffer != NULL) && (token_count < 100))
	{
		//Copy string
		strncpy(binary_dirs[token_count], env_list_buffer, strlen(env_list_buffer));
		binary_dirs[token_count][strlen(env_list_buffer)] = 0; //Null-termination

		//Get next token
		env_list_buffer = strsep(&env_list, ":");

		//Increment counter
		token_count++;
	}
	
	//Update pointer sent to function
	*p_binary_dirs = binary_dirs;

	//Cleanup
	free(s_buffer);

	return token_count;

}

//Checks if user-requested program exists
char * find_executable(char ** binary_dirs, char * exe_name)
{
	//Variable declarations
	char file_path[300]; //Maximum length of file name in Linux ext* system is 255
	int dir_index;
	struct stat stat_buf;

	//Loop through directory list
	for (dir_index = 0; binary_dirs[dir_index] != 0; dir_index++)
	{

		//Generate full file path
		if(sprintf(file_path, "%s/%s", binary_dirs[dir_index], exe_name) < 0)
		{
			perror("ERROR: String concatenation failure!");
			exit(EXIT_FAILURE);
		}

		//Use lstat to find file
		if(lstat(file_path, &stat_buf) == -1)
		{
			//File not found
			continue;
		}

		if((S_ISREG(stat_buf.st_mode)) && (stat_buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		{
			//File found and is executable
			return binary_dirs[dir_index];
		}
	}

	//Could not find program
	return NULL;
}

//Executes user-requested program (if it exists)
void execute_command(
	char ** command, //Command to execute and args
	int input_fd, //Input file descriptor
	int output_fd, //Output file descriptor
	bool wait_on_child, //Wait for child process to exit, or continue
	int * child_count
	)
{
	//Variable declarations
	pid_t child_pid;
	char **binary_dirs, *exe_dir, *command_string;
	int child_retval,
		binary_dirs_siz;

	//Initialize vars
	child_retval = -1;

	//Load binary paths
	binary_dirs_siz = load_path(&binary_dirs);

	//Find directory in which exe resides
	if ((exe_dir = find_executable(binary_dirs, command[0])) == NULL)
	{
		//Error-handling
		fprintf(stderr, "ERROR: command not found\n");
		goto execute_cleanup;
	}

	//Fork and execute
	child_pid = fork();
	
	//Error-checking
	if (child_pid == -1)
	{
		perror("ERROR: fork failed");
		return;
	}

	//Determine who is the parent and who is the child
	if (child_pid == 0) //Child process
	{
		//Change file descriptors
		if (input_fd != STDIN_FILENO)
		{
			fclose(stdin);
			if (dup2(input_fd, STDIN_FILENO) == -1)
			{
				perror("Stdin redirect failed");
				fprintf(stderr, "Error number: %u\n", errno);
				exit(EXIT_FAILURE);
			}
		}
		if (output_fd != STDOUT_FILENO)
		{
			fclose(stdout);
			if (dup2(output_fd, STDOUT_FILENO) == -1)
			{
				perror("Stdout redirect failed");
				fprintf(stderr, "Error number: %u\n", errno);
				exit(EXIT_FAILURE);
			}
		}

		fflush(stdout);

		//Concatenate path and exe name strings
		if ((command_string = malloc(sizeof(char)*(strlen(command[0]) + strlen(exe_dir) + 2))) == NULL)
		{
			perror("ERROR: memory allocation failure!");
			exit(EXIT_FAILURE);
		}

		command_string[0] = '\0';
		sprintf(command_string, "%s/%s", exe_dir, command[0]);

		//Execute command
		child_retval = execve(command_string, command, NULL);

		//Below should never execute...

		//Free string
		free(command_string);

		//Terminate child process
		exit(child_retval);
	}
	else //Parent process
	{

		//Check if we wait on child
		if (wait_on_child)
		{
			child_pid = waitpid(child_pid, &child_retval, 0);

			if (child_retval)
			{
				fprintf(stderr, "ERROR: child process returned %u\n", child_retval);
			}
		}
		else
		{
			//Report child creation
			printf("\n[process running in background with pid %u\n", child_pid);

			//Increment child counter
			(*child_count)++;
		}
	}

execute_cleanup:

	//Cleanup
	for (binary_dirs_siz = 0; binary_dirs_siz < 100; binary_dirs_siz++)
	{
		free(binary_dirs[binary_dirs_siz]);
	}
	free(binary_dirs);
	free(command);
}

//Helper function to tokenize user input
void tokenize_input(char * original_input, char *** p_formatted_output)
{
	//Variable declarations
	char **input_array, *tokenized_input, *s_itr, *s_buffer, *s_buffer2;
	uintptr_t arr_index;
	int itr_len;

	//Variable initialization
	if ((tokenized_input = malloc(sizeof(char)*(strlen(original_input) + 1))) == NULL)
	{
		perror("ERROR: memory allocation failure!");
		exit(EXIT_FAILURE);
	}

	if ((input_array = malloc(sizeof(char*)* (UI_MAX_LEN + 1))) == NULL)
	{
		perror("ERROR: memory allocation failure!");
		exit(EXIT_FAILURE);
	}

	for (arr_index = 0; arr_index < UI_MAX_LEN; arr_index++)
	{
		if ((input_array[arr_index] = calloc((UI_MAX_LEN + 1), sizeof(char))) == NULL)
		{
			perror("ERROR: memory allocation failure!");
			exit(EXIT_FAILURE);
		}
	}

	//Clone input
	strncpy(tokenized_input, original_input, strlen(original_input));
	tokenized_input[strlen(original_input)] = 0;

	//Tokenize input
	arr_index = 0;

	//Set string buffer to first element of tokenized_input in order to let free() work
	s_buffer = tokenized_input;

	while (((s_itr = strsep(&tokenized_input, " ")) != NULL) && arr_index < UI_MAX_LEN)
	{
		//Get length of token
		itr_len = strlen(s_itr);
		itr_len++;

		//Do tilde replacement
		if (s_itr[0] == '~')
		{
			s_buffer2 = getenv("HOME");

			//Error-checking
			if (strlen(s_itr + 1) + strlen(s_buffer2) > UI_MAX_LEN)
			{
				fprintf(stderr, "ERROR: command too long!");
				exit(EXIT_FAILURE);
			}

			strcpy(input_array[arr_index], s_buffer2);
			strcat(input_array[arr_index], s_itr + 1);
		}
		else if (s_itr[0] == '$') //Do environment variable replacement
		{
			if ((s_buffer2 = getenv(s_itr + 1)) == NULL)
			{
				perror("ERROR: environment variable does not exist!");
				continue;
			}

			//Error-checkng
			if (strlen(s_itr + 1) + strlen(s_buffer2) > UI_MAX_LEN)
			{
				fprintf(stderr, "ERROR: command too long!");
				exit(EXIT_FAILURE);
			}

			strcpy(input_array[arr_index], s_buffer2);
		}
		else
		{
			//Copy string into array
			strncpy(input_array[arr_index], s_itr, itr_len);
		}
		

		//Increment index counter
		arr_index++;
	}

	//Set last element of input_array to be its size
	input_array[UI_MAX_LEN] = (char *)arr_index;

	//Set output pointer
	*p_formatted_output = input_array;

	//Cleanup
	free(s_buffer);
	
}

//Generate command array
char ** gen_command(char **tokenized_input, int start_pos, int arr_index)
{
	//Variable declaration
	int cp_indx,
		out_indx;
	char ** output;

	//Error-checking
	if (start_pos > arr_index)
	{
		fprintf(stderr, "ERROR: invalid input received\n");
		return NULL;
	}

	if ((output = malloc(sizeof(char *)* (arr_index - start_pos + 3))) == NULL)
	{
		perror("ERROR: memory allocation failure!");
		exit(EXIT_FAILURE);
	}

	out_indx = 0;

	for (cp_indx = start_pos; cp_indx <= arr_index; cp_indx++)
	{
		output[out_indx++] = tokenized_input[cp_indx];
	}

	//Null-terminate array
	output[out_indx] = NULL;

	return output;
}

//Insert string into token array, returns number of tokens added
int insert_tokens(char *** p_dest, char * source, int pos)
{
	char ** new_tokens, **old_tokens, **combined_tokens;
	int new_tok_len, old_tok_len, arr_index;

	tokenize_input(source, &new_tokens);

	//Extract metadata from token arrays
	new_tok_len = (uintptr_t)new_tokens[UI_MAX_LEN]; 
	old_tok_len = (uintptr_t)(*p_dest)[UI_MAX_LEN];

	//Ensure new length won't exceed limits
	if(new_tok_len + old_tok_len > UI_MAX_LEN)
	{
		fprintf(stderr, "ERROR: new command is too long\n");
		return old_tok_len;
	}

	//Initialize combined_tokens
	if ((combined_tokens = malloc(sizeof(char*)* (UI_MAX_LEN + 1))) == NULL)
	{
		perror("ERROR: memory allocation error");
		exit(EXIT_FAILURE);
	}

	//Allocate elements of combined_tokens
	for (arr_index = 0; arr_index < UI_MAX_LEN; arr_index++)
	{
		if ((combined_tokens[arr_index] = calloc(UI_MAX_LEN + 1, sizeof(char))) == NULL)
		{
			perror("ERROR: memory allocation failure!");
			exit(EXIT_FAILURE);
		}
	}

	//De-reference p_dest
	old_tokens = *p_dest;

	//Copy in first set of tokens
	for (arr_index = 0; arr_index < pos; arr_index++)
	{
		strncpy(combined_tokens[arr_index], old_tokens[arr_index], UI_MAX_LEN);
		combined_tokens[arr_index][UI_MAX_LEN] = '\0'; //Ensure null termination
	}

	//Copy in second set of tokens
	for (; arr_index < pos + new_tok_len; arr_index++)
	{
		strncpy(combined_tokens[arr_index], new_tokens[arr_index - pos], UI_MAX_LEN);
		combined_tokens[arr_index][UI_MAX_LEN] = '\0'; //Ensure null termination
	}

	//Copy in third set of tokens
	for (; arr_index + 1 < pos + new_tok_len + old_tok_len; arr_index++)
	{
		strncpy(combined_tokens[arr_index], old_tokens[arr_index - new_tok_len + 1], UI_MAX_LEN);
		combined_tokens[arr_index][UI_MAX_LEN] = '\0'; //Ensure null termination
	}

	//Cleanup
	for (arr_index = 0; arr_index < UI_MAX_LEN; arr_index++)
	{
		free(old_tokens[arr_index]);
	}

	free(old_tokens);

	//Cleanup
	for (arr_index = 0; arr_index < UI_MAX_LEN; arr_index++)
	{
		free(new_tokens[arr_index]);
	}

	free(new_tokens);

	//Set pointer
	*p_dest = combined_tokens;

	return new_tok_len - 1;
}


//Search history for text string
int search_hist_str(char ** hist_arr, int hist_indx, char * search_str)
{
	//Variable declarations
	int arr_index;

	//Variable initialization
	arr_index = 0;

	//Error-checking
	if (strlen(search_str) != 3)
	{
		return -1;
	}

	//Loop through history array
	for (arr_index = 0; arr_index < hist_indx; arr_index++)
	{
		//Compare history string to search string
		if (strncmp(hist_arr[arr_index], search_str, 3) == 0)
		{
			//If match, return index position
			return arr_index;
		}
	}

	//Failure
	return -1;
}

//Reset latest history item
void reset_history(char **history, char **tokenized_input, int history_index, int num_tokens)
{
	int s_len, arr_index;

	//Adjust index
	history_index--;

	//Initialize var
	s_len = num_tokens - 1;

	//Get length of tokenized input
	for (arr_index = 0; arr_index < num_tokens; arr_index++)
	{
		s_len += strlen(tokenized_input[arr_index]);
	}

	//Check length of new command
	if (s_len > UI_MAX_LEN)
	{
		fprintf(stderr, "ERROR: command too long!\n");
		return;
	}

	//Free original string
	free(history[history_index]);

	//Allocate new string
	if ((history[history_index] = calloc(UI_MAX_LEN + 2, sizeof(char))) == NULL)
	{
		perror("ERROR: memory allocation failure!");
		return;
	}

	//Copy in new string
	for (arr_index = 0; arr_index < num_tokens; arr_index++)
	{
		strcat(history[history_index], tokenized_input[arr_index]);
		strcat(history[history_index], " ");
	}

	//Eliminate final space
	history[history_index][strlen(history[history_index]) - 1] = '\0';

	return;
}

//Stores user input in history array
void store_history(char ** input_history, int * history_index, char * input)
{
	int i;
	char * history;

	history = input_history[(*history_index)];

	//Copy in input
	for (i = 0; i < (strlen(input) % UI_MAX_LEN); i++)
	{
		history[i] = input[i];
	}
	history[UI_MAX_LEN + 1] = 0; //Null-termination

	//Update history_index
	(*history_index) = (*history_index + 1) % UI_MAX_LEN;
}

//Prints user input history
void print_history(char ** input_history, int history_index)
{
	int loop_index;

	//Loop through history array
	for (loop_index = 0; loop_index < history_index; loop_index++)
	{
		printf("%u: %s\n", loop_index + 1, input_history[loop_index]);
	}
}

//Allocate memory for input history
void init_history(char *** p_input_history)
{
	int index;

	//Allocate array
	if ((*p_input_history = calloc(UI_MAX_LEN + 1, sizeof(char*))) == NULL)
	{
		perror("ERROR: memory allocation failure!");
		exit(EXIT_FAILURE);
	}

	//Loop through array
	for (index = 0; index < (UI_MAX_LEN + 1); index++)
	{
		//Allocate strings in array
		if (((*p_input_history)[index] = calloc((UI_MAX_LEN + 2), sizeof(char))) == NULL)
		{
			perror("ERROR: memory allocation failure!");
			exit(EXIT_FAILURE);
		}
	}

}


//Helper function to free memory for history array
void clean_history(char ***p_input_history)
{
	int index;

	for (index = 0; index < (UI_MAX_LEN + 1); index++)
	{
		free((*p_input_history)[index]);
	}

	free(*p_input_history);
}


//Parse user input
void parse_input(char *user_input, bool *continue_loop, int * child_count, char ** history, int history_index)
{
	//Variable declarations
	char ** tokenized_input, //User input, broken into an array of strings
		*s_itr, //String "iterator"
		*s_buf, //String buffer
		c_itr, //Char "iterator"
		**execute_subarray; //Array to be sent to execute function (single command with flags)
	int num_tokens, //Number of strings in tokenized_input array
		arr_index, //Current index into tokenized_input array
		start_pos, //Starting index of current command
		s_itr_siz, //Length of s_itr
		in_fd, //Input file descriptor
		out_fd, //Output file descriptor
		pipe_fd[2], //Pipe file descriptor array
		int_buf, //General integer buffer
		r_code; //Return code for function calls
	bool wait, //Boolean determining whether to use blocking wait
		subarr_set, //Boolean determining whether the command subarray has already been set
		history_reset, //Boolean indicating the tokenized input has been modified by a !+ history operation
		redin, //Boolean indicating input fd has been redirected
		piping; //Boolean determining whether the next command will be taking input from a pipe

	//Break input string into an array of arguments
	tokenize_input(user_input, &tokenized_input);

	//Get number of tokens (embedded in last element of array)
	num_tokens = (uintptr_t)((tokenized_input[UI_MAX_LEN]));

	//Initialize vars
	redin = piping = false;
	subarr_set = false;
	history_reset = false;
	start_pos = 0;
	in_fd = STDIN_FILENO;
	out_fd = STDOUT_FILENO;

	//Scroll through argument list
	for (arr_index = 0; arr_index < num_tokens; arr_index++)
	{
		//Initialize to default values
		wait = true;

		//Check if we are operating on new command
		if (start_pos == arr_index)
		{
			out_fd = STDOUT_FILENO;
		}

		//Check for piping
		if (!piping && !redin) //Piping disabled (normal)
		{
			in_fd = STDIN_FILENO;
		}
		else if(piping)//Piping enabled
		{
			//Set input file descriptor
			in_fd = pipe_fd[0];
		}

		//Set string "iterator" and get length
		s_itr = tokenized_input[arr_index];
		s_itr_siz = strlen(s_itr);

		//Get first character of string
		c_itr = s_itr[0];

		if (s_itr_siz == 0)
		{
			//Skip empty commands
			continue;
		}
		else if (s_itr_siz == 1)
		{
			//Detect pipes
			if (c_itr == '|')
			{
				//Error-checking
				if (arr_index == 0)
				{
					fprintf(stderr, "ERROR: invalid input\n");
					//End
					goto terminate_parse;
				}

				wait = false; //Don't wait on child process
				piping = true;

				//Set up pipe
				r_code = pipe(pipe_fd);

				//Error-checking
				if (r_code < 0)
				{
					perror("ERROR: Pipe construction failed!");
					//End
					goto terminate_parse;
				}

				//Adjust output file descriptor
				out_fd = pipe_fd[1];

				//Generate command array
				if (!subarr_set)
				{
					subarr_set = true;
					execute_subarray = gen_command(tokenized_input, start_pos, (arr_index - 1));
				}

				//Adjust starting position of next command
				start_pos = arr_index + 1;

				//Execute
				execute_command(execute_subarray, in_fd, out_fd, wait, child_count);
				subarr_set = false;

				//Finish with current command
				continue;

			}
			else if (c_itr == '>') //Redirect output with overwrite
			{
				//Error-checking
				if (arr_index == 0)
				{
					fprintf(stderr, "ERROR: invalid input\n");
					//End
					goto terminate_parse;
				}

				//Generate command array
				if (!subarr_set)
				{
					subarr_set = true;
					execute_subarray = gen_command(tokenized_input, start_pos, (arr_index - 1));
				}

				//Get output file name
				arr_index++;

				//Adjust starting position of next command
				start_pos = arr_index + 1;

				//Set output fd
				out_fd = open(tokenized_input[arr_index], O_CREAT | O_WRONLY | O_TRUNC);

				//Check if file exists, and is writeable
				r_code = access(tokenized_input[arr_index], W_OK);

				if (r_code != 0)
				{
					fprintf(stderr, "ERROR: Unable to open file\n");
					continue;
				}

				//Error-checking
				if (out_fd < 0)
				{
					perror("ERROR: File open failed");
					
					//End
					goto terminate_parse;
				}

			}
			else if (c_itr == '<') //Redirect input with overwrite
			{
				//Error-checking
				if (arr_index == 0)
				{
					fprintf(stderr, "ERROR: invalid input\n");
					//End
					goto terminate_parse;
				}

				//Generate command array
				if (!subarr_set)
				{
					subarr_set = true;
					execute_subarray = gen_command(tokenized_input, start_pos, (arr_index - 1));
				}

				//Get output file name
				arr_index++;

				//Adjust starting position of next command
				start_pos = arr_index + 1;

				//Set input fd
				in_fd = open(tokenized_input[arr_index], O_RDONLY);

				//Set redirect boolean
				redin = true;

				//Error-checking
				if (in_fd < 0)
				{
					perror("ERROR: File open failed");
					//End
					goto terminate_parse;
				}
			}
			else if (c_itr == '&')
			{
				//Error-checking
				if (arr_index == 0)
				{
					fprintf(stderr, "ERROR: invalid input\n");
					//End
					goto terminate_parse;
				}

				//Generate command array
				if (!subarr_set)
				{
					subarr_set = true;
					execute_subarray = gen_command(tokenized_input, start_pos, (arr_index - 1));
				}

				//Set wait bool
				wait = false;

				//Execute
				execute_command(execute_subarray, in_fd, out_fd, wait, child_count);
				subarr_set = false;

				//Reset pipe flag
				piping = false;

				//End
				goto terminate_parse;
			}
		}
		else if ((s_itr_siz == 2) && (strncmp(s_itr, ">>", 2) == 0))
		{
			//Error-checking
			if (arr_index == 0)
			{
				fprintf(stderr, "ERROR: invalid input\n");
				//End
				goto terminate_parse;
			}

			//Generate command array
			if (!subarr_set)
			{
				subarr_set = true;
				execute_subarray = gen_command(tokenized_input, start_pos, (arr_index - 1));
			}

			//Get output file name
			arr_index++;

			//Adjust starting position of next command
			start_pos = arr_index + 1;

			//Check if file exists, and is writeable
			r_code = access(tokenized_input[arr_index], W_OK);

			if (r_code != 0)
			{
				fprintf(stderr, "ERROR: Unable to open file\n");
			}

			//Set output fd
			out_fd = open(tokenized_input[arr_index], O_APPEND | O_WRONLY);

			//Error-checking
			if (out_fd < 0)
			{
				perror("ERROR: File open failed");
				//End
				goto terminate_parse;
			}
		}
		else if ((s_itr_siz == 2) && (strncmp(s_itr, "cd", 2) == 0))
		{
			if (arr_index + 1 == num_tokens)
			{
				s_itr = getenv("HOME");

				if (chdir(s_itr) == -1)
				{
					perror("ERROR: unable to change to user dir");
				}
			}
			else
			{
				if (chdir(tokenized_input[arr_index + 1]) == -1)
				{
					perror("ERROR: directory change failure");
				}
			}
			//End
			goto terminate_parse;
		}
		else if ((s_itr_siz == 4) && (strncmp(s_itr, "exit", 4) == 0))
		{
			*continue_loop = false;
			fprintf(stdout, "Bye!\n");
			//End
			goto terminate_parse;
		}
		else if ((s_itr_siz == 7) && (strncmp(s_itr, "history", 6) == 0))
		{
			print_history(history, history_index);
			//End
			goto terminate_parse;
		}
		else
		{
			//Execute queued command
			if (subarr_set)
			{
				execute_command(execute_subarray, in_fd, out_fd, wait, child_count);
				subarr_set = false;

				//Reset pipe flag
				redin = piping = false;
			}
		}

		//History commands
		if (s_itr[0] == '!') 
		{
			if (s_itr_siz == 4 && isdigit(s_itr[1]) && isdigit(s_itr[2]) && isdigit(s_itr[3]))
			{
				//Convert string to int
				int_buf = 0;
				int_buf += s_itr[3] - '0';
				int_buf += 10 * (s_itr[2] - '0');
				int_buf += 10 * (s_itr[1] - '0');

				//Convert to 1-index rather than 0th index
				int_buf--;

				//Check if history request is in range
				if (int_buf >= history_index - 1)
				{
					fprintf(stderr, "ERROR: history request is out of range\n");
					//End
					goto terminate_parse;
				}

				//Parse history item
				if (history[int_buf] != NULL)
				{
					num_tokens += insert_tokens(&tokenized_input, history[int_buf], arr_index);
					arr_index--; //Decrement array index to re-process new tokens
					history_reset = true;
				}
			}
			else if (s_itr_siz == 4)
			{
				if ((s_buf = malloc(sizeof(char) * 4)) == NULL)
				{
					perror("ERROR: memory allocation failure!");
					exit(EXIT_FAILURE);
				}


				//Copy in search string
				sprintf(s_buf, "%c%c%c", s_itr[1], s_itr[2], s_itr[3]);

				//Search history array
				int_buf = search_hist_str(history, history_index, (s_buf));

				//Cleanup
				free(s_buf);

				//Check search results
				if (int_buf == -1)
				{
					fprintf(stderr, "ERROR: history item not found\n");
				}
				else
				{
					//Inject history item into tokenized_input
					num_tokens += insert_tokens(&tokenized_input, history[int_buf], arr_index);
					arr_index--; //Decrement array index to re-process new tokens
					history_reset = true;
				}
			}
			else if (s_itr[1] == '!' && s_itr_siz == 2) // !!
			{
				int_buf = history_index - 2;

				if ((int_buf >= history_index - 1) || (history_index == 1))
				{
					fprintf(stderr, "ERROR: history request is out of range\n");
					//End
					goto terminate_parse;
				}
				else if (history[int_buf][0] == '!')
				{
					fprintf(stderr, "ERROR: attempting to create infinite loop\n");
					goto terminate_parse;
				}

				//Parse history item
				if (history[int_buf] != NULL)
				{
					num_tokens += insert_tokens(&tokenized_input, history[int_buf], arr_index);
					arr_index--; //Decrement array index to re-process new tokens
					history_reset = true;
				}
			}

			//Reset history array
			if (history_reset)
			{
				history_reset = false;
				reset_history(history, tokenized_input, history_index, num_tokens);
			}
		}
	}

	if (!subarr_set)
	{
		subarr_set = true;
		execute_subarray = gen_command(tokenized_input, start_pos, (arr_index - 1));
	}

	execute_command(execute_subarray, in_fd, out_fd, wait, child_count);

	//Reset pipe flag
	piping = false;

terminate_parse:

	//Cleanup
	for (num_tokens = 0; num_tokens < UI_MAX_LEN; num_tokens++)
	{
		free(tokenized_input[num_tokens]);
	}

	free(tokenized_input);
}

//Main function
int main(int argc, char * argv[])
{
	//Variable declarations
	char
		user_input[(UI_MAX_LEN + 2)], //String holding most recent user input
		**input_history,  //2D Stack array of user inputs
		*input_buffer, //Buffer string for manipulating user input
		*working_dir; //Current working directory (for shell output)
	int history_index, //Index location of next free history string
		status, //Status value sent to waitpid()
		child_pid, //PID returned by waitpid()
		child_count; //Count of child processes (for non-blocking wait)
	bool looping; //Boolean value to keep main loop running

	//Initialize history index and child_count
	child_count = history_index = 0;

	//Initialize history
	init_history(&input_history);

	//Initialize working_dir
	if ((working_dir = calloc(PATH_MAX, sizeof(char))) == NULL)
	{
		perror("ERROR: memory allocation failure!");
		return EXIT_FAILURE;
	}

	//Set test path
#ifdef TEST_PATHS
	putenv(TEST_PATHS);
#endif

	//Initialize looping
	looping = IN_LOOP;

	//Main Loop
	while (looping)
	{
		//Get working directory (with error-checking)
		if ((working_dir = getcwd(working_dir, PATH_MAX)) == NULL)
		{
			perror("Failed to get working directory");
			return EXIT_FAILURE;
		}


		//Check for terminated child processes, if necessary
		if (child_count > 0)
		{
			status = 0;
			child_pid = waitpid(0, &status, WNOHANG);

			//Check if a child terminated
			if (child_pid != 0)
			{
				//Child terminated, decrement child counter
				child_count--;

				//Report completion of execution
				printf("\n[process %u completed]\n", child_pid);
			}
		}

		//Print working directory
		printf("%s$", working_dir);
		fflush(stdout); //Force print

		//Get user input
		input_buffer = fgets(user_input, (UI_MAX_LEN + 1), stdin);

		//Error-checking
		if (input_buffer == NULL)
		{
			perror("Error reading user input");
		}

		//Get rid of trailing newline
		if (user_input[strlen(input_buffer) - 1] == '\n')
		{
			user_input[strlen(input_buffer) - 1] = 0;
		}

		//Store user input in history
		store_history(input_history, &history_index, user_input);

		//Attempt to parse user input
		parse_input(user_input, &looping, &child_count, input_history, history_index);

	}

	clean_history(&input_history);
	free(working_dir);

	return EXIT_SUCCESS;
}