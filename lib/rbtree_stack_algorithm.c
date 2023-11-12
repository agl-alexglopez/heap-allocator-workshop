/// Author: Alexander Griffin Lopez
/// File: rbtree_stack.c
/// ---------------------
///  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
///  implementation to track the free space in the heap, relying on the properties of a red-black
///  tree to remain balanced. This implementation also uses some interesting strategies to unify
///  left and right cases for a red black tree and maintains a doubly linked list of duplicate nodes
///  of the same size if a node in the tree has repeats of its size. We do not use a parent field,
///  instead opting to form a stack of nodes and pass that stack to the necessary functions.
///  Citations:
///  -------------------
///  1. Bryant and O'Hallaron, Computer Systems: A Programmer's Perspective, Chapter 9.
///     I used the explicit free list outline from the textbook, specifically
///     regarding how to implement left and right coalescing. I even used their suggested
///     optimization of an extra control bit so that the footers to the left can be overwritten
///     if the block is allocated so the user can have more space.
///  2. The text Introduction to Algorithms, by Cormen, Leiserson, Rivest, and Stein was central to
///     my implementation of the red black tree portion of my allocator. Specifically, I took the
///     the implementation from chapter 13. The placeholder black null node that always sits at the
///     bottom of the tree proved useful for the tree and the linked list of duplicate memory block
///     sizes.
#include "allocator.h"
#include "rbtree_stack_utilities.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

///  Static Heap Tracking

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
///  - Every path from root to free.black_nil, root not included, has same number of black nodes.
///  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
///  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
///  - Use a list_start pointer to a doubly linked list of duplicate nodes of the same size.
///  - For more details on the types see the _utilities.h file.
static struct free_nodes
{
    rb_node *tree_root;
    // These two pointers will point to the same address. Used for clarity between tree and list.
    rb_node *black_nil;
    duplicate_node *list_tail;
    size_t total;
} free_nodes;

static struct heap
{
    void *client_start;
    void *client_end;
    size_t heap_size;
} heap;

///   Static Helper Functions

/// @brief rotate    a unified version of the traditional left and right rotation functions. The
///                  rotation is either left or right and opposite is its opposite direction. We
///                  take the current nodes child, and swap them and their arbitrary subtrees are
///                  re-linked correctly depending on the direction of the rotation.
/// @param *current  the node around which we will rotate.
/// @param rotation  either left or right. Determines the rotation and its opposite direction.
/// @warning         this function modifies the stack.
static void rotate( tree_link rotation, rb_node *current, rb_node *path[], int path_len )
{
    rb_node *child = current->links[!rotation];
    current->links[!rotation] = child->links[rotation];

    if ( child->links[rotation] != free_nodes.black_nil ) {
        child->links[rotation]->list_start->parent = current;
    }

    rb_node *parent = path[path_len - 2];
    if ( child != free_nodes.black_nil ) {
        child->list_start->parent = parent;
    }

    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = child;
    } else {
        parent->links[parent->links[R] == current] = child;
    }
    child->links[rotation] = current;
    current->list_start->parent = child;
    // Make sure to adjust lineage due to rotation for the path.
    path[path_len - 1] = child;
    path[path_len] = current;
}

///     Static Red-Black Tree Insertion Helper Function

/// @brief add_duplicate  this implementation stores duplicate nodes in a linked list to prevent the
///                       rotation of duplicates in the tree. This adds the duplicate node to the
///                       linked list of the node already present.
/// @param *head          the node currently organized in the tree. We will add to its list.
/// @param *to_add        the node to add to the linked list.
static void add_duplicate( rb_node *head, duplicate_node *add, rb_node *parent )
{
    add->header = head->header;
    // The first node in the list can store the tree nodes parent for faster coalescing later.
    if ( head->list_start == free_nodes.list_tail ) {
        add->parent = parent;
    } else {
        add->parent = head->list_start->parent;
        head->list_start->parent = NULL;
    }
    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[P] = add;
    add->links[N] = head->list_start;
    head->list_start = add;
    add->links[P] = (duplicate_node *)head;
    free_nodes.total++;
}

///      Static Red-Black Tree Insertion Logic

