# LDB Database

The LDB (Linked-list database) is a headless database management system focused in single-key, read-only application on vast amounts of data, while maintaining a minimal footprint and keeping system calls to a bare minimum. Information is structured using linked lists. 

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

delete KEY from DBNAME/TABLENAME
    Deletes all records for the given hex key in the db/table

collate DBNAME/TABLENAME max LENGTH
    Collates all lists in a table, removing duplicates and records greater than LENGTH bytes

merge DBNAME/TABLENAME1 into DBNAME/TABLENAME2 max LENGTH
    Merges tables erasing tablename1 when done. Tables must have the same configuration

unlink list from DBNAME/TABLENAME key KEY
    Unlinks the given list (32-bit KEY) from the sector map

dump DBNAME/TABLENAME hex N
    Dumps table contents with first N bytes in hex
```

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











