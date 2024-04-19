# LDB Database

The LDB (Linked-list database) is a headless database management system focused in single-key, read-only application on vast amounts of data, while maintaining a minimal footprint and keeping system calls to a bare minimum. Information is structured using linked lists. 

# Build and Install

## Prerequisites
Building LDB requires libgcrypt and zlib. Make sure packages `zlib1g-dev` and `libgcrypt-dev` are installed.

## Build
Run `make all` to build the shell binary and the shared library.

## Install
Run `make install` to copy the binary to `/usr/bin`, the shared library to `/usr/lib` and the header to `/usr/include`.

## Run test
Run `./run_test.sh` to test the ldb binary.

# Features

Some of the features of the LDB are:
 
* Single, fixed size, numeric key (32-bit)
* Single-field records
* Larger keys also are supported by storing exceeded data keys in the data record.
* No indexing: Mapping
* Data tables define either fixed or variable-length data records
* Read-only
* Database updates are performed in a single-threaded, non-disruptive batch operation
* Zlib data compression
* Data records are organized in a linked list
* C library for native development
* LDB shell allows interaction with external languages

## LDB Shell Commands

```
create database DBNAME
    Creates an empty database

create table DBNAME/TABLENAME keylen N reclen N
    Creates an empty table in the given database with
    the specified key length (>= 4) and record length (0=variable)

show databases
    Lists databases

show tables from DBNAME
    Lists tables from given database

bulk insert DBNAME/TABLENAME from PATH with (CONFIG)
    Imports data from PATH into the specified db/table. If PATH is a directory, its files will be recursively imported.
    TABLENAME is optional and will be derived from the directory name's file if not specified.

    (CONFIG) is a configuration string with the following format:
        (FILE_DEL=1/0,KEYS=N,MZ=1/0,WFP=1/0,OVERWRITE=1/0,SORT=1/0,FIELDS=N,VALIDATE_FIELDS=1/0,VALIDATE_VERSION=1/0,VERBOSE=1/0,COLLATE=1/0,MAX_RECORD=N,TMP_PATH=/path/to/tmp)
        
        Where 1/0 represents "true" / "false", and N is an integer.
        FILE_DEL: Delete file after importation is completed.
        KEYS: Number of binary keys in the CSV file.
        MZ: MZ file indicator.
        WFP: WFP file indicator.
        OVERWRITE: Overwrite the destination table. (Default: 0).
        SORT: Sort the tuples during the import process. (Default: 1).
        FIELDS: Number of CSV fields.
        VALIDATE_FIELDS: Check field quantity during importation. (Default: 1).
        VALIDATE_VERSION: Validate version.json.(Default: 1).
        VERBOSE: Enable verbose mode. (Default: 0).
        THREADS: Define the number of threads to be used during the importation process. Defaul value: half of system available.
        COLLATE: Perform collation after import, removing data larger than MAX_RECORD bytes. (Default: 0).
        MAX_RECORD: Maximum record size in bytes (Default value: 2048).
        MAX_RAM_PERCENT: limit the system RAM usage during collate process. Default value: 50.
        TMP_PATH: Path to the folder used for temporary files (default: /tmp).

bulk insert DBNAME/TABLENAME from PATH
    Imports data from PATH into the specified db/table. If PATH is a directory, its files will be recursively imported.
    The configuration will be retrieved from the "db.conf" file located at "/etc/local/scanoss/ldb/". A default configuration file will be created if it doesn't exist.

insert into DBNAME/TABLENAME key KEY hex DATA
    Inserts data (hex) into given db/table for the given hex key

insert into DBNAME/TABLENAME key KEY ascii DATA
    Inserts data (ASCII) into db/table for the given hex key

select from DBNAME/TABLENAME key KEY
    Retrieves all records from db/table for the given hex key (hexdump output)

select from DBNAME/TABLENAME key KEY ascii
    Retrieves all records from db/table for the given hex key (ascii output)

select from DBNAME/TABLENAME key KEY csv hex N
    Retrieves all records from db/table for the given hex key (csv output, with first N bytes in hex)

delete from DBNAME/TABLENAME max LENGTH keys KEY_LIST
    Deletes all records for the given comma separated hex key list from the db/table. Max record length expected

delete from DBNAME/TABLENAME record CSV_RECORD\n"
	Deletes the specific CSV record from the specified table. Some field of the CSV may be skippet from the comparation using '*'
	Example: delete from db/url record key,madler,*,2.4,20171227,zlib,pkg:github/madler/pigz,https://github.com/madler/pigz/archive/v2.4.zip
	All the records matching the all the csv's field with exception of the second thirdone will be removed.

delete from DBNAME/TABLENAME records from PATH\
	Similar to the previous command, but the records (may be more than one) will be loaded from a csv file in PATH.

collate DBNAME/TABLENAME max LENGTH
    Collates all lists in a table, removing duplicates and records greater than LENGTH bytes

merge DBNAME/TABLENAME1 into DBNAME/TABLENAME2 max LENGTH
    Merges tables erasing tablename1 when done. Tables must have the same configuration

unlink list from DBNAME/TABLENAME key KEY
    Unlinks the given list (32-bit KEY) from the sector map

dump DBNAME/TABLENAME hex N
    Dumps table contents with first N bytes in hex

dump keys from DBNAME/TABLENAME [sector N]
    Dumps a unique list of existing keys
```
## Other Uses

### Update Database
```bash
ldb -u [--update] path -n[--name] db_name -c[--collate]
```
Create or update an existing database from "path." If "db_name" is not specified, "oss" will be used by default. If the "--collate" option is present, each table will be collated during the importation process.

### Process Commands from File
```bash
ldb -f [filename]
```
Process a list of commands from a file named "filename."q

# Using the shell

The following example explains how to create a database, a table, a record, and querying that record.

```
$ echo "create database test" | ldb
OK
$ echo "create table test/table1 keylen 16 reclen variable" | ldb
OK
$ echo "insert into test/table1 key 26e3a3bd01585cde84408f01fe981f6a ascii THIS_IS_A_TEST" | ldb
$ echo "select from test/table1 key 26e3a3bd01585cde84408f01fe981f6a" | ldb
0000  544849535f49535f415f54455354      THIS_IS_A_TEST  
$ echo "select from test/table1 key 26e3a3bd01585cde84408f01fe981f6a ascii" | ldb
THIS_IS_A_TEST
```

# License

The LDB is released under the GPL 2.0 license. See the LICENSE file for more infomation.
 
Copyright (C) 2018-2020 SCANOSS.COM
http://scanoss.com













