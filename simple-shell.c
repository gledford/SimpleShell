/**
 * Simple shell interface program.
 *
 * Operating System Concepts - Ninth Edition
 * Copyright John Wiley & Sons - 2013
 *
 * Programmer: Garry Ledford (drexel ID gl89)
 * Date: 23FEB2015
 * Code skeleton taken from Operating System Concepts
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdarg.h>

#define MAX_LINE		80 /* 80 chars per line, per command */
#define BUFFER_LENGTH (MAX_LINE/2) + 1
#define MAX_HISTORY		10 /* Max Number of History Items*/

//Data Structures
//*****************************************************************************
struct parameters
{
	char *parameter;
	struct parameters *next;
};

struct param_list
{
	int length;
	struct parameters *head;
};

struct previous_commands
{
	int index;
	char *string;
};

struct alias_command
{
	char *string;
	char *command;
	struct alias_command *next;
};
//*****************************************************************************

//Function Prototypes
//*****************************************************************************
void print_aliases();
void add_to_history(struct previous_commands *item, struct previous_commands *queue[MAX_HISTORY]);
void print_history(struct previous_commands *prev_commands[MAX_HISTORY]);
char* get_history_command(struct previous_commands *prev_commands[MAX_HISTORY], int id);
char* remove_newline(char *input_buffer);
int is_run_is_background_set(char *input_buffer);
void save_alias(char *input_buffer);
void print_aliases();
char* replace_alias(char *input_buffer);
void switch_stdout_to_file(int append_to_file);
void switch_stdout_to_terminal();
void verbose_print(const char* string, ...);
void load_init_file();
int set_path(const char *buffer);
//*****************************************************************************

//Globals
//*****************************************************************************
static char args[BUFFER_LENGTH];
static int head = -1, tail = -1;
static struct alias_command *alias_head = NULL;
static struct alias_command *alias_curr = NULL;
static int running_script = 0;
static char *script_file_name = NULL;
static int current_file_line_number = 0;
static int fd = 0;
static fpos_t pos;
static int verbose = 0;
static regex_t g_path_regex;
//*****************************************************************************

