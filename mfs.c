#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BLOCK_SIZE 1024
#define NUM_BLOCKS 65536
#define BLOCKS_PER_FILE 1024
#define NUM_FILES 256
#define FIRST_DATA_BLOCK 1001
#define MAX_FILE_SIZE 1048576

uint8_t data[NUM_BLOCKS][BLOCK_SIZE]; //place where all the files live

//512 blocks just for free block map
uint8_t * free_blocks;
uint8_t * free_inodes;

//directory structure
struct _directoryEntry
{
    char filename[64];
    short in_use;
    int32_t inode;
};

struct _directoryEntry * directory;

//inode structure
struct inode
{
    int32_t blocks[BLOCKS_PER_FILE];
    short in_use;
	uint8_t attribute;
	uint32_t file_size;
};

struct inode * inodes;

FILE * fp;
char image_name[64];
uint8_t image_open;

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255	// The maximum command-line size

#define MAX_NUM_ARGUMENTS 11	//10 command line parameters in addition to the command

int32_t findFreeBlock()
{
	int i;
	for(i=0; i<NUM_BLOCKS; i++)
	{
		if(free_blocks[i])
		{
			return i + 1001 ;
		}
	}
	return -1;
}
int32_t findFreeInode()
{
	int i;
	for(i=0; i < NUM_FILES; i++)
	{
		if(free_inodes[i])
		{
			return i;
		}
	}
	return -1;
}

int32_t findFreeInodeBlock( int32_t inode)
{
	int i;
	for(i=0; i < BLOCKS_PER_FILE; i++)
	{
		//we found a free block
		if(inodes[inode].blocks[i] == -1)
		{
			return i;
		}
	}
	return -1;
}

void init()
{
	directory = (struct _directoryEntry*)&data[0][0];
	inodes = (struct inode*)&data[20][0];
	free_blocks = (uint8_t *)&data[1000][0];
    free_inodes = (uint8_t *)&data[19][0];
  
	memset(image_name, 0, 64);
	image_open = 0;

	int i;
	for (i=0 ; i < NUM_FILES ; i++)
	{
		directory[i].in_use = 0;
		directory[i].inode = -1;
		free_inodes[i] = 1; 
		memset( directory[i].filename, 0, 64);

		int j;
		for (j = 0 ; j < NUM_BLOCKS ; j++)
		{
			inodes[i].blocks[j] = -1;
			inodes[i].in_use = 0;
			inodes[i].attribute = 0;
			inodes[i].file_size = 0;
		}
	}

	int j;
	for(j=0; j<NUM_BLOCKS; j++)
	{
		free_blocks[j] = 1;
	}
}

//finds the amount of free space in the filesystem
uint32_t df()
{
  int j;
  int count = 0;
  //loop through all the blocks and count the free ones
  for(j=FIRST_DATA_BLOCK; j<NUM_BLOCKS; j++)
  {
	if(free_blocks[j])
	{
		count++;
	}
  }
  //amount of free space is the amount of free blocks multiplied by the size of each block.
  return count * BLOCK_SIZE;
}

//Retrieve from filesystem image and place it in cwd (not currently working)

