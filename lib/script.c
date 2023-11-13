/// Author: Alex G. Lopez
/// File script.c
/// -------------------
/// This file contains the utility functions for processing script files and executing them on
/// custom allocators. We also implement our timing functions and plotting functions for the
/// programs that use this interface. Examine the plot_ functions for more details about how the
/// graphs are formed.
/// Script parsing was written by stanford professors jzelenski and ntroccoli. For the execution
/// and timing functions I took the test_harness.c implementation and stripped out all uneccessary
/// safety and correctness checks so the functions run faster and do not introduce O(n) work.
#include "script.h"
#include "allocator.h"
#include <errno.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/// TYPE DECLARATION

// NOLINTBEGIN(*-swappable-parameters)

typedef unsigned char byte_t;
// Amount by which we resize ops when needed when reading in from file
const int ops_resize_amount = 500;
const int max_script_line_len = 1024;

///  Parse File and Create Script

/// @brief read_script_line  reads one line from the specified file and stores at most buffer_size
///                          characters from it in buffer, removing any trailing newline. It skips
///                          lines that are whitespace or comments (begin with # as first
///                          non-whitespace character).  When reading a line, it increments the
///                          counter pointed to by `pnread` once for each line read/skipped.
/// @param buffer[]          the buffer in which we store the line from the .script line.
/// @param buffer_size       the allowable size for the buffer.
/// @param *fp               the file for which we are parsing requests.
/// @param *pnread           the pointer we use to progress past a line whether it is read or skipped.
/// @return                  true if did read a valid line eventually, or false otherwise.
/// @citation                 jzelenski and ntroccoli Stanford University.
static bool read_script_line( char buffer[], size_t buffer_size, FILE *fp, int *pnread )
{
    while ( true ) {
        if ( fgets( buffer, (int)buffer_size, fp ) == NULL ) {
            return false;
        }

        ( *pnread )++;

        // remove any trailing newline
        if ( buffer[strlen( buffer ) - 1] == '\n' ) {
            buffer[strlen( buffer ) - 1] = '\0';
        }

        /// Stop only if this line is not a comment line (comment lines start
        /// with # as first non-whitespace character)

        char ch = 0;
        if ( sscanf( buffer, " %c", &ch ) == 1 && ch != '#' ) {
            return true;
        }
    }
}

/// @brief parse_script_line  parses the provided line from the script and returns info about it
///                           as a request_t object filled in with the type of the request, the
///                           size, the ID, and the line number.
/// @param *buffer            the individual line we are parsing for a heap request.
/// @param lineno             the line in the file we are parsing.
/// @param *script_name       the name of the current script we can output if an error occurs.
/// @return                   the request_t for the individual line parsed.
/// @warning                  if the line is malformed, this function throws an error.
/// @citation                 jzelenski and ntroccoli Stanford University.
static request_t parse_script_line( char *buffer, int lineno, char *script_name )
{
    request_t request = { .lineno = lineno, .op = 0, .size = 0 };
    char request_char = 0;
    int nscanned = sscanf( buffer, " %c %zu %zu", &request_char, &request.id, &request.size ); // NOLINT
    if ( request_char == 'a' && nscanned == 3 ) {
        request.op = alloc_req;
    } else if ( request_char == 'r' && nscanned == 3 ) {
        request.op = realloc_req;
    } else if ( request_char == 'f' && nscanned == 2 ) {
        request.op = free_req;
    }

    if ( !request.op || request.size > MAX_REQUEST_SIZE ) {
        printf( "Line %d of script file '%s' is malformed.", lineno, script_name );
        abort();
    }

    return request;
}

