///
/// File list_segregated_design.c
/// ---------------------------------
/// This file contains the implementation of utility functions for the
/// list_segregated heap allocator. These functions serve as basic navigation for
/// nodes and blocks. They are inlined in the header file so must be declared
/// here. Hopefully the compiler inlines them in "hot-spot" functions.
///
#include "list_segregated_utilities.h"

extern size_t roundup( size_t requested_size, size_t multiple );

extern size_t get_size( header header_val );

extern header *get_right_header( header *cur_header, size_t block_size );

extern header *get_left_header( header *cur_header );

extern bool is_block_allocated( header header_val );

extern free_node *get_free_node( header *cur_header );

extern header *get_block_header( free_node *user_mem_space );

extern void init_header( header *cur_header, size_t block_size, header_status_t header_status );

extern void init_footer( header *cur_header, size_t block_size );

extern bool is_left_space( header *cur_header );