/// @brief fix_rb_insert  implements a modified Cormen et.al. red black fixup after the insertion of
///                       a new node. Unifies the symmetric left and right cases with the use of
///                       an array and an enum tree_link.
/// @param *path[]        the path representing the route down to the node we have inserted.
/// @param path_len       the length of the path.
static void fix_rb_insert( rb_node *path[], int path_len )
{
    // We always place the black_nil at 0th index in the stack as root's parent so this is safe.
    while ( get_color( path[path_len - 2]->header ) == RED ) {
        rb_node *current = path[path_len - 1];
        rb_node *parent = path[path_len - 2];
        rb_node *grandparent = path[path_len - 3];

        // We can store the case we need to complete and its opposite rather than repeat code.
        tree_link symmetric_case = grandparent->links[R] == parent;
        rb_node *aunt = grandparent->links[!symmetric_case];
        if ( get_color( aunt->header ) == RED ) {
            paint_node( aunt, BLACK );
            paint_node( parent, BLACK );
            paint_node( grandparent, RED );
            current = grandparent;
            path_len -= 2;
        } else {
            if ( current == parent->links[!symmetric_case] ) {
                current = parent;
                rb_node *other_child = current->links[!symmetric_case];
                rotate( symmetric_case, current, path, path_len - 1 );
                parent = other_child;
            }
            paint_node( parent, BLACK );
            paint_node( grandparent, RED );
            rotate( !symmetric_case, grandparent, path, path_len - 2 );
            path_len--;
        }
    }
    paint_node( free_nodes.tree_root, BLACK );
}

/// @brief insert_rb_node  a modified insertion with additional logic to add duplicates if the
///                        size in bytes of the block is already in the tree.
/// @param *current        we must insert to tree or add to a list as duplicate.
static void insert_rb_node( rb_node *current )
{
    size_t current_key = get_size( current->header );

    rb_node *path[MAX_TREE_HEIGHT];
    // This simplifies our insertion fixup logic later. See loop in the fixup function.
    path[0] = free_nodes.black_nil;
    int path_len = 1;
    rb_node *seeker = free_nodes.tree_root;
    size_t parent_size = 0;
    while ( seeker != free_nodes.black_nil ) {
        path[path_len++] = seeker;

        parent_size = get_size( seeker->header );
        // Ability to add duplicates to linked list means no fixups necessary if duplicate.
        if ( current_key == parent_size ) {
            add_duplicate( seeker, (duplicate_node *)current, path[path_len - 2] );
            return;
        }
        // You may see this idiom throughout. L(0) if key fits in tree to left, R(1) if not.
        seeker = seeker->links[parent_size < current_key];
    }
    rb_node *parent = path[path_len - 1];
    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = current;
    } else {
        parent->links[parent_size < current_key] = current;
    }
    current->links[L] = free_nodes.black_nil;
    current->links[R] = free_nodes.black_nil;
    // Store the doubly linked duplicates in list. list_tail aka black_nil is the dummy tail.
    current->list_start = free_nodes.list_tail;
    paint_node( current, RED );
    path[path_len++] = current;
    fix_rb_insert( path, path_len );
    free_nodes.total++;
}

///     Static Red-Black Tree Deletion Helper Functions

/// @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
/// @param *replacement   the node that will fill the deleted position. It can be black_nil.
/// @warning              this function modifies the stack.
static void rb_transplant( rb_node *replacement, rb_node *path[], int path_len )
{
    rb_node *parent = path[path_len - 2];
    rb_node *remove = path[path_len - 1];
    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = replacement;
    } else {
        parent->links[parent->links[R] == remove] = replacement;
    }
    if ( replacement != free_nodes.black_nil ) {
        replacement->list_start->parent = parent;
    }
    // Delete the removed node from the path to give back correct path to fixup function.
    path[path_len - 1] = replacement;
}

/// @brief delete_duplicate  will remove a duplicate node from the tree when the request is coming
///                          from a call from malloc. Address of duplicate does not matter so we
///                          remove the first node from the linked list.
/// @param *head             We know this node has a next node and it must be removed for malloc.
static rb_node *delete_duplicate( rb_node *head )
{
    duplicate_node *next_node = head->list_start;
    /// Take care of the possible node to the right in the doubly linked list first. This could be
    /// another node or it could be free_nodes.list_tail, it does not matter either way.
    next_node->links[N]->parent = next_node->parent;
    next_node->links[N]->links[P] = (duplicate_node *)head;
    head->list_start = next_node->links[N];
    free_nodes.total--;
    return (rb_node *)next_node;
}