int main(void)
{
	int child_status, history_num = 1;
	const char space_delimiter[2] = " ";

	//Initialize the parameter list and pointer to the current parameter
	struct param_list *parameter_list = (struct param_list*)malloc(sizeof(struct param_list));
	struct parameters *current_parameter = NULL;
	struct previous_commands *prev_commands[MAX_HISTORY];

	//Initialize all of previous commands
	int cmd_indx;
	for(cmd_indx = 0; cmd_indx < MAX_HISTORY; cmd_indx++)
	{
		prev_commands[cmd_indx] = (struct previous_commands*)malloc(sizeof(struct previous_commands));
		prev_commands[cmd_indx]->string = (char*)malloc(sizeof(char));
	}

	//Compile the regex for finding history commands (!!, !#, etc)
	regex_t history_command_regex;
	regcomp(&history_command_regex, "![!|0-9]", 0);

	regex_t script_regex;
	regcomp(&script_regex, "script", 0);

	regcomp(&g_path_regex, "set path = ([0-9a-zA-Z/_. )]", 0);

	regex_t verbose_regex;
	regcomp(&verbose_regex, "set verbose ", 0);

	load_init_file();

    while (1)
    {
    	//If a script is running, print to terminal what was written to the file
    	if (running_script)
    	{
    		switch_stdout_to_terminal();

			int ch, num_of_lines = 0;
			
			//Get number of lines in the file
			FILE *file2 = fopen(script_file_name, "r");
			do
			{
				ch = fgetc(file2);
				if (ch == '\n')
				{
					num_of_lines++;
				}
			} while (ch != EOF);
			fclose(file2);

			//Print the most recently added lines from the file to the terminal
			FILE *file = fopen(script_file_name, "r");
			int first_newline = 1;
			int ch2, cur_line_num = 0;;
			do
			{
				ch2 = fgetc(file);
				
				if (ch2 == '\n') cur_line_num++;

				if (cur_line_num >= (current_file_line_number + 1) && ch2 != EOF) 
				{
					//Do not print the first newline
					if (cur_line_num == (current_file_line_number + 1) && first_newline)
					{
						first_newline = 0;
					}
					else
					{
						putchar(ch2);
					}
				}
			} while (ch2 != EOF);
			fclose(file);

			//Point to the last line printed
			current_file_line_number = (num_of_lines - current_file_line_number);

		    printf("osh>"); //prints to terminal only
		    fflush(stdout);

			switch_stdout_to_file(1);
    	}

    	char *command;

		int run_in_background = 0;
		int start_script = 0;

        printf("osh>");
        fflush(stdout);
        
        //Get input from stdin
        fgets(args, BUFFER_LENGTH, stdin);
        char *input_buffer = remove_newline(args);
        char *input_copy = (char*)malloc(sizeof(char));
        strcpy(input_copy, input_buffer);

        if (0 == regexec(&verbose_regex, input_buffer, 0, NULL, 0))
        {
        	if (0 == strcmp(input_buffer, "set verbose on"))
        	{
        		verbose = 1;
        		verbose_print("VEBOSE: Enabled verbose\n");
        	}
        	else
        	{
        		verbose_print("VEBOSE: Disabled verbose\n");
        		verbose = 0;
        	}

	        if (running_script)
	        {
	        	printf("%s\n", args); //print the stdin to the file
	        	fflush(stdout);
	        }

	        if (running_script)
	        {
	        	printf("%s\n", args); //print the stdin to the file
	        	fflush(stdout);
	        }
        	continue;
        }

        if (set_path(input_buffer))
        {
        	if (running_script)
        	{
        		printf("%s\n", args); //print the stdin to the file
        		fflush(stdout);	
        	}

        	continue;
        }
        
        //if input is endscript and we are writing to a file
        if (0 == strcmp(input_buffer, "endscript") && running_script)
        {
			printf("%s\n", args); //print the stdin to the file
			printf("Script done, output file is %s\n", script_file_name); //write to file
			fflush(stdout);

			switch_stdout_to_terminal();

			running_script = 0;
			current_file_line_number = 0;

			printf("Script done, output file is %s\n", script_file_name); //write to console
			fflush(stdout);

			free(script_file_name);
        }

        if (running_script)
        {
        	printf("%s\n", args); //print the stdin to the file
        	fflush(stdout);
        }

		//if input is script _filename_
		if (0 == regexec(&script_regex, input_buffer, 0, NULL, 0) && 0 != strcmp(input_buffer, "endscript"))
		{
			script_file_name = (char*)malloc(sizeof(char));

			char *input_copy2 = (char*)malloc(sizeof(char));
			strcpy(input_copy2, input_buffer);

			char *file_name = strtok(input_copy2, space_delimiter);
			file_name = strtok(NULL, space_delimiter);
			if (file_name == NULL)
			{
				strcpy(script_file_name, "typescript");
			}
			else
			{
				strcpy(script_file_name, file_name);
			}
			
			printf("Script started, output file is %s\n", script_file_name); //write to console
			fflush(stdout);
			
			start_script = 1;
			running_script = 1;

			switch_stdout_to_file(0);

			printf("Script started, output file is %s\n", script_file_name); //write to file
			fflush(stdout);

			free(input_copy2);
		}

    	//Check if the agument matches the history regex ex !# or !!
    	if (0 == regexec(&history_command_regex, input_copy, 0, NULL, 0) && start_script == 0)
    	{
    		int history_id;
    		char *cpy = (char*)malloc(sizeof(char));
    		strcpy(cpy, input_buffer);
    		char *cpy_token = strtok(cpy, space_delimiter);
    		
    		//If the second parameter is a "!"
    		if (0 == strcmp(&cpy_token[1], "!"))
    		{
    			//get previous history #
    			history_id = history_num - 1;
    		}
    		else
    		{
    			//get input history number
    			history_id = atoi(&cpy_token[1]);    			
    		}
    		
    		//Replace the input with the history command
    		input_buffer = get_history_command(prev_commands, history_id);
    		strcpy(input_copy, input_buffer);
    	}
        
        char *token = strtok(input_copy, space_delimiter);

        //If input is alias, save off the alias command
        if (token != NULL && 0 == strcmp(&token[0], "alias") && start_script == 0)
        {
        	//if the input is more than "alias"
        	if ((int)strlen(input_buffer) > 5)
        	{
	        	save_alias(input_buffer);

				//Add alias to history list
		    	struct previous_commands *current_command = (struct previous_commands*)malloc(sizeof(struct previous_commands));
		        current_command->string = (char*)malloc(sizeof(char));
		        strcpy(current_command->string, input_buffer);
		        current_command->index = history_num;
		        history_num++;
		        add_to_history(current_command, prev_commands);
			}
			//Else the user wants to print the aliases
			else
			{
	        	print_aliases();
			}

	        token = NULL; //set to NULL to prevent further processing
        }

        if (token != NULL && start_script == 0 && 0 != strcmp(input_buffer, "endscript"))
        {
			char *replaced_with_alias = (char*)malloc(sizeof(char));

			//Check the input to see if we need to replace anything with the aliased commands
			strcpy(replaced_with_alias, replace_alias(input_buffer));

			//If we replaced a command
			if (strcmp(replaced_with_alias, input_copy) != 0)
			{	
				//Set the token to the new replaced buffer
				token = strtok(replaced_with_alias, space_delimiter);
			}
		}

        if (token != NULL && (int)strlen(token) > 0 && *token != '\n' 
        	&& start_script == 0 && 0 != strcmp(input_buffer, "endscript"))
        {
        	//If the input is history
	        if (0 == strcmp(input_copy, "history"))
	        {
	        	//Add history to history list
	        	struct previous_commands *current_command = (struct previous_commands*)malloc(sizeof(struct previous_commands));
		        current_command->string = (char*)malloc(sizeof(char));
		        strcpy(current_command->string, "history");
		        current_command->index = history_num;
		        history_num++;
		        add_to_history(current_command, prev_commands);

	        	print_history(prev_commands);
	        }
			else
			{
				//Add current command to history list
		        struct previous_commands *current_command = (struct previous_commands*)malloc(sizeof(struct previous_commands));
		        current_command->string = (char*)malloc(sizeof(char));
		        strcpy(current_command->string, input_copy);
		        current_command->index = history_num;
		        history_num++;
		        add_to_history(current_command, prev_commands);

	        	command = &token[0];

	        	//If the input is exit, exit the program
	        	if (0 == strcmp(command, "exit"))
	        	{
	        		//cleanup memory here
	        		if (alias_curr && alias_curr != alias_head) free(alias_curr);
	        		if (alias_head) free(alias_head);
	        		if (parameter_list) free(parameter_list);
	        		if (current_parameter) free(current_parameter);
	        		if (input_copy) free(input_copy);

	        		int cmd_indx;
					for(cmd_indx = 0; cmd_indx < MAX_HISTORY; cmd_indx++)
					{
						if (prev_commands[cmd_indx]) free(prev_commands[cmd_indx]);
						if (prev_commands[cmd_indx]->string) free(prev_commands[cmd_indx]->string);
					}

	        	    exit(1);
	        	}

		       	token = strtok(NULL, space_delimiter);
		       	struct parameters * working_parameter = (struct parameters*)malloc(sizeof(struct parameters));

		       	//Grab the first parameter to the command if it exist
		       	if (token != NULL)
		       	{
					working_parameter->parameter = &token[0];
					parameter_list->head = working_parameter;
					parameter_list->length = 1;
					current_parameter = working_parameter; 
		       	}
		       	else
		       	{
					parameter_list->length = 0;		       		
		       	} 		

				//Grab the additional parameters
		       	token = strtok(NULL, space_delimiter);
		        while (token != NULL)
		        {
		        	struct parameters * loop_parameter = (struct parameters*)malloc(sizeof(struct parameters));
		        	loop_parameter->parameter = &token[0];
		        	current_parameter->next = loop_parameter; 
		        	current_parameter = loop_parameter;
		        	parameter_list->length++;

		        	token = strtok(NULL, space_delimiter);
		        }
		        
		        //Point the current parameter to the newly head
		        if (parameter_list->length > 0)
		        {
			        current_parameter = parameter_list->head;
		        }

		        //Check if the run in background command was set
				run_in_background = is_run_is_background_set(args);

				//Create the parameter list with the number of parameters plus 2 for the command and NULL
				//This is used as the input to execvp
				char *params[parameter_list->length + 2];

				//Set the first parameter to the command for execvp
				params[0] = command;

				//Fill out the param array using the current parameter list
				int param_index;
				for (param_index = 1; param_index < (parameter_list->length + 1); param_index++)
				{
					params[param_index] = current_parameter->parameter;
					current_parameter = current_parameter->next;
				}

				//Default the last parameter to NULL for execvp
				params[parameter_list->length + 1] = NULL;

				//If the last parameter is a &, remove the character for execvp
				if (*params[parameter_list->length] == '&')
				{
					params[parameter_list->length] = 0;
				}

				//If we have a command
				if (command[0] > 0)
				{
		    		verbose_print("VEBOSE: Searching PATH: \"%s\" for command \"%s\"\n", getenv("PATH"), command);

				    pid_t child_pid = fork();

				    //Run the child process
				    if (0 == child_pid)
				    {
				    	//Run the command with the parameters
					  	if (0 != execvp(command, params))
					  	{
					  		printf("Error: command \"%s\" not found\n", command);
					  		exit(1);
					  	}
				    }
				    //Run the parent process
				    else
				    {
				      if (run_in_background == 0)
				      {
						waitpid(child_pid, &child_status, 0);
				      }
				    }
				 }
			}
		}
    }
   
	return 0;
}