/// @breif parse_script  parses the script file at the specified path, and returns an object with
///                      info about it.  It expects one request per line, and adds each request's
///                      information to the ops array within the script.  This function throws an
///                      error if the file can't be opened, if a line is malformed, or if the file
///                      is too long to store each request on the heap.
/// @param *path         the path to the .script file to parse.
/// @return              a pointer to the script_t with information regarding the .script requests.
/// @citation            ntroccoli and jzelenski, Stanford University.
script_t parse_script( const char *path )
{
    FILE *fp = fopen( path, "re" );
    if ( fp == NULL ) {
        printf( "Could not open script file \"%s\".", path );
    }

    // Initialize a script object to store the information about this script
    script_t script = { .ops = NULL, .blocks = NULL, .num_ops = 0, .peak_size = 0 };
    const char *basename = strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path;
    strncpy( script.name, basename, sizeof( script.name ) - 1 );
    script.name[sizeof( script.name ) - 1] = '\0';

    int lineno = 0;
    int nallocated = 0;
    size_t maxid = 0;
    char buffer[max_script_line_len];

    for ( int i = 0; read_script_line( buffer, sizeof( buffer ), fp, &lineno ); i++ ) {

        // Resize script->ops if we need more space for lines
        if ( i == nallocated ) {
            nallocated += ops_resize_amount;
            void *new_memory = realloc( script.ops, nallocated * sizeof( request_t ) );
            if ( NULL == new_memory ) {
                free( script.ops );
                printf( "Libc heap exhausted. Cannot continue." );
                abort();
            }
            script.ops = new_memory;
        }

        script.ops[i] = parse_script_line( buffer, lineno, script.name );

        if ( script.ops[i].id > maxid ) {
            maxid = script.ops[i].id;
        }

        script.num_ops = i + 1;
    }

    (void)fclose( fp );
    script.num_ids = maxid + 1;

    script.blocks = calloc( script.num_ids, sizeof( block_t ) );
    if ( !script.blocks ) {
        printf( "Libc heap exhausted. Cannot continue." );
        abort();
    }

    return script;
}

///  Execute Commands in Script Struct

/// @breif exec_malloc     executes a call to mymalloc of the given size.
/// @param req             the request zero indexed within the script.
/// @param requested_size  the block size requested from the client.
/// @param *script         the script_t with information we track from the script file requests.
/// @return                the generic memory provided by malloc for the client. NULL on failure.
static void *exec_malloc( int req, size_t requested_size, script_t *script )
{
    size_t id = script->ops[req].id;
    void *p = mymalloc( requested_size );
    if ( NULL == p && requested_size != 0 ) {
        allocator_error( script, script->ops[req].lineno,
                         "heap exhausted, malloc returned NULL. Script too large or allocator error.\n" );
        abort();
    }

    script->blocks[id] = ( block_t ){ .ptr = p, .size = requested_size };
    return p;
}

/// @brief exec_realloc    executes a call to myrealloc of the given size.
/// @param req             the request zero indexed within the script.
/// @param requested_size  the block size requested from the client.
/// @param *script         the script_t with information we track from the script file requests.
/// @return                the generic memory provided by realloc for the client. NULL on failure.
static void *exec_realloc( int req, size_t requested_size, script_t *script )
{
    size_t id = script->ops[req].id;
    void *oldp = script->blocks[id].ptr;
    void *newp = myrealloc( oldp, requested_size );
    if ( NULL == newp && requested_size != 0 ) {
        allocator_error( script, script->ops[req].lineno,
                         "heap exhausted, realloc returned NULL. Script too large or allocator error.\n" );
        abort();
    }

    script->blocks[id].size = 0;
    script->blocks[id] = ( block_t ){ .ptr = newp, .size = requested_size };
    return newp;
}

