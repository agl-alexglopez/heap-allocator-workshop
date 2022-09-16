#ifndef _SCRIPT_H_
#define _SCRIPT_H_
#include <stdio.h>
#include "allocator.h"

// enum and struct for a single allocator request
enum request_type {
    ALLOC = 1,
    FREE,
    REALLOC
};
typedef struct {
    enum request_type op;   // type of request
    int id;                 // id for free() to use later
    size_t size;            // num bytes for alloc/realloc request
    int lineno;             // which line in file
} request_t;

// struct for facts about a single malloc'ed block
typedef struct {
    void *ptr;
    size_t size;
} block_t;

// struct for info for one script file
typedef struct {
    char name[128];         // short name of script
    request_t *ops;         // array of requests read from script
    int num_ops;            // number of requests
    int num_ids;            // number of distinct block ids
    block_t *blocks;        // array of memory blocks malloc returns when executing
    size_t peak_size;       // total payload bytes at peak in-use
} script_t;

/* @breif parse_script  parses the script file at the specified path, and returns an object with
 *                      info about it.  It expects one request per line, and adds each request's
 *                      information to the ops array within the script.  This function throws an
 *                      error if the file can't be opened, if a line is malformed, or if the file
 *                      is too long to store each request on the heap.
 * @param *path         the path to the .script file to parse.
 * @return              a pointer to the script_t with information regarding the .script requests.
 */
script_t parse_script(const char *filename);

/* @brief read_script_line  reads one line from the specified file and stores at most buffer_size
 *                          characters from it in buffer, removing any trailing newline. It skips lines
 *                          that are all-whitespace or that contain comments (begin with # as first
 *                          non-whitespace character).  When reading a line, it increments the
 *                          counter pointed to by `pnread` once for each line read/skipped.
 * @param buffer[]          the buffer in which we store the line from the .script line.
 * @param buffer_size       the allowable size for the buffer.
 * @param *fp               the file for which we are parsing requests.
 * @param *pnread           the pointer we use to progress past a line whether it is read or skipped.
 * @return                  true if did read a valid line eventually, or false otherwise.
 */
bool read_script_line(char buffer[], size_t buffer_size, FILE *fp, int *pnread);

/* @brief parse_script_line  parses the provided line from the script and returns info about it
 *                           as a request_t object filled in with the type of the request, the
 *                           size, the ID, and the line number.
 * @param *buffer            the individual line we are parsing for a heap request.
 * @param lineno             the line in the file we are parsing.
 * @param *script_name       the name of the current script we can output if an error occurs.
 * @return                   the request_t for the individual line parsed.
 * @warning                  if the line is malformed, this function throws an error.
 */
request_t parse_script_line(char *buffer, int lineno, char *script_name);

/* @brief allocator_error  reports an error while running an allocator script.
 * @param *script          the script_t with information we track form the script file requests.
 * @param lineno           the line number where the error occured.
 * @param *format          the specified format string.
 */
void allocator_error(script_t *script, int lineno, char* format, ...);

#endif
