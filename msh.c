// The MIT License (MIT)
// 
// Copyright (c) 2016 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255	// The maximum command-line size

#define MAX_NUM_ARGUMENTS 11	//10 command line parameters in addition to the command

#define MAX_HIST 15             //Max number of commands to be saved

int main()
{
	char * command_string = (char*) malloc( MAX_COMMAND_SIZE );

	//dynamically allocating space for the history
	char** hist = calloc(MAX_HIST, sizeof(char*));
	for (int i = 0; i < MAX_HIST ; i++)
	{
		hist[i] = calloc(MAX_COMMAND_SIZE, sizeof(char));
	}

	//dynamically allocating space for the pid array
	pid_t* pidlist = calloc(MAX_HIST, sizeof(pid_t));

	//keeping track of the size of history so it doesnt go above 15
	int count = 0;

	while( 1 )
	{
		// Print out the msh prompt
		printf ("msh> ");

		// Read the command from the commandline.  The
		// maximum command that will be read is MAX_COMMAND_SIZE
		// This while command will wait here until the user
		// inputs something since fgets returns NULL when there
		// is no input
		while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

		//insert !n code here
		if (command_string[0] == '!')
		{
			char ret[3];
			char bad;

			//parse command_string to take out the index number as a string
			sscanf(command_string, "%c%s\n", &bad, ret);

			int index = atoi(ret);
			
			//if index doesnt exist in history
			if (index > count-1)
			{
				printf("Command not in history.\n");
			}

			//if index exists in history
			else
			{
				strcpy(command_string, hist[index]);
			}
		}

		/* Parse input */
		char *token[MAX_NUM_ARGUMENTS];

		for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
		{
			token[i] = NULL;
		}

		int token_count = 0;                                 
															
		// Pointer to point to the token
		// parsed by strsep
		char *argument_ptr = NULL;                                         
															
		char *working_string  = strdup( command_string );                

		// we are going to move the working_string pointer so
		// keep track of its original value so we can deallocate
		// the correct amount at the end
		char *head_ptr = working_string;

		// Tokenize the input strings with whitespace used as the delimiter
		while ( ( (argument_ptr = strsep(&working_string, WHITESPACE ) ) != NULL) && 
				(token_count<MAX_NUM_ARGUMENTS))
		{
			token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
			if( strlen( token[token_count] ) == 0 )
			{
				token[token_count] = NULL;
			}
				token_count++;
		}
		
		//when press enter another line pops up, count is not incremented
		//history is not updated
		if (token[0] == NULL)
		{
		}

		//if the token is not null i.e., if there is any command given
		else
		{
			//code for updating history
			if (count == MAX_HIST)
			{
				//count never goes above 14 so if its 15 we decrement
				count--;
				for (int i = 0 ; i < count ; i++)
				{
					//moving everything in the array one space to the left
					strcpy(hist[i], hist[i+1]);
					pidlist[i] = pidlist[i+1];
				}

				//putting the new command in history at the end
				strcpy(hist[count], command_string);
				count++;
			}

			if (count < MAX_HIST)
			{
				//normally putting the command in the next spot and incrementing count
				strcpy(hist[count], command_string);
				count++;
			}

			//quit or exit
			if (!strcmp("quit", token[0]) || !strcmp("exit", token[0]))
			{
				exit(0);
			}

			//when cd is entered, pid = -1
			else if (!strcmp("cd", token[0]))
			{			
				//since we incremeted count after putting in command, we take count-1 to put in pid
				pidlist[count-1] = -1;

				if (token[1] == NULL)
				{
					//no directory given
					printf("-msh: cd:   : No such file or directory\n");
				}

				else
				{
					int ret = chdir(token[1]);

					//fail to find directory
					if (ret == -1)
					{
						printf("-msh: cd: %s: No such file or directory\n", token[1]);
					}
				}
			}

			//history code			
			else if (!strcmp("history", token[0]))
			{
				pidlist[count-1] = -1;
				
				if (token[1] == NULL)
				{
					for (int i = 0 ; i < count ; i++)
					{
						printf("[%d]: %s", i, hist[i]);
					}
				}

				if (token[1] != NULL)
				{
					for (int i = 0 ; i < count ; i++)
					{
						printf("[%d]: [%d] %s", i, pidlist[i], hist[i]);
					}
				}
			}

			//fork code
			else
			{
				pid_t pid = fork();

				//child
				if (pid == 0)
				{
					//execvp to use system processes
					int fail = execvp(token[0], &token[0]);

					//!n has its own special fail printing
					if (fail == -1 && command_string[0] != '!')
					{
						printf("%s: Command not found.\n", token[0]);
					}

					return 0;
				}

				//parent
				else
				{
					//putting child pid into pid array
					pidlist[count-1] = pid;
					int status;
					wait(&status);
				}
			}
		}

		// Cleanup allocated memory
		for( int i = 0; i < MAX_NUM_ARGUMENTS; i++ )
		{
			if( token[i] != NULL )
			{
				free( token[i] );
			}
		}

		free(head_ptr);
	}

	//freeing all memory
	for (int i = 0; i < MAX_HIST ; i++)
	{
		free(hist[i]);
	}
	free(hist);
	free(pidlist);
	free(command_string);

	return 0;
	// e2520ca2-76f3-90d6-0242ac120003
}
