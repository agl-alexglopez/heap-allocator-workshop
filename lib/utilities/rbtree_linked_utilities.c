/// File rbtree_linked_design.c
/// ---------------------------------
/// This file contains the implementation of utility functions for the
/// rbtree_linked heap allocator. These functions serve as basic navigation for
/// nodes and blocks. These functions can distract from the algorithm
/// implementations in the actual rbtree_linked.c file so we seperate them out
/// here.
#include "rbtree_linked_utilities.h"

extern size_t roundup( size_t requested_size, size_t multiple );

extern void paint_node( rb_node *node, rb_color color );

extern rb_color get_color( header header_val );

extern size_t get_size( header header_val );

extern rb_node *get_min( rb_node *root, rb_node *black_nil );

extern bool is_block_allocated( const header block_header );

extern bool is_left_space( const rb_node *node );

extern void init_header_size( rb_node *node, size_t payload );

extern void init_footer( rb_node *node, size_t payload );

extern rb_node *get_right_neighbor( const rb_node *current, size_t payload );

extern rb_node *get_left_neighbor( const rb_node *node );

extern void *get_client_space( const rb_node *node_header );

extern rb_node *get_rb_node( const void *client_space );
