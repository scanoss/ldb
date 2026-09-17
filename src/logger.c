#include <stdarg.h>
#include <sys/ioctl.h>
#include <unistd.h>
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

/* True only while the interactive `ldb import` UI is active. Terminal UI
 * (clear screen, spinner, cursor positioning) belongs to that command only,
 * not to the generic library, so it defaults to OFF. */
static bool import_ui = false;

void logger_set_import_ui(bool enable)
{
    import_ui = enable;
}

/* True only when stderr is an interactive terminal. Terminal UI (clear
 * screen, spinner, cursor positioning) must be emitted ONLY in that case,
 * and always to stderr — never to stdout, which is reserved for data. */
static inline bool logger_is_interactive(void)
{
    return isatty(STDERR_FILENO);
}

/* Terminal UI may be emitted only when the import UI is enabled AND stderr
 * is an interactive terminal. */
static inline bool logger_ui_active(void)
{
    return import_ui && logger_is_interactive();
}

/* Clear the screen by writing the ANSI sequence to stderr instead of
 * shelling out to `clear`, which inherits (and pollutes) stdout. */
void logger_clear_screen(void)
{
    if (logger_ui_active())
    {
        fprintf(stderr, "\033[H\033[2J\033[3J");
        fflush(stderr);
    }
}

void logger_basic(const char * fmt, ...)
{
    if (level != LOG_BASIC || quiet || !logger_ui_active())
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
    logger_basic(NULL);
    
    pthread_mutex_lock(&logger_lock);
    if (!first_cls && !quiet && logger_ui_active())
    {
        logger_clear_screen();
        first_cls = true;
    }
    
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
    if (level > LOG_BASIC && string && !quiet && logger_is_interactive())
    {
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
       
        if (threads_number > 1 && logger_ui_active())
        {
            if (i+logger_offset+threads_number/2 > logger_window.ws_row)
            {
                logger_offset = 0;
                logger_clear_screen();
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

static void logger_write_header(void)
{
    time_t currentTime = time(NULL);
	struct tm *localTime = localtime(&currentTime);
	char timeString[64];
	strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", localTime);
    FILE * f = fopen(import_logger_path, "a");
    if (f)
    {
        fprintf(f, "%s\n", timeString);
        fclose(f);
    }
}

void logger_set_path(const char * path)
{
    if (*import_logger_path || !path || !*path)
        return;

    strncpy(import_logger_path, path, LDB_MAX_PATH - 1);
    logger_write_header();
}

void logger_dbname_set(char * db)
{
    if (*import_logger_path)
        return;

    ldb_prepare_dir(LOGGER_DIR);
    sprintf(import_logger_path, "%s/%s.log", LOGGER_DIR, db);
    logger_write_header();
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