//*****************************************************************************
// Abstract: adds the input item  to the input queue using the queue
// like a circular buffer.
//*****************************************************************************
void add_to_history(struct previous_commands *item, struct previous_commands *queue[MAX_HISTORY])
{
	//If head and tail are not initialized, set them to zero
	if (head == -1 && tail == -1)
	{
		head++;
		tail++;
	}
	else
	{
		//If either the head or the tail are at the end of the queue
		if ((tail == (MAX_HISTORY - 1)) || (head == (MAX_HISTORY - 1)))
		{
			//if tail is already at the end of the array
			if (tail == (MAX_HISTORY - 1))
			{
				//set tail to first index
				tail = 0;

				//move head over one
				head++;
			}

			//if head is at the end of the array
			if (head == (MAX_HISTORY - 1))
			{
				//set head to fist index
				head = 0;

				//move tail over one
				tail++;
			}
		}
		else
		{
			tail++;

			if (head == tail)
			{
				head++;
			}
		}
	}

	strcpy(queue[tail]->string, item->string);
	queue[tail]->index = item->index;
}

//*****************************************************************************
// Abstract: Prints the commands from the input previous commands array
// in order using the globals head and tail
//*****************************************************************************
void print_history(struct previous_commands *prev_commands[MAX_HISTORY])
{
	//Start at the tail
	int current = tail;
	
	while(current != head)
	{	
		if (prev_commands[current]->index != 0) 
		{
			printf("%d %s\n", prev_commands[current]->index, prev_commands[current]->string);
		}

		if ((current - 1) == head)
		{
			printf("%d %s\n", prev_commands[current - 1]->index, prev_commands[current - 1]->string);
		}

		if (current == 0)
		{
			current = MAX_HISTORY - 1;
		}
		else
		{
			current--;
		}
	}
}

