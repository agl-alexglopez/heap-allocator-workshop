///
/// File list_segregated_design.c
/// ---------------------------------
/// This file contains the implementation of utility functions for the
/// list_segregated heap allocator. These functions serve as basic navigation for
/// nodes and blocks. They are inlined in the header file so must be declared
/// here. Hopefully the compiler inlines them in "hot-spot" functions.
#include "list_segregated_utilities.h"
#include "debug_break.h"
#include "print_utility.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct header_size
{
    header header;
    size_t size;
} header_size;

typedef struct bad_jump
{
    header *current;
    header *prev;
} bad_jump;

/////////////////////////////    Debugging and Testing Functions    /////////////////////////

/// @brief is_header_corrupted   will determine if a block has the 3rd bit on,
/// which is invalid.
/// @param *cur_header_location  the valid header we will determine status for.
/// @return                      true if the block has the second or third bit
/// on.
bool is_header_corrupted( header header_val ) { return header_val & STATUS_CHECK; }

/// @breif check_init   checks the internal representation of our heap,
/// especially the
///                     head and tail nodes for any issues that would ruin our
///                     algorithms.
/// @param table[]      the lookup table that holds the list size ranges
/// @param *nil         a special free_node that serves as a sentinel for logic
/// and edgecases.
/// @param client_size  the total space available for client.
/// @return             true if everything is in order otherwise false.
bool check_init( seg_node table[], free_node *nil, size_t client_size )
{
    void *first_address = table;
    void *last_address = (byte *)nil + FREE_NODE_WIDTH;
    if ( (size_t)( (byte *)last_address - (byte *)first_address ) != client_size ) {
        breakpoint();
        return false;
    }
    // Check our lookup table. Sizes should never be altered and pointers should
    // never be NULL.
    unsigned short size = MIN_BLOCK_SIZE;
    for ( size_t i = 0; i < SMALL_TABLE_SIZE; i++, size += HEADERSIZE ) {
        if ( table[i].size != size ) {
            breakpoint();
            return false;
        }
        // This should either be a valid node or the sentinel.
        if ( NULL == table[i].start ) {
            breakpoint();
            return false;
        }
    }
    size = LARGE_TABLE_MIN;
    for ( size_t i = SMALL_TABLE_SIZE; i < TABLE_SIZE - 1; i++, size *= 2 ) {
        if ( table[i].size != size ) {
            breakpoint();
            return false;
        }
        // This should either be a valid node or the nil.
        if ( NULL == table[i].start ) {
            breakpoint();
            return false;
        }
    }
    if ( table[TABLE_SIZE - 1].size != USHRT_MAX ) {
        breakpoint();
        return false;
    }
    return true;
}

/// @brief is_valid_header  checks the header of a block of memory to make sure
/// that is
///                         not an unreasonable size or otherwise corrupted.
/// @param *cur_header      the header to a block of memory
/// @param block_size       the reported size of this block of memory from its
/// header.
/// @client_size            the entire space available to the user.
/// @return                 true if the header is valid, false otherwise.
bool is_valid_header( header_size hs, size_t client_size )
{
    // Most definitely impossible and our header is corrupted. Pointer arithmetic
    // would fail.
    if ( hs.size > client_size ) {
        return false;
    }
    if ( is_header_corrupted( hs.header ) ) {
        return false;
    }
    if ( hs.size % HEADERSIZE != 0 ) {
        return false;
    }
    return true;
}

/// @brief is_memory_balanced  loops through all blocks of memory to verify that
/// the sizes
///                            reported match the global bookeeping in our
///                            struct.
/// @param *total_free_mem     the output parameter of the total size used as
/// another check.
/// @param *client_start       the start address of the client heap segment.
/// @param *client_end         the end address of the client heap segment.
/// @param client_size         the size in bytes of the total space available
/// for client.
/// @param fits_total          the total number of free nodes in our table of
/// lists.
/// @return                    true if our tallying is correct and our totals
/// match.
bool is_memory_balanced( size_t *total_free_mem, heap_range hr, size_total st )
{
    // Check that after checking all headers we end on size 0 tail and then end of
    // address space.
    header *cur_header = hr.start;
    size_t size_used = FREE_NODE_WIDTH + TABLE_BYTES;
    size_t total_free_nodes = 0;
    while ( cur_header != hr.end ) {
        size_t block_size_check = get_size( *cur_header );
        if ( block_size_check == 0 ) {
            breakpoint();
            return false;
        }

        if ( !is_valid_header( ( header_size ){ *cur_header, block_size_check }, st.size ) ) {
            breakpoint();
            return false;
        }
        if ( is_block_allocated( *cur_header ) ) {
            size_used += block_size_check;
        } else {
            total_free_nodes++;
            *total_free_mem += block_size_check;
        }
        cur_header = get_right_header( cur_header, block_size_check );
    }
    if ( size_used + *total_free_mem != st.size ) {
        breakpoint();
        return false;
    }
    if ( total_free_nodes != st.total ) {
        breakpoint();
        return false;
    }
    return true;
}

