// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * src/shell.c
 *
 * LDB Database simple shell
 *
 * Copyright (C) 2018-2020 SCANOSS.COM
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "ldb.h"
/**
  * @file shell.c
  * @date 12 Jul 2020
  * @brief Contains shell functions and help text
 
  * //TODO Long description
  * @see https://github.com/scanoss/ldb/blob/master/src/shell.c
  */

#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include "ldb.h"
#include "command.h"
#include "ldb_string.h"
#include "logger.h"
/**
 * @brief Contains the shell help text
 * 
 */
void help()
{
	printf("LDB stores information using single, 32-bit keys and single data records. Data records could be fixed in size (drastically footprint for large amounts of short, fixed-sized records). The LDB console accepts the following commands:\n");
	printf("\n");
	printf("Shell Commands\n");
	printf("	create database DBNAME\n");
	printf("    	Creates an empty database\n\n");
	
	printf("	create table DBNAME/TABLENAME keylen N reclen N\n");
	printf("    	Creates an empty table in the given database with\n");
	printf("    	the specified key length (>= 4) and record length (0=variable)\n\n");
	
	printf("	show databases\n");
	printf("  		Lists databases\n\n");
	
	printf("	show tables from DBNAME\n");
	printf("    	Lists tables from given database\n\n");
	
	printf("bulk insert DBNAME/TABLENAME from PATH with (CONFIG)\n");
	printf("Import data from PATH into the specified db/table. If PATH is a directory, its files will be recursively imported.\n");
	printf("TABLENAME is optional and will be defined from the directory name's file if not specified.\n");
	printf("(CONFIG) is a configuration string with the following format:\n");
	printf("    (FILE_DEL=1/0,KEYS=N,MZ=1/0,BIN=1/0,WFP=1/0,OVERWRITE=1/0,SORT=1/0,FIELDS=N,VALIDATE_FIELDS=1/0,VALIDATE_VERSION=1/0,VERBOSE=1/0,COLLATE=1/0,MAX_RECORD=N,TMP_PATH=/path/to/tmp)\n");
	printf("    Where 1/0 represents true/false, and N is an integer.\n");
	printf("    FILE_DEL: Delete file after importation is complete.\n");
	printf("    KEYS: Number of binary keys in the CSV file.\n");
	printf("    MZ: MZ file indicator.\n");
	printf("    WFP: WFP file indicator.\n");
	printf("    OVERWRITE: Overwrite the destination table.\n");
	printf("    SORT: Sort during the importation. Default: 1\n");
	printf("    FIELDS: Number of CSV fields.\n");
	printf("    VALIDATE_FIELDS: Check field quantity during importation. Default: 1\n");
	printf("    VALIDATE_VERSION: Validate version.json. Default: 1\n");
	printf("    VERBOSE: Enable verbose mode. Default: 0\n");
	printf("    THREADS: Define the number of threads to be used during the importation process. Defaul value: half of system available.\n");
	printf("    COLLATE: Perform collation after import, removing data larger than MAX_RECORD bytes. Default: 0\n");
	printf("    MAX_RECORD: define the max record size, if a sector is bigger than \"MAX_RECORD\" bytes will be removed.\n");
	printf("    MAX_RAM_PERCENT: limit the system RAM usage during collate process. Default value: 50.\n");
	printf("    TMP_PATH: Define the temporary directory. Default value \"/tmp\".\n");
	printf("	It is not mandatory to specify all parameters; default values will be assumed for missing parameters.\n\n");

	printf("	bulk insert  DBNAME/TABLENAME from PATH\n");
	printf("    	Import data from PATH into given db/table. If PATH is a directory, the files inside will be recursively imported.\n");
	printf("    	The configuration will be taken from the file \"db.conf\" at %s. A default configuration file will be created if it does not exist\n", LDB_CFG_PATH);
	
	printf("	insert into DBNAME/TABLENAME key KEY hex DATA\n");
	printf("    	Inserts data (hex) into given db/table for the given hex key\n\n");
	
	printf("	insert into DBNAME/TABLENAME key KEY ascii DATA\n");
	printf("    	Inserts data (ASCII) into db/table for the given hex key\n\n");
	
	printf("	select from DBNAME/TABLENAME key KEY\n");
	printf("    	Retrieves all records from db/table for the given hex key (hexdump output)\n\n");
	
	printf("	select from DBNAME/TABLENAME key KEY ascii\n");
	printf("    	Retrieves all records from db/table for the given hex key (ascii output)\n\n");
	
	printf("	select from DBNAME/TABLENAME key KEY csv hex N\n");
	printf("    	Retrieves all records from db/table for the given hex key (csv output, with first N bytes in hex)\n\n");
	
	printf("	delete from DBNAME/TABLENAME max LENGTH keys KEY_LIST\n");
	printf("    	Deletes all records for the given comma separated hex key list from the db/table. Max record length expected\n\n");

	printf("	delete from DBNAME/TABLENAME record CSV_RECORD\n");
	printf("    	Deletes the specific CSV record from the specified table. Some field of the CSV may be skippet from the comparation using '*'\n");
	printf("    	Example 1: delete from db/url record key,madler,*,2.4,20171227,zlib,pkg:github/madler/pigz,https://github.com/madler/pigz/archive/v2.4.zip\n");
	printf("    	All the records matching the all the csv's field with exception of the second thirdone will be removed\n\n");

	printf("	delete from DBNAME/TABLENAME records from PATH\n");
	printf("    	Similar to the previous command, but the records (may be more than one) will be loaded from a csv file in PATH\n\n");
	
	printf("	collate DBNAME/TABLENAME max LENGTH\n");
	printf("    	Collates all lists in a table, removing duplicates and records greater than LENGTH bytes\n\n");
	
	printf("	merge DBNAME/TABLENAME1 into DBNAME/TABLENAME2 max LENGTH\n");
	printf("    	Merges tables erasing tablename1 when done. Tables must have the same configuration\n\n");
	
	printf("	unlink list from DBNAME/TABLENAME key KEY\n");
	printf("    	Unlinks the given list (32-bit KEY) from the sector map\n\n");
	
	printf("	dump DBNAME/TABLENAME hex N [sector N], use 'hex -1' to print the complete register as hex\n");
	printf("    	Dumps table contents with first N bytes in hex\n\n");
	
	printf("	dump keys from DBNAME/TABLENAME [sector N]\n");
	printf("    	Dumps a unique list of existing keys\n\n");
	
	printf("	cat KEY from DBNAME/MZTABLE\n");
	printf("		Shows the contents for KEY in MZ archive\n");

	printf("Other uses\n");
	printf("	ldb -u [--update] path -n[--name] db_name -c[--collate]\n");
	printf("		create \"db_name\" or update a existent one from \"path\". If \"db_name\" is not specified \"oss\" will be used by default.\n");
	printf("		If \"--collate\" option is present, each table will be collated during the importation process.\n");
	printf("		This command is an alias of \"bulk insert\" using the default parameters of an standar ldb\n");
	printf("	ldb -f [filename]	Process a list of commands from a file named filename\n");
}