///      Static Red-Black Tree Deletion Logic

/// @brief fix_rb_delete  completes a unified Cormen et.al. fixup function. Uses a direction enum
///                       and an array to help unify code paths based on direction and opposites.
/// @param *extra_black   the current node that was moved into place from the previous delete. It
///                       may have broken rules of the tree or thrown off balance.
/// @param *path[]        the path representing the route down to the node we have inserted.
/// @param path_len       the length of the path.
static void fix_rb_delete( rb_node *extra_black, rb_node *path[], int path_len )
{
    // The extra_black is "doubly black" if we enter the loop, requiring repairs.
    while ( extra_black != free_nodes.tree_root && get_color( extra_black->header ) == BLACK ) {
        rb_node *parent = path[path_len - 2];

        // We can cover left and right cases in one with simple directional link and its opposite.
        tree_link symmetric_case = parent->links[R] == extra_black;

        rb_node *sibling = parent->links[!symmetric_case];
        if ( get_color( sibling->header ) == RED ) {
            paint_node( sibling, BLACK );
            paint_node( parent, RED );
            rotate( symmetric_case, parent, path, path_len - 1 );
            // Rotating a parent the same direction as the extra_black, moves it down the path.
            path[path_len++] = extra_black;
            sibling = parent->links[!symmetric_case];
        }
        if ( get_color( sibling->links[L]->header ) == BLACK && get_color( sibling->links[R]->header ) == BLACK ) {
            paint_node( sibling, RED );
            extra_black = path[path_len - 2];
            path_len--;
        } else {
            if ( get_color( sibling->links[!symmetric_case]->header ) == BLACK ) {
                paint_node( sibling->links[symmetric_case], BLACK );
                paint_node( sibling, RED );
                rotate( !symmetric_case, sibling, path, path_len );
                sibling = parent->links[!symmetric_case];
            }
            paint_node( sibling, get_color( parent->header ) );
            paint_node( parent, BLACK );
            paint_node( sibling->links[!symmetric_case], BLACK );
            rotate( symmetric_case, parent, path, path_len - 1 );
            extra_black = free_nodes.tree_root;
        }
    }
    // The extra_black has reached a red node, making it "red-and-black", or the root. Paint BLACK.
    paint_node( extra_black, BLACK );
}

/// @brief delete_rb_node  performs the necessary steps to have a functional, balanced tree after
///                        deletion of any node in the free.
/// @param *remove         the node to remove from the tree from a call to malloc or coalesce.
/// @param *path[]         the path representing the route down to the node we have inserted.
/// @param path_len        the length of the path.
static rb_node *delete_rb_node( rb_node *remove, rb_node *path[], int path_len )
{
    rb_color fixup_color_check = get_color( remove->header );

    rb_node *parent = path[path_len - 2];
    rb_node *extra_black = NULL;
    if ( remove->links[L] == free_nodes.black_nil || remove->links[R] == free_nodes.black_nil ) {
        tree_link nil_link = remove->links[L] != free_nodes.black_nil;
        extra_black = remove->links[!nil_link];
        rb_transplant( extra_black, path, path_len );
    } else {
        int len_removed_node = path_len;
        // Warning, path_len may have changed.
        rb_node *right_min = get_min( remove->links[R], free_nodes.black_nil, path, &path_len );
        fixup_color_check = get_color( right_min->header );

        extra_black = right_min->links[R];
        if ( right_min != remove->links[R] ) {
            rb_transplant( extra_black, path, path_len );
            right_min->links[R] = remove->links[R];
            right_min->links[R]->list_start->parent = right_min;
        } else {
            path[path_len - 1] = extra_black;
        }
        rb_transplant( right_min, path, len_removed_node );
        right_min->links[L] = remove->links[L];
        right_min->links[L]->list_start->parent = right_min;
        right_min->list_start->parent = parent;
        paint_node( right_min, get_color( remove->header ) );
    }
    // Nodes can only be red or black, so we need to get rid of "extra" black by fixing tree.
    if ( fixup_color_check == BLACK ) {
        fix_rb_delete( extra_black, path, path_len );
    }
    free_nodes.total--;
    return remove;
}

