#ifndef __COMMAND_H
#define _COMMAND_H
#include "ldb.h"

typedef enum {
HEX,
ASCII,
CSV
} select_format;

typedef enum { 
HELP, 
CREATE_DATABASE, 
CREATE_TABLE,
CREATE_CONFIG,
SHOW_DATABASES,
SHOW_TABLES,
INSERT_ASCII, 
INSERT_HEX, 
SELECT_ASCII,
SELECT_CSV,
SELECT, 
DELETE,
DELETE_RECORD,
DELETE_RECORDS,
COLLATE,
BULK_INSERT,
BULK_INSERT_DEFAULT,
MERGE,
VERSION,
UNLINK_LIST,
DUMP_SECTOR,
DUMP,
DUMP_KEYS,
} commandtype;

void ldb_command_create_database(char *command);
void ldb_command_create_config(char * command);
char *ldb_command_normalize(char *text);
void ldb_command_show_tables(char *command);
void ldb_command_show_databases();
void ldb_command_select(char *command, select_format format);
void ldb_command_telect(char *command);
void ldb_command_insert(char *command, commandtype type);
void ldb_command_create_table(char *command);
void ldb_command_bulk(char *command, commandtype type);
bool ldb_import_command(char * dbtable, char * path, char * config);
void ldb_command_dump(char *command);
void ldb_command_dump_keys(char *command);
void ldb_command_merge(char *command);
void ldb_command_delete(char *command);
void ldb_command_collate(char *command);
void ldb_command_unlink_list(char *command);
void ldb_command_delete_records(char *command);

#endif