#ifndef __JOIN_H
#define __JOIN_H
void ldb_join_mz(char * table, char *source, char *destination, bool skip_delete, bool encrypted);
void ldb_join_snippets(char * table, char *source, char *destination, bool skip_delete);
int64_t ldb_bin_join(char *source, char *destination, bool overwrite, bool snippets, bool delete);
#endif
                                                                                                                                              
