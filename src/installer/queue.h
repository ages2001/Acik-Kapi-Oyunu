#ifndef WINRAM_QUEUE_H
#define WINRAM_QUEUE_H
#include "common.h"
typedef struct {
    char magic[8];
    int id;
    Options opts;
} Job;
extern char hostroot[4];
int write_job(const char *file, const char *path, int id, const Options *opt);
int read_job(const char *file, Job *job);
int run_queue(void);
#endif