/**
 * @brief Process and run the user input command
 * 
 * @param raw_command string with the user input
 * @return true if the program must keept running, false otherwise
 */
bool execute(char *raw_command)
{

	char *command = ldb_command_normalize(raw_command);

	// Empty command does nothing
	if (!strlen(command)) return true;

	// QUIT quits
	if (!strcmp(command,"quit")) return false;

	// Parse other commands
	int command_nr = 0;
	int word_nr = 0;
	if (!ldb_syntax_check(command, &command_nr, &word_nr)) 
	{
		printf("E066 Syntax error\n");
		free(command);
		return true;
	}
	switch (command_nr)
	{
		case HELP:
			help();
			break;

		case SHOW_TABLES:
			ldb_command_show_tables(command);
			break;

		case SHOW_DATABASES:
			ldb_command_show_databases();
			break;

		case INSERT_ASCII:
			ldb_command_insert(command, command_nr);
			break;
		
		case BULK_INSERT:
		case BULK_INSERT_DEFAULT:
			ldb_command_bulk(command, command_nr);
			break;

		case INSERT_HEX:
			ldb_command_insert(command, command_nr);
			break;

		case SELECT:
			ldb_command_select(command, HEX);
			break;

		case SELECT_ASCII:
			ldb_command_select(command, ASCII);
			break;

		case SELECT_CSV:
			ldb_command_select(command, CSV);
			break;

		case CREATE_DATABASE:
			ldb_command_create_database(command);
			break;

		case CREATE_TABLE:
			ldb_command_create_table(command);
			break;
		case CREATE_CONFIG:
			ldb_command_create_config(command);
			break;

		case UNLINK_LIST:
			ldb_command_unlink_list(command);
			break;

		case COLLATE:
			ldb_command_collate(command);
			break;

		case DELETE:
			ldb_command_delete(command);
			break;
		case DELETE_RECORD:
		case DELETE_RECORDS:
			ldb_command_delete_records(command);
			break;

		case MERGE:
			ldb_command_merge(command);
			break;

		case DUMP_KEYS:
			ldb_command_dump_keys(command);
			break;

		case VERSION:
			ldb_version(NULL);
			break;

		case DUMP:
			ldb_command_dump(command);
			break;

		case DUMP_SECTOR:
			ldb_command_dump(command);
			break;

		default:
			printf("E067 Command not implemented\n");
			break;
	}

	free(command);
	return true;
}

