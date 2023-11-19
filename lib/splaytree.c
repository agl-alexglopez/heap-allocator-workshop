#include "allocator.h"
#include "debug_break.h"
#include "print_utility.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef size_t header;

struct node
{
    header header;
    struct node *links[2];
    struct duplicate_node *list_start;
};

struct duplicate_node
{
    header header;
    struct duplicate_node *links[2];
    struct node *parent;
};

enum tree_link
{
    L = 0,
    R = 1
};

enum list_link
{
    N = 0,
    P = 1
};

#define SIZE_MASK ~0x7UL
#define BLOCK_SIZE 40UL
#define HEADERSIZE sizeof( size_t )
#define FREED 0x0UL
#define ALLOCATED 0x1UL
#define LEFT_ALLOCATED 0x2UL
#define HEAP_NODE_WIDTH 32UL
#define MAX_TREE_HEIGHT 64UL
#define LEFT_FREE ~0x2UL

// NOLINTBEGIN(*-non-const-global-variables)

/// Implemented as a splay tree of free nodes.
static struct free_nodes
{
    struct node *root;
    // These two pointers will point to the same address. Used for clarity between tree and list.
    struct node *nil;
    struct duplicate_node *list_tail;
    size_t total;
} free_nodes;

static struct heap
{
    void *client_start;
    void *client_end;
    size_t heap_size;
} heap;

// NOLINTEND(*-non-const-global-variables)

static size_t roundup( size_t requested_size, size_t multiple );
static size_t get_size( header header_val );
static struct node *get_min( struct node *root, struct node *nil, struct node *path[], int *path_len );
static bool is_block_allocated( header block_header );
static bool is_left_space( const struct node *node );
static void init_header_size( struct node *node, size_t payload );
static void init_footer( struct node *node, size_t payload );
static struct node *get_right_neighbor( const struct node *current, size_t payload );
static struct node *get_left_neighbor( const struct node *node );
static void *get_client_space( const struct node *node_header );
static struct node *get_node( const void *client_space );
static void init_free_node( struct node *to_free, size_t block_size );
static void *split_alloc( struct node *free_block, size_t request, size_t block_space );
static struct node *coalesce( struct node *leftmost_node );
static void remove_head( struct node *head, struct node *lft_child, struct node *rgt_child );
static void *free_coalesced_node( void *to_coalesce );
static struct node *find_best_fit( size_t key );
static void insert_node( struct node *current );
static void splay( struct node *x, struct node *path[], int path_len );
static void rotate( enum tree_link rotation, struct node *current, struct node *path[], int path_len );

///////////////////////////////   Shared Heap Functions   ////////////////////////////////

size_t get_free_total( void ) { return free_nodes.total; }