/// @brief are_fits_valid  loops through only the segregated fits list to make
/// sure it
///                        matches the loop we just completed by checking all
///                        blocks.
/// @param total_free_mem  the input from a previous loop that was completed by
///                        jumping block by block over the entire heap.
/// @param table[]         the lookup table that holds the list size ranges
/// @param *nil            a special free_node that serves as a sentinel for
/// logic and edgecases.
/// @return                true if the segregated fits list totals correctly,
/// false if not.
bool are_fits_valid( size_t total_free_mem, seg_node table[], free_node *nil )
{
    size_t linked_free_mem = 0;
    for ( size_t i = 0; i < TABLE_SIZE; i++ ) {
        for ( free_node *cur = table[i].start; cur != nil; cur = cur->next ) {
            header *cur_header = get_block_header( cur );
            size_t cur_size = get_size( *cur_header );
            if ( i != TABLE_SIZE - 1 && cur_size >= table[i + 1].size ) {
                breakpoint();
                return false;
            }
            if ( is_block_allocated( *cur_header ) ) {
                breakpoint();
                return false;
            }
            // This algorithm does not allow two free blocks to remain next to one
            // another.
            if ( is_left_space( get_block_header( cur ) ) ) {
                breakpoint();
                return false;
            }
            linked_free_mem += cur_size;
        }
    }
    if ( total_free_mem != linked_free_mem ) {
        breakpoint();
        return false;
    }
    return true;
}

/////////////////////////////        Printing Functions     //////////////////////////////////

/// @brief print_fits  prints the segregated fits free list in order to check if
/// splicing and
///                    adding is progressing correctly.
/// @param table[]     the lookup table that holds the list size ranges
/// @param *nil        a special free_node that serves as a sentinel for logic
/// and edgecases.
void print_fits( print_style style, seg_node table[], free_node *nil )
{
    bool alternate = false;
    for ( size_t i = 0; i < TABLE_SIZE; i++, alternate = !alternate ) {
        printf( COLOR_GRN );
        if ( style == VERBOSE ) {
            printf( "%p: ", &table[i] );
        }
        if ( i == TABLE_SIZE - 1 ) {
            printf( "[CLASS:%ubytes+]=>", table[i].size );
        } else if ( i >= SMALL_TABLE_SIZE ) {
            printf( "[CLASS:%u-%ubytes]=>", table[i].size, table[i + 1].size - 1U );
        } else {
            printf( "[CLASS:%ubytes]=>", table[i].size );
        }
        printf( COLOR_NIL );
        if ( alternate ) {
            printf( COLOR_RED );
        } else {
            printf( COLOR_CYN );
        }

        for ( free_node *cur = table[i].start; cur != nil; cur = cur->next ) {
            if ( cur ) {
                header *cur_header = get_block_header( cur );
                printf( "<=>[" );
                if ( style == VERBOSE ) {
                    printf( "%p:", get_block_header( cur ) );
                }
                printf( "(%zubytes)]", get_size( *cur_header ) );
            } else {
                printf( "Something went wrong. NULL free fits node.\n" );
                break;
            }
        }
        printf( "<=>[%p]\n", nil );
        printf( COLOR_NIL );
    }
}

/// @brief print_alloc_block  prints the contents of an allocated block of
/// memory.
/// @param *cur_header        a valid header to a block of allocated memory.
static void print_alloc_block( header *cur_header )
{
    size_t block_size = get_size( *cur_header ) - HEADERSIZE;
    printf( COLOR_GRN );
    // We will see from what direction our header is messed up by printing 16
    // digits.
    printf( "%p: HEADER->0x%016zX->[ALOC-%zubytes]\n", cur_header, *cur_header, block_size );
    printf( COLOR_NIL );
}