//*****************************************************************************
// Abstract: Returns the string that matches the input history ID number.
// If nothing matches, NULL is returned.
//*****************************************************************************
char* get_history_command(struct previous_commands *prev_commands[MAX_HISTORY], int id)
{
	char *return_val = (char*)malloc(sizeof(char));

	int i;
	for (i = 0; i < MAX_HISTORY; i++)
	{
		if (prev_commands[i]->index == id)
		{
			printf("%s\n", prev_commands[i]->string);
			strcpy(return_val, prev_commands[i]->string);
		}
	}

	return return_val;
}

//*****************************************************************************
// Abstract: Removes the new line from the input buffer
//*****************************************************************************
char *remove_newline(char *input_buffer)
{
	int length = strlen(input_buffer);

	if (length > 0 && input_buffer[length - 1] == '\n')
	{
		input_buffer[length - 1] = '\0';
	}

	return input_buffer;
}

//*****************************************************************************
// Abstract: Returns true is the input has a & and false if not
//*****************************************************************************
int is_run_is_background_set(char *input_buffer)
{
	int i, run_in_background = 0;
	for (i = BUFFER_LENGTH; i > 0; i--)
	{
		if (input_buffer[i] == '&')
		{
			run_in_background = 1;
			break;
		}
	}
	
	return run_in_background;
}

