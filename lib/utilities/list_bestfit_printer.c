/// File: list_bestfit_printer.c
/// ----------------------------
/// This file contains my implementation of the list printing functions. These
/// are helpful to view the status of the heap and pair well with gdb. One of
/// these functions is used to show the free nodes in the print_peaks.c program.
#include "list_bestfit_utilities.h"
#include <limits.h>
#include <stdio.h>

/////////////////////////////        Printing Functions            //////////////////////////////////

/// @brief print_alloc_block  prints the contents of an allocated block of memory.
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
    /* We should be able to see the header is the same as the footer. If they are
     * not the same we will face subtle bugs that are very hard to notice.
     */
    if ( *footer != *cur_header ) {
        *footer = ULONG_MAX;
    }
    printf( COLOR_RED );
    printf( "%p: HEADER->0x%016zX->[FREE-%zubytes->FOOTER->%016zX]\n", cur_header, *cur_header, block_size,
            *footer );
    printf( COLOR_NIL );
}

/// @brief print_bad_jump  If we overwrite data in a header, this print statement will help us
///                        notice where we went wrong and what the addresses were.
/// @param *current        the current node that is likely garbage values that don't make sense.
/// @param *prev           the previous node that we jumped from.
/// @param *head           the free_node head of the linked list.
/// @param *tail           the free_node tail of the linked list.
static void print_bad_jump( header *current, header *prev, free_node *head, free_node *tail )
{
    size_t prev_size = get_size( *prev );
    size_t cur_size = get_size( *current );
    printf( COLOR_CYN );
    printf( "A bad jump from the value of a header has occured. Bad distance to "
            "next header.\n" );
    printf( "The previous address: %p:\n", prev );
    printf( "\tHeader Hex Value: %016zX:\n", *prev );
    printf( "\tBlock Byte Value: %zubytes:\n", prev_size );
    printf( "\nJump by %zubytes...\n", prev_size );
    printf( "The current address: %p:\n", current );
    printf( "\tHeader Hex Value: %016zX:\n", *current );
    printf( "\tBlock Byte Value: %zubytes:\n", cur_size );
    printf( "\nJump by %zubytes...\n", cur_size );
    printf( "Current state of the free list:\n" );
    printf( COLOR_NIL );
    // The doubly linked free_list may be messed up as well.
    print_linked_free( VERBOSE, head, tail );
}

/// @brief print_linked_free  prints the doubly linked free list in order to check if splicing
///                           and adding is progressing correctly.
/// @param *head              the free_node head of the linked list.
/// @param *tail              the free_node tail of the linked list.
void print_linked_free( print_style style, free_node *head, free_node *tail )
{
    printf( COLOR_RED );
    printf( "[" );
    if ( style == VERBOSE ) {
        printf( "%p:", head );
    }
    printf( "(HEAD)]" );
    for ( free_node *cur = head->next; cur != tail; cur = cur->next ) {
        if ( cur ) {
            header *cur_header = get_block_header( cur );
            printf( "<=>[" );
            if ( style == VERBOSE ) {
                printf( "%p:", cur );
            }
            printf( "(%zubytes)]", get_size( *cur_header ) - HEADERSIZE );
        } else {
            printf( "Something went wrong. NULL free free_list node.\n" );
            break;
        }
    }
    printf( "<=>[" );
    if ( style == VERBOSE ) {
        printf( "%p:", tail );
    }
    printf( "(TAIL)]\n" );
    printf( COLOR_NIL );
}

/// @brief print_all    prints our the complete status of the heap, all of its blocks,
///                     and the sizes the blocks occupy. Printing should be clean with no
///                     overlap of unique id's between heap blocks or corrupted headers.
/// @param client_start the starting address of the heap segment.
/// @param client_end   the final address of the heap segment.
/// @param client_size  the size in bytes of the heap.
/// @param *head        the first node of the doubly linked list of nodes.
/// @param *tail        the last node of the doubly linked list of nodes.
void print_all( void *client_start, void *client_end, size_t client_size, free_node *head, free_node *tail )
{
    header *cur_header = client_start;
    printf( "Heap client segment starts at address %p, ends %p. %zu total bytes "
            "currently used.\n",
            cur_header, client_end, client_size );
    printf( "A-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n\n" );
    printf( "%p: FIRST ADDRESS\n", head );
    printf( "%p: NULL<-DUMMY HEAD NODE->%p\n", head, head->next );
    printf( "%p: START OF HEAP. HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n", client_start );
    header *prev = cur_header;
    while ( cur_header != client_end ) {
        size_t full_size = get_size( *cur_header );

        if ( full_size == 0 ) {
            print_bad_jump( cur_header, prev, head, tail );
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
    printf( "%p: END OF HEAP\n", client_end );
    printf( "%p: %p<-DUMMY TAIL NODE->NULL\n", tail, tail->prev );
    printf( "%p: LAST ADDRESS\n", (byte *)tail + FREE_NODE_WIDTH );
    printf( "\nA-BLOCK = ALLOCATED BLOCK, F-BLOCK = FREE BLOCK\n" );
    printf( "\nDOUBLY LINKED LIST OF FREE NODES AND BLOCK SIZES.\n" );
    printf( "HEADERS ARE NOT INCLUDED IN BLOCK BYTES:\n" );
    print_linked_free( VERBOSE, head, tail );
}