/// @brief find_best_fit  a red black tree is well suited to best fit search in O(logN) time. We
///                       will find the best fitting node possible given the options in our tree.
/// @param key            the size_t number of bytes we are searching for in our tree.
/// @return               the pointer to the rb_node that is the best fit for our need.
static rb_node *find_best_fit( size_t key )
{
    rb_node *path[MAX_TREE_HEIGHT];
    path[0] = free_nodes.black_nil;
    int path_len = 1;
    int len_to_best_fit = 1;

    rb_node *seeker = free_nodes.tree_root;
    // We will use this sentinel to start our competition while we search for best fit.
    size_t best_fit_size = ULLONG_MAX;
    rb_node *remove = seeker;
    while ( seeker != free_nodes.black_nil ) {
        size_t seeker_size = get_size( seeker->header );
        path[path_len++] = seeker;
        if ( key == seeker_size ) {
            remove = seeker;
            len_to_best_fit = path_len;
            break;
        }
        tree_link search_direction = seeker_size < key;
        /// The key is less than the current found size but let's remember this size on the way down
        /// as a candidate for the best fit. The closest fit will have won when we reach the bottom.
        if ( search_direction == L && seeker_size < best_fit_size ) {
            remove = seeker;
            best_fit_size = seeker_size;
            len_to_best_fit = path_len;
        }
        seeker = seeker->links[search_direction];
    }

    if ( remove->list_start != free_nodes.list_tail ) {
        // We will keep remove in the tree and just get the first node in doubly linked list.
        return delete_duplicate( remove );
    }
    return delete_rb_node( remove, path, len_to_best_fit );
}

