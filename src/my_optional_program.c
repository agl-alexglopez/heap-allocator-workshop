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
enum
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
typedef struct freq_cell
{
    int freq;
    char *text;
} freq_cell;

///  Function Prototypes

// Driver function for printing.
void print_uniq_lines( FILE *file_pointer );
// Helper functions that manage the array of structs
freq_cell *fill_freq_array( FILE *file_pointer, size_t increment, size_t *size );
bool is_added( char *heap_line, freq_cell *freq_array, size_t index );
freq_cell *realloc_array( freq_cell *freq_array, size_t total_space );
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

/// @brief print_uniq_lines  prints lines in a file and their frequencies in order of first
///                          appearance in that file.
/// @param *file_pointer     the pointer to the file for which we will print frequencies
void print_uniq_lines( FILE *file_pointer )
{
    size_t array_size = 0;
    freq_cell *freq_array = fill_freq_array( file_pointer, ESTIMATE, &array_size );
    // First elem will be NULL sentinel if there were no strings so we will just free array.
    for ( size_t i = 0; i < array_size; i++ ) {
        printf( "%7d %s\n", freq_array[i].freq, freq_array[i].text );
        myfree( freq_array[i].text );
    }
    myfree( freq_array );
}

/// @brief *fill_freq_array  reads all lines of a file and organizes them into an array of frequency
///                          cell structs that tally the number of times we have seen a given line
///                          in order of that line's first appearance in the file. This function
///                          returns a pointer to the heap allocated array. The user may determine
///                          the starting size that will also act as the size used to expand the
///                          array when needed.
/// @param *file_pointer     the pointer to the file for which we will tally line counts.
/// @param increment         first size for the array. It will expand by this amount when needed.
/// @return                  the pointer to the start of our heap allocated frequency array.
/// @warning                 it is the user's responsibility to free heap allocated strings within
///                          the zero indexed array and free the heap array. The array at the index
///                          of its size will have the text field set to NULL, including size 0.
freq_cell *fill_freq_array( FILE *file_pointer, size_t increment, size_t *size )
{
    size_t total_space = increment;
    freq_cell *freq_array = mymalloc( total_space * sizeof( freq_cell ) );
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

/// @param is_added    takes a heap string and inserts it into a frequency array if it is unique.
///                    This function will increment the frequency field of the struct in the array
///                    if it is a repeat or set the frequency to one if it is newly added.
/// @param *heap_line  the heap string that we will insert if it is unique for the array.
/// @param *freq_array the freq_cell array we are checking our heap string against.
/// @param index       the potential index that we may insert an element into.
/// @return            true if the heap string was inserted, false if it was not. The function will
///                    increment the frequency field of the new string or already present string.
bool is_added( char *heap_line, freq_cell *freq_array, size_t index )
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

/// @brief realloc_array  reallocates space for our heap array of frequency cells to expand.
/// @param *freq_array    the pointer to the freq_cell array we are tasked with expanding.
/// @param total_space    the current size of the array before expansion.
/// @return               a pointer to the newly allocated heap array of freq_cells.
/// @warning              it is the user's responsibility to make sure the freq_array points
///                       to previously allocated heap memory and to keep track of the current
///                       size of the allocated heap memory for the array.
freq_cell *realloc_array( freq_cell *freq_array, size_t total_space )
{
    freq_cell *more_space = myrealloc( freq_array, total_space * sizeof( freq_cell ) );
    assert( more_space );
    freq_array = more_space;
    return freq_array;
}

/// @brief *read_line     takes a FILE pointer and returns one complete line of text from the file
///                       it points to. It returns a heap allocated string without its newline
///                       character or NULL if there is no string or only a new line character.
/// @param *file_pointer  the FILE pointer we read from and progress to the next line.
/// @return               a heap allocated string without the '\n' or NULL if no string is found.
/// @warning              it is the user's responsibility to free the heap allocated string. This
///                       function also assumes we will not encounter a string larger than size_t.
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

/// @brief initialize_heap_allocator  because these allocators are in a controlled environment for
///                                   display and testing purposes, we initialize our own segment.
/// @return                           true if successful false if failure.
bool initialize_heap_allocator()
{
    init_heap_segment( HEAP_SIZE );
    return myinit( heap_segment_start(), heap_segment_size() );
}