//*****************************************************************************
// Abstract: Saves the alias and the aliased command in the input
//*****************************************************************************
void save_alias(char *input_buffer)
{
	char *temp_input = (char*)malloc(sizeof(char));
	char *temp_input2 = (char*)malloc(sizeof(char));
	
	strcpy(temp_input, input_buffer);	
	strcpy(temp_input2, input_buffer);
	
	const char space_delimiter[2] = " ";
	const char quote_delimiter[2] = "\"";
	
	char *alias_cmd = (char*)malloc(sizeof(char));

	char *token = strtok(temp_input, space_delimiter); //reads alias
	token = strtok(NULL, space_delimiter); //reads aliased string
    
	char *alias_string = (char*)malloc(sizeof(char));
    strcpy(alias_string, token);

    //if we have a string to alias
    if (token != NULL)
    {
		token = strtok(temp_input2, quote_delimiter);
		token = strtok(NULL, quote_delimiter);

	    if (token != NULL)
	    {
	    	strcpy(alias_cmd, token);

	    	struct alias_command *new_alias = (struct alias_command*)malloc(sizeof(struct alias_command));
	    	new_alias->string = (char*)malloc(sizeof(char));
	    	new_alias->command = (char*)malloc(sizeof(char));
	    	strcpy(new_alias->string, alias_string);
	    	strcpy(new_alias->command, alias_cmd);
	    	new_alias->next = NULL;

	    	//If this is the first alias, point the head and curr to the new alias
	    	if (alias_head == NULL)
	    	{
	    		alias_head = alias_curr = new_alias;
	    	}
	    	//Else point the curr->next to the new alias and point curr to new alias
	    	else
	    	{
	    		alias_curr->next = new_alias;
	    		alias_curr = new_alias;
	    	}
	    }
    }
}

//*****************************************************************************
// Abstract: Prints all of the saved aliases
//*****************************************************************************
void print_aliases()
{
    struct alias_command *test = alias_head;
    while(test != NULL)
    {
    	printf("alias %s=\'%s\'\n", test->string, test->command);
    	test = test->next;
    }
    fflush(stdout);
}

