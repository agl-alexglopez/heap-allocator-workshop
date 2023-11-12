/// File rbtree_clrs_design.c
/// ---------------------------------
/// This file contains the implementation of utility functions for the
/// rbtree_clrs heap allocator. These functions serve as basic navigation for
/// nodes and blocks. We defined them as inline in the header file so we declare
/// them here and hope the compiler inlines them in "hot" functions.
#include "rbtree_clrs_utilities.h"

/////////////////////////////   Basic Block and Header Operations  //////////////////////////////////

extern size_t roundup( const size_t requested_size, size_t multiple );

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
