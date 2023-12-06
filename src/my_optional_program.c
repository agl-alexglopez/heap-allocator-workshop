/// File: my_optional_program.c
/// ---------------------------
///  This is my unique line printing program from assignment three. A range of heap actions occur.

#include "allocator.h"
#include "segment.h"
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/// initial allocation will be for min size, if not big enough, doubles
/// to 64, then 128, then 256, etc. as needed to accommodate the entire line
/// resize-as-you-go, doubling each time.
enum buffer_limits
{
    MINIMUM_SIZE = 32,
    NEW_LINE = '\n',
    NULL_TERMINATOR = '\0',
    /// initial estimate of number of uniq lines
    /// resize-as-you-go, add in increments of 100
    ESTIMATE = 100,
};

#define HEAP_SIZE 1L << 32

// "freq" abbreviation used throughout means frequency of occurences of the associated text line.
struct freq_cell
{
    int freq;
    char *text;
};

///  Function Prototypes

// Driver function for printing.
void print_uniq_lines( FILE *file_pointer );
// Helper functions that manage the array of structs
struct freq_cell *fill_freq_array( FILE *file_pointer, size_t increment, size_t *size );
bool is_added( char *heap_line, struct freq_cell *freq_array, size_t index );
struct freq_cell *realloc_array( struct freq_cell *freq_array, size_t total_space );
char *read_line( FILE *file_pointer );
bool initialize_heap_allocator();

///     Main Program

int main( int argc, char *argv[] )
{
    if ( !initialize_heap_allocator() ) {
        return 1;
    }
    FILE *file_pointer = NULL;

    if ( argc == 1 ) {
        // User types or hits <Enter> to create multiple lines of text input. <Ctrl-d> stops input.
        file_pointer = stdin;
    } else {
        file_pointer = fopen( argv[1], "re" );
        if ( file_pointer == NULL ) {
            printf( "cannot access '%s'", argv[1] );
            (void)raise( SIGABRT );
        }
    }

    print_uniq_lines( file_pointer );
    (void)fclose( file_pointer );
    return 0;
}

/// Function Implementations

void print_uniq_lines( FILE *file_pointer )
{
    size_t array_size = 0;
    struct freq_cell *freq_array = fill_freq_array( file_pointer, ESTIMATE, &array_size );
    // First elem will be NULL sentinel if there were no strings so we will just free array.
    for ( size_t i = 0; i < array_size; i++ ) {
        printf( "%7d %s\n", freq_array[i].freq, freq_array[i].text );
        myfree( freq_array[i].text );
    }
    myfree( freq_array );
}

struct freq_cell *fill_freq_array( FILE *file_pointer, size_t increment, size_t *size )
{
    size_t total_space = increment;
    struct freq_cell *freq_array = mymalloc( total_space * sizeof( struct freq_cell ) );
    assert( freq_array );

    size_t index = 0;
    char *current_line = NULL;
    // Make final text NULL. Makes a cleaner pointer return and we can still find the end of array.
    while ( ( current_line = read_line( file_pointer ) ) ) {
        if ( index == total_space ) {
            freq_array = realloc_array( freq_array, total_space += increment );
        }
        is_added( current_line, freq_array, index ) ? ++index : myfree( current_line );
    }
    *size = index;
    return freq_array;
}

bool is_added( char *heap_line, struct freq_cell *freq_array, size_t index )
{
    if ( index != 0 ) {
        size_t i = 0;
        while ( i < index && strcmp( heap_line, freq_array[i].text ) != 0 ) {
            ++i;
        }
        if ( i < index ) {
            ++freq_array[i].freq;
            return false;
        }
    }
    // Uninitialized frequencies will throw off the counts.
    freq_array[index].freq = 0;
    freq_array[index].text = heap_line;
    ++freq_array[index].freq;
    return true;
}

struct freq_cell *realloc_array( struct freq_cell *freq_array, size_t total_space )
{
    struct freq_cell *more_space = myrealloc( freq_array, total_space * sizeof( struct freq_cell ) );
    assert( more_space );
    freq_array = more_space;
    return freq_array;
}

/// @warning it is the user's responsibility to free the heap allocated string. This function also
/// assumes we will not encounter a string larger than size_t.
char *read_line( FILE *file_pointer )
{
    size_t heap_size = MINIMUM_SIZE;
    char *heap_string = mymalloc( heap_size );
    assert( heap_string );

    // We need another pointer to our heap in case fgets returns NULL. Otherwise, heap is lost.
    char *more_space = heap_string;
    more_space = fgets( more_space, (int)heap_size, file_pointer );
    if ( more_space == NULL ) {
        myfree( heap_string );
        return NULL;
    }

    // We will need to maintain a length variable to quickly get rid of the '\n' char.
    size_t newline_char = strlen( heap_string ) - 1;
    while ( heap_string[newline_char] != NEW_LINE ) {
        heap_size <<= 1;
        char *check = myrealloc( heap_string, heap_size );
        assert( check );
        heap_string = check;

        // Now we need to put more of our string into the buffer.
        more_space = heap_string + newline_char + 1;
        more_space = fgets( more_space, (int)( heap_size >> 1 ), file_pointer );
        if ( more_space == NULL ) {
            // We can be nice and clean up our doubled memory as that could be quite large.
            assert( myrealloc( heap_string, heap_size >> 1 ) );
            return heap_string;
        }
        newline_char += strlen( more_space );
    }
    heap_string[newline_char] = NULL_TERMINATOR;
    return heap_string;
}

bool initialize_heap_allocator()
{
    init_heap_segment( HEAP_SIZE );
    return myinit( heap_segment_start(), heap_segment_size() );
}