bool myinit( void *heap_start, size_t heap_size )
{
    size_t client_request = roundup( heap_size, ALIGNMENT );
    if ( client_request < BLOCK_SIZE ) {
        return false;
    }
    heap.client_start = heap_start;
    heap.heap_size = client_request;
    heap.client_end = (uint8_t *)heap.client_start + heap.heap_size - HEAP_NODE_WIDTH;

    // This helps us clarify if we are referring to tree or duplicates in a list. Use same address.
    free_nodes.list_tail = heap.client_end;
    free_nodes.nil = heap.client_end;
    free_nodes.nil->header = 1UL;
    free_nodes.root = heap.client_start;
    init_header_size( free_nodes.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    init_footer( free_nodes.root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    free_nodes.root->links[L] = free_nodes.nil;
    free_nodes.root->links[R] = free_nodes.nil;
    free_nodes.root->list_start = free_nodes.list_tail;
    free_nodes.total = 1;
    return true;
}

void *mymalloc( size_t requested_size )
{
    (void)requested_size;
    UNIMPLEMENTED();
}

void *myrealloc( void *old_ptr, size_t new_size )
{
    (void)old_ptr;
    (void)new_size;
    UNIMPLEMENTED();
}

void myfree( void *ptr )
{
    (void)ptr;
    UNIMPLEMENTED();
}

bool validate_heap( void ) { UNIMPLEMENTED(); }

void print_free_nodes( enum print_style style )
{
    (void)style;
    UNIMPLEMENTED();
}

/////////////////////    Static Heap Helper Functions    //////////////////////////////////

/// @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
/// @param *to_free        the heap_node to add to the red black tree.
/// @param block_size      the size we use to initialize the node and find the right place in tree.
static void init_free_node( struct node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED;
    to_free->list_start = free_nodes.list_tail;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_node( to_free );
}

/// @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
///                      split, it will add the newly freed split block to the free red black tree.
/// @param *free_block   a pointer to the node for a free block in its entirety.
/// @param request       the user request for space.
/// @param block_space   the entire space that we have to work with.
/// @return              a void pointer to generic space that is now ready for the client.
static void *split_alloc( struct node *free_block, size_t request, size_t block_space )
{
    if ( block_space >= request + BLOCK_SIZE ) {
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_free_node( get_right_neighbor( free_block, request ), block_space - request - HEADERSIZE );
        init_header_size( free_block, request );
        free_block->header |= ALLOCATED;
        return get_client_space( free_block );
    }
    get_right_neighbor( free_block, block_space )->header |= LEFT_ALLOCATED;
    init_header_size( free_block, block_space );
    free_block->header |= ALLOCATED;
    return get_client_space( free_block );
}

/// @brief coalesce        attempts to coalesce left and right if the left and right node
///                        are free. Runs the search to free the specific free node in O(logN) + d
///                        where d is the number of duplicate nodes of the same size.
/// @param *leftmost_node  the current node that will move left if left is free to coalesce.
/// @return                the leftmost node from attempts to coalesce left and right. The leftmost
///                        node is initialized to reflect the correct size for the space it now has.
/// @warning               this function does not overwrite the data that may be in the middle if we
///                        expand left and write. The user may wish to move elsewhere if reallocing.
static struct node *coalesce( struct node *leftmost_node )
{
    size_t coalesced_space = get_size( leftmost_node->header );
    struct node *rightmost_node = get_right_neighbor( leftmost_node, coalesced_space );

    // The black_nil is the right boundary. We set it to always be allocated with size 0.
    if ( !is_block_allocated( rightmost_node->header ) ) {
        coalesced_space += get_size( rightmost_node->header ) + HEADERSIZE;
        (void)free_coalesced_node( rightmost_node );
    }
    // We use our static struct for convenience here to tell where our segment start is.
    if ( leftmost_node != heap.client_start && is_left_space( leftmost_node ) ) {
        leftmost_node = get_left_neighbor( leftmost_node );
        coalesced_space += get_size( leftmost_node->header ) + HEADERSIZE;
        leftmost_node = free_coalesced_node( leftmost_node );
    }

    // We do not initialize a footer here because we don't want to overwrite user data.
    init_header_size( leftmost_node, coalesced_space );
    return leftmost_node;
}

/// @brief remove_head  frees the head of a doubly linked list of duplicates which is a node in a
///                     red black tree. The next duplicate must become the new tree node and the
///                     parent and children must be adjusted to track this new node.
/// @param head         the node in the tree that must now be coalesced.
/// @param lft_child    if the left child has a duplicate, it tracks the new tree node as parent.
/// @param rgt_child    if the right child has a duplicate, it tracks the new tree node as parent.
static void remove_head( struct node *head, struct node *lft_child, struct node *rgt_child )
{
    // Store the parent in an otherwise unused field for a major O(1) coalescing speed boost.
    struct node *tree_parent = head->list_start->parent;
    head->list_start->header = head->header;
    head->list_start->links[N]->parent = head->list_start->parent;

    struct node *new_tree_node = (struct node *)head->list_start;
    new_tree_node->list_start = head->list_start->links[N];
    new_tree_node->links[L] = lft_child;
    new_tree_node->links[R] = rgt_child;

    // We often write to fields of the black_nil, invariant. DO NOT use the nodes it stores!
    if ( lft_child != free_nodes.nil ) {
        lft_child->list_start->parent = new_tree_node;
    }
    if ( rgt_child != free_nodes.nil ) {
        rgt_child->list_start->parent = new_tree_node;
    }
    if ( tree_parent == free_nodes.nil ) {
        free_nodes.root = new_tree_node;
    } else {
        tree_parent->links[tree_parent->links[R] == head] = new_tree_node;
    }
}

/// @brief free_coalesced_node  a specialized version of node freeing when we find a neighbor we
///                             need to free from the tree before absorbing into our coalescing. If
///                             this node is a duplicate we can splice it from a linked list.
/// @param *to_coalesce         the address for a node we must treat as a list or tree node.
/// @return                     the node we have now correctly freed given all cases to find it.
static void *free_coalesced_node( void *to_coalesce )
{
    struct node *tree_node = to_coalesce;
    // Go find and fix the node the normal way if it is unique.
    if ( tree_node->list_start == free_nodes.list_tail ) {
        return find_best_fit( get_size( tree_node->header ) );
    }
    struct duplicate_node *list_node = to_coalesce;
    struct node *lft_tree_node = tree_node->links[L];

    // Coalescing the first node in linked list. Dummy head, aka lft_tree_node, is to the left.
    if ( lft_tree_node != free_nodes.nil && lft_tree_node->list_start == to_coalesce ) {
        list_node->links[N]->parent = list_node->parent;
        lft_tree_node->list_start = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

        // All nodes besides the tree node head and the first duplicate node have parent set to NULL.
    } else if ( NULL == list_node->parent ) {
        list_node->links[P]->links[N] = list_node->links[N];
        list_node->links[N]->links[P] = list_node->links[P];

        // Coalesce head of a doubly linked list in the tree. Remove and make a new head.
    } else {
        remove_head( tree_node, lft_tree_node, tree_node->links[R] );
    }
    free_nodes.total--;
    return to_coalesce;
}

/////////////////////////////      Splay Tree Implementation       //////////////////////////////////

/// @brief find_best_fit  a splay tree is well suited to best fit search in O(logN) time. We
///                       will find the best fitting node possible given the options in our tree.
/// @param key            the size_t number of bytes we are searching for in our tree.
/// @return               the pointer to the struct node that is the best fit for our need.
static struct node *find_best_fit( size_t key )
{
    (void)key;
    UNIMPLEMENTED();
}

/// @brief insert_node  a modified insertion with additional logic to add duplicates if the
///                     size in bytes of the block is already in the tree.
/// @param *current     we must insert to tree or add to a list as duplicate.
static void insert_node( struct node *current )
{
    (void)current;
    UNIMPLEMENTED();
}

static void splay( struct node *cur, struct node *path[], int path_len )
{
    while ( path_len >= 3 && path[path_len - 2] != free_nodes.nil ) {
        struct node *gparent = path[path_len - 3];
        struct node *parent = path[path_len - 2];
        if ( gparent != free_nodes.nil ) {
            rotate( parent->links[R] == cur, parent, path, path_len - 1 );
            --path_len;
            continue;
        }
        if ( cur == parent->links[L] && parent == gparent->links[L] ) {
            rotate( R, gparent, path, path_len - 2 );
            --path_len;
            rotate( R, parent, path, path_len - 1 );
            --path_len;
            continue;
        }
        if ( cur == parent->links[R] && parent == gparent->links[R] ) {
            rotate( L, gparent, path, path_len - 2 );
            --path_len;
            rotate( L, parent, path, path_len - 1 );
            --path_len;
            continue;
        }
        if ( cur == parent->links[R] && parent == gparent->links[L] ) {
            rotate( L, parent, path, path_len - 1 );
            --path_len;
            rotate( R, parent, path, path_len - 1 );
            --path_len;
            continue;
        }
        rotate( R, parent, path, path_len - 1 );
        --path_len;
        rotate( L, parent, path, path_len - 1 );
        --path_len;
    }
}

/// @brief rotate    a unified version of the traditional left and right rotation functions. The
///                  rotation is either left or right and opposite is its opposite direction. We
///                  take the current nodes child, and swap them and their arbitrary subtrees are
///                  re-linked correctly depending on the direction of the rotation.
/// @param *current  the node around which we will rotate.
/// @param rotation  either left or right. Determines the rotation and its opposite direction.
/// @warning         this function modifies the stack.
static void rotate( enum tree_link rotation, struct node *current, struct node *path[], int path_len )
{
    if ( path_len < 2 ) {
        printf( "Path length is %d but request for path len %d - 2 was made.", path_len, path_len );
        abort();
    }
    struct node *parent = path[path_len - 2];
    struct node *child = current->links[!rotation];
    current->links[!rotation] = child->links[rotation];
    if ( child->links[rotation] != free_nodes.nil ) {
        child->links[rotation]->list_start->parent = current;
    }
    if ( child != free_nodes.nil ) {
        child->list_start->parent = parent;
    }
    if ( parent == free_nodes.nil ) {
        free_nodes.root = child;
    } else {
        parent->links[parent->links[R] == current] = child;
    }
    child->links[rotation] = current;
    current->list_start->parent = child;
    // Make sure to adjust lineage due to rotation for the path.
    path[path_len - 1] = child;
    path[path_len] = current;
}

/////////////////////////////   Basic Block and Header Operations  //////////////////////////////////

/// @brief roundup         rounds up size to the nearest multiple of two to be aligned in the heap.
/// @param requested_size  size given to us by the client.
/// @param multiple        the nearest multiple to raise our number to.
/// @return                rounded number.
static inline size_t roundup( size_t requested_size, size_t multiple )
{
    return ( requested_size + multiple - 1 ) & ~( multiple - 1 );
}

/// @brief get_size    returns size in bytes as a size_t from the value of node's header.
/// @param header_val  the value of the node in question passed by value.
/// @return            the size in bytes as a size_t of the node.
static inline size_t get_size( header header_val ) { return SIZE_MASK & header_val; }

/// @brief get_min     returns the smallest node in a valid binary search tree.
/// @param *root       the root of any valid binary search tree.
/// @param *black_nil  the sentinel node sitting at the bottom of the tree. It is always black.
/// @param *path[]     the stack we are using to track tree lineage and rotations.
/// @param *path_len   the length of the path we update as we get the tree minimum.
/// @return            a pointer to the minimum node in a valid binary search tree.
static inline struct node *get_min( struct node *root, struct node *nil, struct node *path[], int *path_len )
{
    for ( ; root->links[L] != nil; root = root->links[L] ) {
        path[( *path_len )++] = root;
    }
    path[( *path_len )++] = root;
    return root;
}

/// @brief is_block_allocated  determines if a node is allocated or free.
/// @param block_header        the header value of a node passed by value.
/// @return                    true if allocated false if not.
static inline bool is_block_allocated( const header block_header ) { return block_header & ALLOCATED; }

/// @brief is_left_space  determines if the left neighbor of a block is free or allocated.
/// @param *node          the node to check.
/// @return               true if left is free false if left is allocated.
static inline bool is_left_space( const struct node *node ) { return !( node->header & LEFT_ALLOCATED ); }

/// @brief init_header_size  initializes any node as the size and indicating left is allocated.
///                          Left is allocated because we always coalesce left and right.
/// @param *node             the region of possibly uninitialized heap we must initialize.
/// @param payload           the payload in bytes as a size_t of the current block we initialize
static inline void init_header_size( struct node *node, size_t payload )
{
    node->header = LEFT_ALLOCATED | payload;
}

/// @brief init_footer  initializes footer at end of the heap block to matcht the current header.
/// @param *node        the current node with a header field we will use to set the footer.
/// @param payload      the size of the current nodes free memory.
static inline void init_footer( struct node *node, size_t payload )
{
    header *footer = (header *)( (uint8_t *)node + payload );
    *footer = node->header;
}

/// @brief get_right_neighbor  gets the address of the next struct node in the heap to the right.
/// @param *current            the struct node we start at to then jump to the right.
/// @param payload             the size in bytes as a size_t of the current struct node block.
/// @return                    the struct node to the right of the current.
static inline struct node *get_right_neighbor( const struct node *current, size_t payload )
{
    return (struct node *)( (uint8_t *)current + HEADERSIZE + payload );
}

/// @brief *get_left_neighbor  uses the left block size gained from the footer to move to the header.
/// @param *node               the current header at which we reside.
/// @return                    a header pointer to the header for the block to the left.
static inline struct node *get_left_neighbor( const struct node *node )
{
    header *left_footer = (header *)( (uint8_t *)node - HEADERSIZE );
    return (struct node *)( (uint8_t *)node - ( *left_footer & SIZE_MASK ) - HEADERSIZE );
}

/// @brief get_client_space  steps into the client space just after the header of a node.
/// @param *node_header      the struct node we start at before retreiving the client space.
/// @return                  the void address of the client space they are now free to use.
static inline void *get_client_space( const struct node *node_header )
{
    return (uint8_t *)node_header + HEADERSIZE;
}

/// @brief get_struct node    steps to the struct node header from the space the client was using.
/// @param *client_space  the void the client was using for their type. We step to the left.
/// @return               a pointer to the struct node of our heap block.
static inline struct node *get_node( const void *client_space )
{
    return (struct node *)( (uint8_t *)client_space - HEADERSIZE );
}