/// @brief print_free_block  prints the contents of a free block of heap memory.
/// @param *cur_header       a valid header to a block of allocated memory.
static void print_free_block( header *cur_header )
{
    size_t full_size = get_size( *cur_header );
    size_t block_size = full_size - HEADERSIZE;
    header *footer = (header *)( (byte *)cur_header + full_size - HEADERSIZE );
    // We should be able to see the header is the same as the footer. If they are
    // not the same we will face subtle bugs that are very hard to notice.
    //
    if ( *footer != *cur_header ) {
        *footer = ULONG_MAX;
    }
    printf( COLOR_RED );
    printf( "%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n", cur_header, *cur_header, block_size,
            *footer );
    printf( COLOR_NIL );
}

/// @brief print_bad_jump  If we overwrite data in a header, this print
/// statement will help us
///                        notice where we went wrong and what the addresses
///                        were.
/// @param *current        the current node that is likely garbage values that
/// don't make sense.
/// @param *prev           the previous node that we jumped from.
/// @param table[]         the lookup table that holds the list size ranges
/// @param *nil            a special free_node that serves as a sentinel for
/// logic and edgecases.
static void print_bad_jump( bad_jump j, seg_node table[], free_node *nil )
{
    size_t prev_size = get_size( *j.prev );
    size_t cur_size = get_size( *j.current );
    printf( COLOR_CYN );
    printf( "A bad jump from the value of a header has occured. Bad distance to "
            "next header.\n" );
    printf( "The previous address: %p:\n", j.prev );
    printf( "\tHeader Hex Value: %016zX:\n", *j.prev );
    printf( "\tBlock Byte Value: %zubytes:\n", prev_size );
    printf( "\nJump by %zubytes...\n", prev_size );
    printf( "The current address: %p:\n", j.current );
    printf( "\tHeader Hex Value: %016zX:\n", *j.current );
    printf( "\tBlock Byte Value: %zubytes:\n", cur_size );
    printf( "\nJump by %zubytes...\n", cur_size );
    printf( "Current state of the free list:\n" );
    printf( COLOR_NIL );
    print_fits( VERBOSE, table, nil );
}

/// @brief print_all    prints our the complete status of the heap, all of its
/// blocks, and
///                     the sizes the blocks occupy. Printing should be clean
///                     with no overlap of unique id's between heap blocks or
///                     corrupted headers.
/// @param client_start the starting address of the heap segment.
/// @param client_end   the final address of the heap segment.
/// @param client_size  the size in bytes of the heap.
/// @param table[]      the lookup table of segregated sizes of nodes stored in
/// each slot.
/// @param *nil         the free node that serves as a universal head and tail
/// to all lists.
void print_all( heap_range hr, size_t client_size, seg_node table[], free_node *nil )
{
    header *cur_header = hr.start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            cur_header, hr.end, client_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n\n" );

    printf( "%p: FIRST ADDRESS\n", table );

    // This will create large amount of output but realistically table is before
    // the rest of heap.
    print_fits( VERBOSE, table, nil );
    printf( "--END OF LOOKUP TABLE, START OF HEAP--\n" );

    header *prev = cur_header;
    while ( cur_header != hr.end ) {
        size_t full_size = get_size( *cur_header );

        if ( full_size == 0 ) {
            print_bad_jump( ( bad_jump ){ cur_header, prev }, table, nil );
            printf( "Last known pointer before jump: %p", prev );
            return;
        }

        if ( is_block_allocated( *cur_header ) ) {
            print_alloc_block( cur_header );
        } else {
            print_free_block( cur_header );
        }
        prev = cur_header;
        cur_header = get_right_header( cur_header, full_size );
    }
    printf( "%p: END OF HEAP\n", hr.end );
    printf( COLOR_RED );
    printf( "<-%p:SENTINEL->\n", nil );
    printf( COLOR_NIL );
    printf( "%p: LAST ADDRESS\n", (byte *)nil + FREE_NODE_WIDTH );
    printf( "\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "\nSEGREGATED LIST OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    // For large heaps we wouldn't be able to scroll to the table location so
    // print again here.
    print_fits( VERBOSE, table, nil );
}