/// @brief exec_request  a helper function to execute a single call to the heap allocator. It may
///                      may call malloc(), realloc(), or free().
/// @param *script       the script_t with the information regarding the script file we execute.
/// @param req           the zero-based index of the request to the heap allocator.
/// @param *cur_size     the current size of the heap overall.
/// @param **heap_end    the pointer to the end of the heap, we will adjust if heap grows.
/// @return              0 if there are no errors, -1 if there is an error.
int exec_request( script_t *script, int req, size_t *cur_size, void **heap_end )
{
    size_t id = script->ops[req].id;
    size_t requested_size = script->ops[req].size;

    if ( script->ops[req].op == alloc_req ) {
        void *p = exec_malloc( req, requested_size, script );

        *cur_size += requested_size;
        if ( (byte_t *)p + requested_size > (byte_t *)( *heap_end ) ) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if ( script->ops[req].op == realloc_req ) {
        size_t old_size = script->blocks[id].size;
        void *p = exec_realloc( req, requested_size, script );

        *cur_size += ( requested_size - old_size );
        if ( (byte_t *)p + requested_size > (byte_t *)( *heap_end ) ) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if ( script->ops[req].op == free_req ) {
        size_t old_size = script->blocks[id].size;
        void *p = script->blocks[id].ptr;
        script->blocks[id] = ( block_t ){ .ptr = NULL, .size = 0 };
        myfree( p );
        *cur_size -= old_size;
    }

    if ( *cur_size > script->peak_size ) {
        script->peak_size = *cur_size;
    }
    return 0;
}

///  Time Commands in Script Struct

/// @brief time_malloc     a function that times the speed of one request to malloc on my heap.
/// @param req             the current request we are operating on in the script.
/// @param requested_size  the size in bytes from the script line.
/// @param *script         the script object we are working through for our requests.
/// @param **p             the generic pointer we will use to determine a successfull malloc.
static double time_malloc( size_t req, size_t requested_size, script_t *script, void **p )
{
    size_t id = script->ops[req].id;

    // When measurement times are very low gnuplot points have trouble marking terminal graphs.
    clock_t request_start = 0;
    clock_t request_end = 0;
    request_start = clock();
    *p = mymalloc( requested_size );
    request_end = clock();

    if ( *p == NULL && requested_size != 0 ) {
        allocator_error( script, script->ops[req].lineno,
                         "heap exhausted, malloc returned NULL. Script too large or allocator error.\n" );
        abort();
    }

    script->blocks[id] = ( block_t ){ .ptr = *p, .size = requested_size };
    return ( ( (double)( request_end - request_start ) ) / CLOCKS_PER_SEC ) * 1000;
}

/// @brief time_realloc    a function that times the speed of one request to realloc on my heap.
/// @param req             the current request we are operating on in the script.
/// @param requested_size  the size in bytes from the script line.
/// @param *script         the script object we are working through for our requests.
/// @param **newp          the generic pointer we will use to determine a successfull realloc
static double time_realloc( size_t req, size_t requested_size, script_t *script, void **newp )
{
    size_t id = script->ops[req].id;
    void *oldp = script->blocks[id].ptr;

    // When measurement times are very low gnuplot points have trouble marking terminal graphs.
    clock_t request_start = 0;
    clock_t request_end = 0;
    request_start = clock();
    *newp = myrealloc( oldp, requested_size );
    request_end = clock();

    if ( *newp == NULL && requested_size != 0 ) {
        allocator_error( script, script->ops[req].lineno,
                         "heap exhausted, realloc returned NULL. Script too large or allocator error.\n" );
        abort();
    }

    script->blocks[id].size = 0;
    script->blocks[id] = ( block_t ){ .ptr = *newp, .size = requested_size };
    return ( ( (double)( request_end - request_start ) ) / CLOCKS_PER_SEC ) * 1000;
}

/// @brief time_request  a wrapper function for timer functions that allows us to time a request to
///                      the heap. Returns the cpu time of the request in milliseconds.
/// @param *script       the script object that holds data about our script we need to execute.
/// @param req           the current request to the heap we execute.
/// @param *cur_size     the current size of the heap.
/// @param **heap_end    the address of the end of our range of heap memory.
/// @return              the double representing the time to complete one request.
double time_request( script_t *script, int req, size_t *cur_size, void **heap_end )
{
    size_t id = script->ops[req].id;
    size_t requested_size = script->ops[req].size;

    double cpu_time = 0;
    if ( script->ops[req].op == alloc_req ) {
        void *p = NULL;
        cpu_time = time_malloc( req, requested_size, script, &p );

        *cur_size += requested_size;
        if ( (byte_t *)p + requested_size > (byte_t *)( *heap_end ) ) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if ( script->ops[req].op == realloc_req ) {
        size_t old_size = script->blocks[id].size;
        void *p = NULL;
        cpu_time = time_realloc( req, requested_size, script, &p );

        *cur_size += ( requested_size - old_size );
        if ( (byte_t *)p + requested_size > (byte_t *)( *heap_end ) ) {
            *heap_end = (byte_t *)p + requested_size;
        }
    } else if ( script->ops[req].op == free_req ) {
        size_t old_size = script->blocks[id].size;
        void *p = script->blocks[id].ptr;
        script->blocks[id] = ( block_t ){ .ptr = NULL, .size = 0 };

        // When measurement times are very low gnuplot points have trouble marking terminal graphs.
        clock_t request_start = 0;
        clock_t request_end = 0;
        request_start = clock();
        myfree( p );
        request_end = clock();
        cpu_time = ( ( (double)( request_end - request_start ) ) / CLOCKS_PER_SEC ) * 1000;
        *cur_size -= old_size;
    }

    if ( *cur_size > script->peak_size ) {
        script->peak_size = *cur_size;
    }
    return cpu_time;
}

/// @brief allocator_error  reports an error while running an allocator script.
/// @param *script          the script_t with information we track form the script file requests.
/// @param lineno           the line number where the error occured.
/// @param *format          the specified format string.
void allocator_error( script_t *script, int lineno, char *format, ... )
{
    va_list args;
    (void)fprintf( stdout, "\nALLOCATOR FAILURE [%s, line %d]: ", script->name, lineno );
    va_start( args, format );
    (void)vfprintf( stdout, format, args );
    va_end( args );
    (void)fprintf( stdout, "\n" );
}

// NOLINTEND(*-swappable-parameters)