/// @brief remove_head  frees the head of a doubly linked list of duplicates which is a node in a
///                     red black tree. The next duplicate must become the new tree node and the
///                     parent and children must be adjusted to track this new node.
/// @param head         the node in the tree that must now be coalesced.
/// @param lft_child    if the left child has a duplicate, it tracks the new tree node as parent.
/// @param rgt_child    if the right child has a duplicate, it tracks the new tree node as parent.
static void remove_head( rb_node *head, rb_node *lft_child, rb_node *rgt_child )
{
    // Store the parent in an otherwise unused field for a major O(1) coalescing speed boost.
    rb_node *tree_parent = head->list_start->parent;
    head->list_start->header = head->header;
    head->list_start->links[N]->parent = head->list_start->parent;

    rb_node *new_tree_node = (rb_node *)head->list_start;
    new_tree_node->list_start = head->list_start->links[N];
    new_tree_node->links[L] = lft_child;
    new_tree_node->links[R] = rgt_child;

    // We often write to fields of the black_nil, invariant. DO NOT use the nodes it stores!
    if ( lft_child != free_nodes.black_nil ) {
        lft_child->list_start->parent = new_tree_node;
    }
    if ( rgt_child != free_nodes.black_nil ) {
        rgt_child->list_start->parent = new_tree_node;
    }
    if ( tree_parent == free_nodes.black_nil ) {
        free_nodes.tree_root = new_tree_node;
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
    rb_node *tree_node = to_coalesce;
    // Go find and fix the node the normal way if it is unique.
    if ( tree_node->list_start == free_nodes.list_tail ) {
        return find_best_fit( get_size( tree_node->header ) );
    }
    duplicate_node *list_node = to_coalesce;
    rb_node *lft_tree_node = tree_node->links[L];

    // Coalescing the first node in linked list. Dummy head, aka lft_tree_node, is to the left.
    if ( lft_tree_node != free_nodes.black_nil && lft_tree_node->list_start == to_coalesce ) {
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

///    Static Heap Helper Functions

/// @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
/// @param *to_free        the heap_node to add to the red black tree.
/// @param block_size      the size we use to initialize the node and find the right place in tree.
static void init_free_node( rb_node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    to_free->list_start = free_nodes.list_tail;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_rb_node( to_free );
}

/// @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
///                      split, it will add the newly freed split block to the free red black tree.
/// @param *free_block   a pointer to the node for a free block in its entirety.
/// @param request       the user request for space.
/// @param block_space   the entire space that we have to work with.
/// @return              a void pointer to generic space that is now ready for the client.
static void *split_alloc( rb_node *free_block, size_t request, size_t block_space )
{
    rb_node *neighbor = NULL;
    if ( block_space >= request + MIN_BLOCK_SIZE ) {
        neighbor = get_right_neighbor( free_block, request );
        // This takes care of the neighbor and ITS neighbor with appropriate updates.
        init_free_node( neighbor, block_space - request - HEADERSIZE );
    } else {
        request = block_space;
        neighbor = get_right_neighbor( free_block, block_space );
        neighbor->header |= LEFT_ALLOCATED;
    }
    init_header_size( free_block, request );
    free_block->header |= ALLOCATED;
    return get_client_space( free_block );
}

/// @brief coalesce        attempts to coalesce left and right if the left and right rb_node
///                        are free. Runs the search to free the specific free node in O(logN) + d
///                        where d is the number of duplicate nodes of the same size.
/// @param *leftmost_node  the current node that will move left if left is free to coalesce.
/// @return                the leftmost node from attempts to coalesce left and right. The leftmost
///                        node is initialized to reflect the correct size for the space it now has.
/// @warning               this function does not overwrite the data that may be in the middle if we
///                        expand left and write. The user may wish to move elsewhere if reallocing.
static rb_node *coalesce( rb_node *leftmost_node )
{
    size_t coalesced_space = get_size( leftmost_node->header );
    rb_node *rightmost_node = get_right_neighbor( leftmost_node, coalesced_space );

    // The black_nil is the right boundary. We set it to always be allocated with size 0.
    if ( !is_block_allocated( rightmost_node->header ) ) {
        coalesced_space += get_size( rightmost_node->header ) + HEADERSIZE;
        rightmost_node = free_coalesced_node( rightmost_node );
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

///          Shared Heap Functions

/// @brief get_free_total  returns the total number of free nodes in the heap.
/// @return                a size_t representing the total quantity of free nodes.
size_t get_free_total() { return free_nodes.total; }

/// @brief myinit      initializes the heap space for use for the client.
/// @param *heap_start pointer to the space we will initialize as our heap.
/// @param heap_size   the desired space for the heap memory overall.
/// @return            true if the space if the space is successfully initialized false if not.
bool myinit( void *heap_start, size_t heap_size )
{
    // Initialize the root of the tree and heap addresses.
    size_t client_request = roundup( heap_size, ALIGNMENT );
    if ( client_request < MIN_BLOCK_SIZE ) {
        return false;
    }
    heap.client_start = heap_start;
    heap.heap_size = client_request;
    heap.client_end = (byte *)heap.client_start + heap.heap_size - HEAP_NODE_WIDTH;

    // This helps us clarify if we are referring to tree or duplicates in a list. Use same address.
    free_nodes.list_tail = heap.client_end;
    free_nodes.black_nil = heap.client_end;
    free_nodes.black_nil->header = 1UL;
    paint_node( free_nodes.black_nil, BLACK );

    free_nodes.tree_root = heap.client_start;
    init_header_size( free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    paint_node( free_nodes.tree_root, BLACK );
    init_footer( free_nodes.tree_root, heap.heap_size - HEAP_NODE_WIDTH - HEADERSIZE );
    free_nodes.tree_root->links[L] = free_nodes.black_nil;
    free_nodes.tree_root->links[R] = free_nodes.black_nil;
    free_nodes.tree_root->list_start = free_nodes.list_tail;
    free_nodes.total = 1;
    return true;
}

/// @brief *mymalloc       finds space for the client from the red black tree.
/// @param requested_size  the user desired size that we will round up and align.
/// @return                a void pointer to the space ready for the user or NULL if the request
///                        could not be serviced because it was invalid or there is no space.
void *mymalloc( size_t requested_size )
{
    if ( requested_size != 0 && requested_size <= MAX_REQUEST_SIZE ) {
        size_t client_request = roundup( requested_size + HEAP_NODE_WIDTH, ALIGNMENT );
        // Search the tree for the best possible fitting node.
        rb_node *found_node = find_best_fit( client_request );
        return split_alloc( found_node, client_request, get_size( found_node->header ) );
    }
    return NULL;
}

/// @brief *myrealloc  reallocates space for the client. It uses right coalescing in place
///                    reallocation. It will free memory on a zero request and a non-Null pointer.
///                    If reallocation fails, the memory does not move and we return NULL.
/// @param *old_ptr    the old memory the client wants resized.
/// @param new_size    the client's newly desired size. May be larger or smaller.
/// @return            new space if the pointer is null, NULL on invalid request or inability to
///                    find space.
void *myrealloc( void *old_ptr, size_t new_size )
{
    if ( new_size > MAX_REQUEST_SIZE ) {
        return NULL;
    }
    if ( old_ptr == NULL ) {
        return mymalloc( new_size );
    }
    if ( new_size == 0 ) {
        myfree( old_ptr );
        return NULL;
    }
    size_t request = roundup( new_size + HEAP_NODE_WIDTH, ALIGNMENT );
    rb_node *old_node = get_rb_node( old_ptr );
    size_t old_size = get_size( old_node->header );

    rb_node *leftmost_node = coalesce( old_node );
    size_t coalesced_space = get_size( leftmost_node->header );
    void *client_space = get_client_space( leftmost_node );

    if ( coalesced_space >= request ) {
        if ( leftmost_node != old_node ) {
            memmove( client_space, old_ptr, old_size );
        }
        client_space = split_alloc( leftmost_node, request, coalesced_space );
    } else if ( ( client_space = mymalloc( request ) ) ) {
        memcpy( client_space, old_ptr, old_size );
        init_free_node( leftmost_node, coalesced_space );
    }
    return client_space;
}

/// @brief myfree  frees valid user memory from the heap.
/// @param *ptr    a pointer to previously allocated heap memory.
void myfree( void *ptr )
{
    if ( ptr != NULL ) {
        rb_node *to_insert = get_rb_node( ptr );
        to_insert = coalesce( to_insert );
        init_free_node( to_insert, get_size( to_insert->header ) );
    }
}

///       Shared Debugging

/// @brief validate_heap  runs various checks to ensure that every block of the heap is well formed
///                       with valid sizes, alignment, and initializations.
/// @return               true if the heap is valid and false if the heap is invalid.
bool validate_heap()
{
    if ( !check_init( heap.client_start, heap.client_end, heap.heap_size ) ) {
        return false;
    }
    // Check that after checking all headers we end on size 0 tail and then end of address space.
    size_t total_free_mem = 0;
    if ( !is_memory_balanced( &total_free_mem, heap.client_start, heap.client_end, heap.heap_size,
                              free_nodes.total ) ) {
        return false;
    }
    // Does a tree search for all memory match the linear heap search for totals?
    if ( !is_rbtree_mem_valid( free_nodes.tree_root, free_nodes.black_nil, total_free_mem ) ) {
        return false;
    }
    // Two red nodes in a row are invalid for the table.
    if ( is_red_red( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    // Does every path from a node to the black sentinel contain the same number of black nodes.
    if ( !is_bheight_valid( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }

    // This comes from a more official write up on red black trees so I included it.
    if ( !is_bheight_valid_V2( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    if ( !is_binary_tree( free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    if ( !is_duplicate_storing_parent( free_nodes.black_nil, free_nodes.tree_root, free_nodes.black_nil ) ) {
        return false;
    }
    return true;
}

///         Shared Printer

/// @brief print_free_nodes  a shared function across allocators requesting a printout of internal
///                          data structure used for free nodes of the heap.
/// @param style             VERBOSE or PLAIN. Plain only includes byte size, while VERBOSE includes
///                          memory addresses and black heights of the tree.
void print_free_nodes( print_style style )
{
    printf( COLOR_CYN "(+X)" COLOR_NIL );
    printf( " Indicates duplicate nodes in the tree linked by a doubly-linked list.\n" );
    print_rb_tree( free_nodes.tree_root, free_nodes.black_nil, style );
}

/// @brief dump_heap  prints out the complete status of the heap, all of its blocks, and the sizes
///                   the blocks occupy. Printing should be clean with no overlap of unique id's
///                   between heap blocks or corrupted headers.
void dump_heap()
{
    print_all( heap.client_start, heap.client_end, heap.heap_size, free_nodes.tree_root, free_nodes.black_nil );
}
