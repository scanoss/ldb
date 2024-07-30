#include <stdarg.h>
#include <sys/ioctl.h>
#include "ldb.h"
#include "logger.h"
#include <time.h>
#include <sys/time.h>

int logger_offset = 0;
struct winsize logger_window;
#define gotoxy(x,y) fprintf(stderr,"\033[%d;%dH", (y), (x))

#define LOGGER_DIR "/var/log/scanoss/ldb/"
char import_logger_path[LDB_MAX_PATH] = "\0";
static pthread_mutex_t logger_lock;
static pthread_t * threads_list = NULL;
static int threads_number = 0;
static log_level_t level = LOG_BASIC;
static double progress_timer = 0;

char animation[] = {'|', '/', '-','\\'};
int animation_index = 0;

int yi = 0;
static bool first_cls = false;
static bool quiet = false;

void logger_basic(const char * fmt, ...)
{
    if (level != LOG_BASIC || quiet)
        return;
    pthread_mutex_lock(&logger_lock);
    if (animation_index >= sizeof(animation))
        animation_index = 0;

    struct timeval t;
	gettimeofday(&t, NULL);
    double tmp = (double)(t.tv_usec) / 1000000 + (double)(t.tv_sec);
	
    if (!fmt && tmp - progress_timer < 2)
    {
	    pthread_mutex_unlock(&logger_lock);
        return;
    }
    
    progress_timer = tmp;

    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        char * string;
        vasprintf(&string, fmt, args);
        fprintf(stderr, "\33[2K\r%c  Import in progress: %s", animation[animation_index], string);
        fflush(stderr);
        free(string);
        va_end(args);
    }
    else
    {
        fprintf(stderr, "\r%c", animation[animation_index]);
    }
    animation_index++;
    pthread_mutex_unlock(&logger_lock);
}

void log_info(const char * fmt, ...)
{
    //logger_basic(NULL);
    
    pthread_mutex_lock(&logger_lock);
    
    char * string = NULL;
    //save to log file
    if (fmt)
    {
        va_list args;
        va_start(args, fmt);
        vasprintf(&string, fmt, args);
        va_end(args);
        FILE * f = fopen(import_logger_path, "a");
        if (f)
        {
            fprintf(f, "%s", string);
            if (!strrchr(string, '\n'))
                fprintf(f, "\n");
            fclose(f);
        }
    }
    //print on stderr
    if (level > LOG_BASIC && string && !quiet)
    {
        if (!first_cls && !quiet)
        {
            system("clear");
            first_cls = true;
        }
        pthread_t t = pthread_self();
        int i = 0;
        bool found = false;
        if (threads_list)
        {
            for (; i < threads_number; i++)
            {
                if (t == threads_list[i])
                {
                    found = true;
                    break;
                }
            }
        }
        if (!found)
            i = 0;
       
        if (threads_number > 1)
        {
            if (i+logger_offset+threads_number/2 > logger_window.ws_row)
            {
                logger_offset = 0;
                system("clear");
            }
            gotoxy(0, i + 1 + logger_offset);
            fprintf(stderr, "\33[2K\r");
            gotoxy(1, i + 1 + logger_offset);
            fprintf(stderr, "Thread %d: ", i);
        }
        fprintf(stderr, "%s\r", string);
    }
	pthread_mutex_unlock(&logger_lock);
}

void logger_dbname_set(char * db)
{
    if (*import_logger_path)    
        return;
    
    ldb_prepare_dir(LOGGER_DIR);
    sprintf(import_logger_path, "%s/%s.log", LOGGER_DIR, db);
    time_t currentTime = time(NULL);
	struct tm *localTime = localtime(&currentTime);
	char timeString[64];
	strftime(timeString, sizeof(timeString), "Exec Time: %Y-%m-%d %H:%M:%S", localTime);
    FILE * f = fopen(import_logger_path, "a");
    if (f)
    {
        fprintf(f, "%s\n", timeString);
        fclose(f);
    }
}

void logger_init(char * db, int tnumber,  pthread_t * tlist)
{
    pthread_mutex_init(&logger_lock, NULL);
    
    if (tlist)
    {
        threads_list = tlist;
        threads_number = tnumber;
    }

    int stdout_fd = fileno(stderr);
    ioctl(stdout_fd, TIOCGWINSZ, &logger_window);
	
    logger_dbname_set(db);
}

void logger_offset_increase(int off)
{
    if (level > LOG_BASIC)
        logger_offset += off;
    fflush(stderr);
}

void logger_set_level(log_level_t l)
{
    level = l;
}

void log_debug(const char * fmt, ...)
{
    if (level > LOG_INFO)
    {
        va_list args;
        va_start(args, fmt);
        char * string;
        vasprintf(&string, fmt, args);
        log_info("%s", string);
        free(string);
        va_end(args);
    }
}

void log_set_quiet(bool mode)
{
    quiet = mode;
}