void retrieve(char * filename, char * newfilename)
{
	short fileFound = 0;
	fp = fopen(filename, "r");

	if(fp == NULL)
	{
		printf("ERROR: File not found\n");
		return;
	}
	//find the file in the filesystem image
	char block[BLOCK_SIZE];
	while(fread(block, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
	{
		//check if block contains file
		if (strncmp(block, filename, strlen(filename)) == 0)
		{
			fileFound = 1;
			break;
		}
	}
	//if file has not been found at this point, raise error
	if(!fileFound)
	{
		printf("ERROR2: File not found\n");
		fclose(fp);
		return;
	}

	//Creates the new output file (name depending on whether parameter 2 is null or not)
	if(newfilename == NULL)
	{
		newfilename = filename;
	}

	//output file is created
	FILE * oFile = fopen(newfilename, "w");
	if(oFile == NULL)
	{
		printf("ERROR: Could not create file\n");
		fclose(fp);
		return;
	}

	uint32_t writtenBytes = fwrite(block, 1, BLOCK_SIZE, oFile);
	//while there are still bytes to be written, write them into the output file
	while(fread(block, 1, BLOCK_SIZE, fp) == BLOCK_SIZE)
	{
		writtenBytes += fwrite(block, 1, BLOCK_SIZE, oFile);
	}

	fclose(fp);
	fclose(oFile);

	printf("Done\n");
}

//delete code
void delete(char* filename) 
{
	//iterate through the directory entries until our filename is found
	for (int i = 0; i < NUM_FILES; i++) 
	{
		//if found, the corresponding inode and dirent marked not in use
		if (!strcmp(directory[i].filename, filename)) 
		{
		inodes[directory[i].inode].in_use = 0;
		directory[i].in_use = 0;

		//free the blocks that are associated with the inode if they are in use
		for (int j = 0; j < BLOCKS_PER_FILE; j++)
		{
			if (inodes[directory[i].inode].blocks[j] != -1)
			{
			free_blocks[inodes[directory[i].inode].blocks[j]] = 0;
			inodes[directory[i].inode].blocks[j] = -1;
			}
		}
		//if we reach here the file has been successfully deleted
		printf("File '%s' deleted.\n", filename);
		return;
		}
	}
	//if we reach here the file was not found
	printf("File '%s' not found.\n", filename);
}

void undelete(char* filename) 
{
	//same file search as delete
	for (int i = 0; i < NUM_FILES; i++) 
	{
		//if file is found,check whether the corresponding inode is already marked as in use. 
		if (!strcmp(directory[i].filename, filename)) 
		{
		if (!inodes[directory[i].inode].in_use)
		{
			inodes[directory[i].inode].in_use = 1;
			directory[i].in_use = 1;

			//mark the blocks associated with the inode as in use and set free_blocks to true
			for (int j = 0; j < BLOCKS_PER_FILE; j++)
			{
			if (inodes[directory[i].inode].blocks[j] != -1)
			{
				free_blocks[inodes[directory[i].inode].blocks[j]] = 1;
			}
			}
			printf("File '%s' undeleted.\n", filename);
			return;
		}
		else
		{
			//File was already undeleted if we reach here.
			printf("File '%s' is already undeleted.\n", filename);
			return;
		}
		}
	}
	printf("File '%s' not found.\n", filename);
}

/*
 * This function will print bytes from a given file starting at startBytes/
 * We do this by seeking to the start byte, reading in the specified number of bytes,
 * and then printing them out in hex. using the %02x format specifier.
 */

void readFile( char *filename, int32_t startBytes, int32_t numOfBytes)
{
	fp = fopen(filename, "r");
	if(fp == NULL)
	{
		printf("ERROR: File not found\n");
		return;
	}

	//seek to the start byte
	if(fseek(fp, startBytes, SEEK_SET) != 0)
	{
		printf("ERROR: Invalid start byte\n");
		fclose(fp);
		return;
	}

	//read in bytes from the file with a buffer initialized 
	// to the number of bytes we want to read
	char * buffer = (char *)malloc(numOfBytes);
	int32_t bytesRead = fread(buffer, 1, numOfBytes, fp);
	if(bytesRead != numOfBytes)
	{
		printf("ERROR: Invalid number of bytes\n");
		fclose(fp);
		return;
	}

	for(bytesRead = 0; bytesRead < numOfBytes; bytesRead++)
	{
		printf("%x ", buffer[bytesRead]);
	}
	printf("\n");

	fclose(fp);
}

//Creates a new empty filesystem image, with the name given by the argument filename
void createfs(char * filename)
{
	fp = fopen(filename, "w");

	//copies the filename to the global variable image_name, zeroes out the data array, and sets image_open to true
	strncpy(image_name, filename, strlen(filename));
	memset(data , 0, NUM_BLOCKS*BLOCK_SIZE);
	image_open = 1;

	//Initializes the directory and inode arrays, and sets all the free blocks and inodes to 0 and -1 respectively
	int i;
	for (i=0 ; i < NUM_FILES ; i++)
	{
		directory[i].in_use = 0;
		directory[i].inode = -1;
		free_inodes[i] = 1; 
		memset( directory[i].filename, 0, 64);

		//initialize inode array
		int j;
		for (j = 0 ; j < NUM_BLOCKS ; j++)
		{
			inodes[i].blocks[j] = -1;
			inodes[i].in_use = 0;
			inodes[i].attribute = 0;
			inodes[i].file_size = 0;
		}
	}

	//set all free blocks to 1 indicating they are free
	int j;
	for(j=0; j<NUM_BLOCKS; j++)
	{
		free_blocks[j] = 1;
	}
	
	//close the file
	fclose (fp);
}

void savefs()
{
	//if there is no image open
	if (!image_open)
	{
		printf("ERROR: Disk image is not open\n");
	}
	fp = fopen(image_name, "w");

	//write the data array to the file and set the memory of the image name to 0
	fwrite (&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
	memset(image_name, 0, 64);

	fclose (fp);
}

//open the filesystem image with the name given by the argument filename
void openfs (char* filename)
{
	fp = fopen(filename, "r");

	//copies the filename to the global variable image_name, reads the data array from the file, and sets image
	//as ready to use.
	strncpy(image_name, filename, strlen(filename));

	//read the data array from the file
	fread(&data[0][0], BLOCK_SIZE, NUM_BLOCKS, fp);
	image_open = 1;

	fclose (fp);
}

void closefs()
{
	if (image_open == 0)
	{
		printf("ERROR: Disk image is not open\n");
		return;
	}

	fclose(fp);
	image_open = 0;
	memset(image_name, 0, 64);
}

void list()
{
	int not_found = 1;

	//iterate through all the files in the directory and prints the name of each file
	//that is marked as in use.
	for (int i = 0 ; i < NUM_FILES ; i++)
	{
		if (directory[i].in_use)
		{
			not_found = 0;
			char filename[65];
			memset(filename, 0, 65);
			strncpy(filename, directory[i].filename, strlen(directory[i].filename));
			printf("%s\n", filename);
		}

	}

	if (not_found)
	{
		printf("ERROR: no files found.\n");
	}

}

void insert(char * filename)
{
	//verify filename is not null
   	if(filename == NULL)
   	{
	 	printf("ERROR: Filename is NULL\n");
	 	return;
   	}

	//verify file exists
   	struct stat buf;
   	int ret = stat(filename, &buf);

   	if(ret == -1)
   	{
	 	printf("ERROR: File does not exist.\n");
	 	return;
   	}

	//verify file isnt too big
   	if(buf.st_size>MAX_FILE_SIZE)
   	{
     	printf("ERROR: File is too large.\n");
	  	return;
   	}

	//verify there is enough space
   	if(buf.st_size>df())
   	{
		printf("ERROR: Not enough free disk space.\n");
	 	return;
   	}

	//find an empty directory entry
   	int i;
   	int directory_entry = -1;
   	for(i=0; i<NUM_FILES; i++)
   	{
		if(directory[i].in_use == 0)
		{
			directory_entry = i;
			break;
		}
   	}

   if(directory_entry == -1)
   {
	 printf("ERROR: Could not find a free directory entry\n");
	 return;
   }
	
	//open the input file read only
   	FILE *ifp = fopen(filename, "r");
   	printf("reading %d bytes from %s\n", (int)buf.st_size, filename);
	// Save off the size of input file because we will use it later and
	// initially set the offset to 0
   	int32_t copy_size = buf.st_size;

	/**We want to copy and write in chunks of BLOCK_SIZE. to do this we are using fseek
	 * to move along our file stream in chunks of BLOCK_SIZE. We will copy bytes
	 * increment our fp by BLOCK_SIZE and repeat until we have copied the entire file.
	*/
   	int32_t offset = 0;

	/**We are going to copy and store our file in BLOCK_SIZE chunks rather than 1 big memory pool
	 * which simulates the way file system stores file data in blocks of space on the disk.
	 * block_index will be used to keep track of which block we are storing our data in.
	*/
   	int32_t block_index = -1;

	//find free inode
	int32_t inode_index = findFreeInode();

	if(inode_index == -1)
	{
		printf("ERROR: Can not find a free inode.\n");
		return;
	} 

	//place file info in directory
	directory[directory_entry].in_use = 1;
	directory[directory_entry].inode = inode_index;
	strncpy(directory[directory_entry].filename, filename, strlen(filename));

	inodes[inode_index].file_size = buf.st_size;

   	// copy_size is initialized to the size of the input file so each loop iter
   	// we copy BLOCK_SIZE bytes from the file then reduce the copy_size by BLOCK_SIZE bytes.
   	// When copy_size is less than BLOCK_SIZE we copy the remaining bytes and then exit the loop
   	// because we have copied the entire file.
   	while(copy_size>0)
   	{
		fseek(ifp, offset, SEEK_SET);
	  	// Read BLOCK_SIZE bytes from the input file and store them in the data array


	  	//find free block
	  	block_index = findFreeBlock();

	  	if(block_index == -1)
	  	{
			printf("ERROR: Can not find a free block.\n");
			return;
	  	}

		

	  	int32_t bytes = fread(data[block_index], BLOCK_SIZE, 1, ifp);

		//save the block in the inode
		int32_t inode_block = findFreeInodeBlock(inode_index);
		inodes[inode_index].blocks[inode_block] = block_index; 

	  	//If bytes == 0 and we haven't reached the end of the file then something is
	  	//wrong. If 0 is returned and we also have the EOF flag set then that is OK (we have )
	  	if(bytes == 0 && !feof(ifp))
	  	{
			 printf("ERROR: An error occured reading from the input file.\n");
		 	return;
	  	}
	
		// Clear EOF file flag
	  	clearerr(ifp);

	  	copy_size -= BLOCK_SIZE;

		//Increment offset into our input file by BLOCK_SIZE. This will allow fseek
		//at the top of the loop to reposition fp to the correct position.
	  	offset += BLOCK_SIZE;

		//increment the index into the block 
	  	block_index = findFreeBlock();

   	}
	// Close the input file after finished copying input file
	fclose(ifp);
}
//XOR encryption using given cipher (limited to a 1 byte value)
void encrypt( char * filename, uint8_t cipher)
{
	FILE *file = fopen(filename, "rb+");
	if (file == NULL)
	{
		printf("ERROR: File does not exist.\n");
		return;
	}

	uint8_t bytes;
	while(fread(&bytes, sizeof(bytes), 1, file) == 1)
	{
		bytes ^= cipher;
		fseek(file, -1, SEEK_CUR);
		fwrite(&bytes, sizeof(bytes), 1, file);
	}

	fclose(file);
}
//XOR decryption using given cipher (limited to a 1 byte value)
void decrypt(char * filename, uint8_t cipher)
{
	{
	FILE *file = fopen(filename, "rb+");
	if (file == NULL)
	{
		printf("ERROR: File does not exist.\n");
		return;
	}

	uint8_t bytes;
	while(fread(&bytes, sizeof(bytes), 1, file) == 1)
	{
		bytes ^= cipher;
		fseek(file, -1, SEEK_CUR);
		fwrite(&bytes, sizeof(bytes), 1, file);
	}

	fclose(file);
}
}
int main()
{
	char * command_string = (char*) malloc( MAX_COMMAND_SIZE );

	fp = NULL;

	init();

    while( 1 )
	{
		// Print out the msh prompt
		printf ("mfs> ");

		// Read the command from the commandline.  The
		// maximum command that will be read is MAX_COMMAND_SIZE
		// This while command will wait here until the user
		// inputs something since fgets returns NULL when there
		// is no input
		while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

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

		if (token[0] == NULL)
		{
		}

		// process the filesystem commands
		else
		{
			if (!strcmp("createfs", token[0]))
			{
				if(token[1] == NULL)
				{
					printf("ERROR: No filename specified\n");
					continue;
				}
				createfs(token[1]);
			}

			if (!strcmp("savefs", token[0]))
			{
				savefs();
			}

			if (!strcmp("open", token[0]))
			{
				if(token[1] == NULL)
				{
					printf("ERROR: No filename specified\n");
					continue;
				}

				openfs(token[1]);
			}

			if (!strcmp("close", token[0]))
			{
				if(token[1] == NULL)
				{
					printf("ERROR: No filename specified\n");
					continue;
				}

				closefs();
			}

			if (!strcmp("list", token[0]))
			{
				if(!image_open)
				{
					printf("ERROR: Disk image not open\n");
					continue;
				}

				list();
			}

			if(!strcmp("df", token[0]))
			{
				if(!image_open)
				{
					printf("ERROR: Disk image is not opened.\n");
					continue;
				}

				printf("%d bytes free\n", df());
			}

			if(!strcmp("quit", token[0]))
			{
				exit(0);
			}

			if(!strcmp("insert", token[0]))
			{
                if(!image_open)
				{
					printf("ERROR: Disk image is not opened.\n");
					continue;
				}

				if(token[1] == NULL)
				{
					printf("ERROR: No filename specified.\n");
					continue;
				}

				insert(token[1]);
			}
			if(!strcmp("read", token[0]))
			{
				if(token[1] == NULL || token[2] == NULL || token[3] == NULL)
				{
					printf("ERROR: Improper Usage(read <filename> <starting bytes> <number of bytes>).\n");
					continue;
				}
				readFile(token[1], atoi(token[2]), atoi(token[3]));
			}
			if(!strcmp("retrieve", token[0]))
			{
				if(token[1] == NULL)
				{
					printf("ERROR: Improper Usage(retrieve <filename> <newfilename>).\n");
					continue;
				}
				retrieve(token[1], token[2]);
			}
			if(!strcmp("encrypt", token[0]))
			{
				if(token[1] == NULL || token[2] == NULL)
				{
					printf("ERROR: Improper Usage(encrypt <filename> <cipher>).\n");
					continue;
				}
				encrypt(token[1], atoi(token[2]));
			}
			if(!strcmp("decrypt", token[0]))
			{
				if(token[1] == NULL || token[2] == NULL)
				{
					printf("ERROR: Improper Usage(decrypt <filename> <cipher>).\n");
					continue;
				}
				decrypt(token[1], atoi(token[2]));
			}

			if (!strcmp("delete", token[0]))
			{
				if (token[1] == NULL)
				{
					printf("Error: File not found.\n");
					continue;
				}

				delete(token[1]);
			}

			if (!strcmp("undelete", token[0]))
			{
				if (token[1] == NULL)
				{
					printf("Error: File not found.\n");
					continue;
				}

				undelete(token[1]);
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
}