//*****************************************************************************
// Abstract: Searches for commands in the input buffer that match an aliases.
// Then returns the a new char* with the replaced alias.
//*****************************************************************************
char* replace_alias(char *input_buffer)
{
	const char space_delimiter[2] = " ";

	//Set up working copies
	char *return_val = (char*)malloc(sizeof(char));
	strcpy(return_val, input_buffer);
	char *temp_input = (char*)malloc(sizeof(char));
	strcpy(temp_input, input_buffer);
	
	//Get first command from the input
	char *token = strtok(temp_input, space_delimiter);

    struct alias_command *test = alias_head;
    while(test != NULL)
    {
    	//If first command matches current alias string
    	if (0 == strcmp(token, test->string))
    	{
    		return_val = test->command;
    		strncpy(return_val, test->command, strlen(test->command));

    		char *full_command = (char*)malloc(sizeof(char));

    		//Add the aliased command to the full_command array
    		int i;
    		for (i = 0; i < strlen(test->command); i++)
    		{
    			full_command[i] = test->command[i];
    		}
    		
    		//Add the remaining parameters to the full_command array
    		int j, k;
    		for(j = (int)strlen(test->string), k = (int)strlen(test->command); 
    			j < (int)strlen(input_buffer); j++, k++)
    		{
    			//printf("input_buffer[%d] = %c\n", j, input_buffer[j]);
    			full_command[k] = input_buffer[j];
    		}

    		full_command[k + 1] = '\0';

    		strcpy(return_val, full_command);

    		break;
    	}
    	test = test->next;
    }

    return return_val;
}

//*****************************************************************************
// Abstract: Redirects stdout to file
//*****************************************************************************
void switch_stdout_to_file(int append_to_file)
{
	fflush(stdout);
	fgetpos(stdout, &pos);
	fd = dup(fileno(stdout));
	
	if (append_to_file)
	{
		freopen(script_file_name, "a", stdout);
	}
	else
	{
		freopen(script_file_name, "w", stdout);
	}
}

//*****************************************************************************
// Abstract: Redirects stdout back to terminal
//*****************************************************************************
void switch_stdout_to_terminal()
{
	fflush(stdout);
	dup2(fd, fileno(stdout));
	close(fd);
	clearerr(stdout);
	fgetpos(stdout, &pos);
}

//*****************************************************************************
// Abstract: Prints verbose data to stdout if the verbose global is set
//*****************************************************************************
void verbose_print(const char* string, ...)
{
	va_list args;
	va_start(args, string);

	if (verbose)
	{
		if (NULL != script_file_name)
		{
			switch_stdout_to_terminal();
		}

		vfprintf(stdout, string, args);

		if (NULL != script_file_name)
		{
			switch_stdout_to_file(1);
		}
	}
}

void load_init_file()
{
	FILE *init_file;
	char init_file_name[] = ".cs543rc";
	char line[80];
	char *line_copy = (char*)malloc(sizeof(char));

	init_file = fopen(init_file_name, "r");

	if (init_file != NULL)
	{
		printf(".cs543rc loaded\n");

		while( fgets(line, 80, init_file) != NULL )
		{
			strcpy(line_copy, line);

			char *token = strtok(line_copy, " ");

        	//If input is alias, save off the alias command
        	if (token != NULL && 0 == strcmp(&token[0], "alias"))
			{
				//if the input is more than "alias"
	        	if ((int)strlen(line) > 5)
	        	{
		        	save_alias(line);
				}
			}

			set_path(line);
		}

		fclose(init_file);
	}
}

int set_path(const char *input_buffer)
{
	int return_val = 0;

	if (0 == regexec(&g_path_regex, input_buffer, 0, NULL, 0))
	{
		char *cpy = (char*)malloc(sizeof(char));
		strcpy(cpy, input_buffer);
		char *new_path = strtok(cpy, "("); //returns "set path = "
		if (new_path != NULL)
		{
			new_path = strtok(NULL, "(");
			if (new_path != NULL)
			{
				new_path = strtok(new_path, ")");
				int tmp_index;
				for(tmp_index = 0; tmp_index < strlen(new_path); tmp_index++)
				{
	    			if (new_path[tmp_index] == ' ')
	    			{
	    				new_path[tmp_index] = ':';
	    			}
				}

				setenv("PATH", new_path, 1);
				return_val = 1;
			}
		}

        free(cpy);
	}

	return return_val;
}