/**
 * @brief Handle the command line interface
 * To close the program, the variable stay should be set to false
 * 
 * @return true if the program must keept running, false otherwise
 */
bool stdin_handle()
{

	char *command = NULL;
	size_t size = 0;

	if (!getline(&command, &size, stdin)) printf("Warning: cannot read STDIN\n");
	ldb_trim(command);

	bool stay = execute(command);

	free(command);
	return stay;
}

void file_handle(char *filename)
{
    char line[LDB_MAX_PATH];
    
    FILE * cmd_file = fopen(filename, "r");
    if (cmd_file == NULL) 
	{
        printf("Can not open commands file.\n");
        exit(EXIT_FAILURE);
    }
    
    while (fgets(line, sizeof(line), cmd_file) != NULL) 
	{
    	ldb_trim(line);
		execute(line);
    }
    
    fclose(cmd_file);
}

/**
 * @brief Prints the welcome banner
 * 
 */
void welcome()
{
	printf("Welcome to LDB %s\n", LDB_VERSION);
	printf("Use help for a command list and quit for leaving this session\n\n"); 
}

/**
 * @brief Prints the ldb prompt
 * 
 */
void ldb_prompt()
{
	printf("ldb> ");
}

bool is_stdin_off()
{
	struct termios t;
	return (tcgetattr(STDIN_FILENO, &t) == 0);
}

typedef enum
{
	LDB_CONSOLE,
	LDB_UPDATE
} ldb_mode_t;

ldb_mode_t mode = LDB_CONSOLE;
static int collate = 0;
int main(int argc, char **argv)
{
	char * dbname = NULL;//[LDB_MAX_NAME] = "\0";
	char * path = NULL;

	static struct option long_options[] =
        {
          {"version",     no_argument,       0, 'v'},
          {"help",  no_argument,       0, 'h'},
          {"collate",  no_argument, 0 , 'c'},
          {"update",  required_argument, 0, 'u'},
          {"name",    required_argument, 0, 'n'},
		  {"file",    required_argument, 0, 'f'},
		  {"verbose",    no_argument, 0, 'V'},
		  {"quiet",    no_argument, 0, 'q'},
          {0, 0, 0, 0}
        };
    
	/* getopt_long stores the option index here. */
    int option_index = 0;
	int opt;
	bool verbose = false;
    while ( (opt = getopt_long (argc, argv, "u:n:f:qchvV", long_options, &option_index)) >= 0) 
	{
		/* Check valid alpha is entered */
		switch (opt)
		{
			case 'v':
				ldb_version(NULL);
				return EXIT_SUCCESS;
			case 'h':
				help();
				return EXIT_SUCCESS;
			case 'u':
			{
				mode = LDB_UPDATE;
				path = strdup(optarg);
				break;
			}
			case 'n':
			{
				dbname = strdup(optarg);
				break;
			}
			case 'f':
			{
				char * filename = strdup(optarg);
				file_handle(filename);
				free(filename);
				exit(EXIT_SUCCESS);
			}
			case 'c':
				collate = true;
				break;
			case 'V':
			{
				verbose = true;
				break;
			}
			case 'q':
			{
				log_set_quiet(true);
				break;
			}
			default:
			break;
		}
	}
	
	switch (mode)
	{
		case LDB_UPDATE:
		{
			char cmd [LDB_MAX_PATH] = "(VALIDATE_VERSION=1";
			if (*path)
			{
				if (collate)
					strcat(cmd, ",COLLATE=1");

				if (verbose)
					strcat(cmd, ",VERBOSE=1");

				strcat(cmd, ")");
				if (!dbname || !*dbname)
					ldb_import_command("oss", path, cmd);
				else
					ldb_import_command(dbname, path, cmd);
				fprintf(stderr, "\r\nImport process end\n\n");			
				return EXIT_SUCCESS;
			}
			break;
		}
		default:
			break;
	}

	free(dbname);

	bool stdin_off = is_stdin_off();

	if (!ldb_check_root()) return EXIT_FAILURE;

	if (stdin_off) welcome();

	do if (stdin_off) ldb_prompt();
	while (stdin_handle() && stdin_off);
	return EXIT_SUCCESS;

}


