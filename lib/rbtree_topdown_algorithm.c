/// Author: Alexander Griffin Lopez
/// File: rbtree_topdown.c
/// ---------------------
///  This file contains my implementation of an explicit heap allocator. This allocator uses a tree
///  implementation to track the free space in the heap, relying on the properties of a red-black
///  tree to remain balanced. This implementation also uses some interesting strategies to unify
///  left and right cases for a red black tree and maintains a doubly linked list of duplicate nodes
///  of the same size if a node in the tree has repeats of its size. We do not use a parent field,
///  instead opting to form a stack of nodes and pass that stack to the necessary functions. We also
///  use a topdown approach to insertion and deletion meaning we fix the tree on the way down, not
///  up.
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
///  3. I used Julienne Walker's implementation of top down insertion and deletion into a red black
///     tree to guide my topdown implementation. Specifically, I followed Walker's insert and delete
///     functions. However, I modified both to allow for duplicates in my tree because the heap
///     requires duplicate nodes to be stored. I used a doubly linked list to acheive this.
///          https://web.archive.org/web/20141129024312/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx
#include "allocator.h"
#include "rbtree_topdown_utilities.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

///             Static Heap Tracking

/// Red Black Free Tree:
///  - Maintain a red black tree of free nodes.
///  - Root is black
///  - No red node has a red child
///  - New insertions are red
///  - NULL is considered black. We use a black sentinel instead. Physically lives on the heap.
///  - Every path from root to free_nodes.black_nil, root not included, has same number of black nodes.
///  - The 3rd LSB of the header holds the color: 0 for black, 1 for red.
///  - The 1st LSB holds the allocated status and 2nd LSB holds left neighbor status for coalescing.
///  - Use a *list_start pointer to a doubly linked list of duplicate nodes of the same size.
///  - For more details on the types see the _utilities.h file.
static struct free_nodes
{
    rb_node *tree_root;
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

///    Static Red-Black Tree Helper Functions

/// @brief single_rotation  performs a single rotation for a given direction and recolors the nodes
///                         as appropriate, making the new root black and new leaf red.
/// @param *root            the root of the tree that will be rotated left or right, moving down.
/// @param rotation         the rotation direction for the operation.
/// @return                 the new root of the rotation, the lower node has moved up.
static rb_node *single_rotation( rb_node *root, rb_node *parent, tree_link rotation )
{
    rb_node *save = root->links[!rotation];
    root->links[!rotation] = save->links[rotation];
    if ( save->links[rotation] != free_nodes.black_nil ) {
        save->links[rotation]->list_start->parent = root;
    }
    if ( save != free_nodes.black_nil ) {
        save->list_start->parent = parent;
    }
    if ( root == free_nodes.tree_root ) {
        free_nodes.tree_root = save;
    }
    save->links[rotation] = root;
    root->list_start->parent = save;
    paint_node( root, RED );
    paint_node( save, BLACK );
    return save;
}

/// @brief double_rotation  performs two rotations to a red-black tree, one in a direction and the
///                         other the opposite direction. A grandchild moves into root position.
/// @param *root            the root around which we will double rotate.
/// @param rotation         the first direction for the first rotation. Its opposite is next.
/// @return                 the grandchild that has moved into the root position.
static rb_node *double_rotation( rb_node *root, rb_node *parent, tree_link rotation )
{
    root->links[!rotation] = single_rotation( root->links[!rotation], root, !rotation );
    return single_rotation( root, parent, rotation );
}

///    Static Red-Black Tree Insertion Helper Function

/// @brief add_duplicate  this implementation stores duplicate nodes in a linked list to prevent the
///                       rotation of duplicates in the tree. This adds the duplicate node to the
///                       linked list of the node already present.
/// @param *head          the node currently organized in the tree. We will add to its list.
/// @param *to_add        the node to add to the linked list.
static void add_duplicate( rb_node *head, duplicate_node *to_add, rb_node *parent )
{
    to_add->header = head->header;
    // This will tell us if we are coalescing a duplicate node. Only linked list will have NULL.
    if ( head->list_start == free_nodes.list_tail ) {
        to_add->parent = parent;
    } else {
        to_add->parent = head->list_start->parent;
        head->list_start->parent = NULL;
    }

    // Get the first next node in the doubly linked list, invariant and correct its left field.
    head->list_start->links[P] = to_add;
    to_add->links[N] = head->list_start;
    head->list_start = to_add;
    to_add->links[P] = (duplicate_node *)head;
}

///     Static Red-Black Tree Insertion Logic

/// @brief insert_rb_topdown  performs a topdown insertion of a node into a redblack tree, fixing
///                           violations that have occured on the way down. If the node is a
///                           duplicate, it will be added to a doubly linked list for that size.
/// @param                    the heap node to insert into the tree or list.
static void insert_rb_topdown( rb_node *current )
{
    size_t key = get_size( current->header );
    paint_node( current, RED );

    tree_link prev_link = L;
    tree_link link = L;
    rb_node *ancestor = free_nodes.black_nil;
    rb_node *gparent = free_nodes.black_nil;
    rb_node *parent = free_nodes.black_nil;
    rb_node *child = free_nodes.tree_root;
    size_t child_size = 0;

    // Unfortunate infinite loop structure due to odd nature of topdown fixups. I kept the search
    // and pointer updates at top of loop for clarity but can't figure out how to make proper
    // loop conditions from this logic.
    for ( ;; ancestor = gparent, gparent = parent, parent = child, prev_link = link,
             child = child->links[link = child_size < key] ) {

        child_size = get_size( child->header );
        if ( child_size == key ) {
            add_duplicate( child, (duplicate_node *)current, parent );
        } else if ( child == free_nodes.black_nil ) {
            child = current;
            child_size = key;
            parent->links[link] = current;
            current->links[L] = free_nodes.black_nil;
            current->links[R] = free_nodes.black_nil;
            current->list_start = free_nodes.list_tail;
        } else if ( get_color( child->links[L]->header ) == RED && get_color( child->links[R]->header ) == RED ) {
            paint_node( child, RED );
            // If you split a black node down the tree the black height remains constant.
            paint_node( child->links[L], BLACK );
            paint_node( child->links[R], BLACK );
        }

        // Our previous fix could have created a violation further up tree.
        if ( get_color( parent->header ) == RED && get_color( child->header ) == RED ) {
            tree_link ancestor_link = ancestor->links[R] == gparent;
            if ( child == parent->links[prev_link] ) {
                ancestor->links[ancestor_link] = single_rotation( gparent, ancestor, !prev_link );
            } else {
                ancestor->links[ancestor_link] = double_rotation( gparent, ancestor, !prev_link );
            }
        }
        if ( child_size == key ) {
            break;
        }
    }

    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = child;
    }
    paint_node( free_nodes.tree_root, BLACK );
    free_nodes.total++;
}

///    Red-Black Tree Deletion Helper Functions

/// @brief rb_transplant  replaces node with the appropriate node to start balancing the tree.
/// @param *parent        the parent of the node we are removing.
/// @param *remove        the node we are removing from the tree.
/// @param *replacement   the node that will fill the remove position. It can be free_nodes.black_nil.
static void rb_transplant( rb_node *parent, rb_node *remove, rb_node *replacement )
{
    if ( parent == free_nodes.black_nil ) {
        free_nodes.tree_root = replacement;
    } else {
        parent->links[parent->links[R] == remove] = replacement;
    }
    if ( replacement != free_nodes.black_nil ) {
        replacement->list_start->parent = parent;
    }
}

/// @brief delete_duplicate  will remove a duplicate node from the tree when the request is coming
///                          from a call from malloc. Address of duplicate does not matter so we
///                          remove the first node from the linked list.
/// @param *head             We know this node has a next node and it must be removed for malloc.
/// @return                  the duplicate spliced from the doubly linked list.
static rb_node *delete_duplicate( rb_node *head )
{
    duplicate_node *next_node = head->list_start;
    // Take care of the possible node to the right in the doubly linked list first. This could be
    // another node or it could be free_nodes.black_nil, it does not matter either way.
    next_node->links[N]->parent = next_node->parent;
    next_node->links[N]->links[P] = (duplicate_node *)head;
    head->list_start = next_node->links[N];
    free_nodes.total--;
    return (rb_node *)next_node;
}

///      Static Red-Black Tree Deletion Logic

/// @brief remove_node         checks for conditions necessary to remove a node with its inorder
///                            predecessor down the free_nodes. Removes a duplicate if one is
///                            encountered.
/// @param parent              the parent of the node we will be removing.
/// @param remove              the node we will remove and return.
/// @param replacement_parent  the parent of the node we will use to replace remove.
/// @param replacement         the indorder predecessor of the node to be removed.
/// @return                    the node that has been removed from the tree or doubly linked list.
static rb_node *remove_node( rb_node *parent, rb_node *remove, rb_node *replacement_parent, rb_node *replacement )
{

    // Quick return, node waiting in the linked list will replace if we found duplicate.
    if ( remove->list_start != free_nodes.list_tail ) {
        return delete_duplicate( remove );

    } else if ( remove->links[L] == free_nodes.black_nil || remove->links[R] == free_nodes.black_nil ) {
        tree_link nil_link = remove->links[L] != free_nodes.black_nil;
        rb_transplant( parent, remove, remove->links[!nil_link] );
    } else {
        if ( replacement != remove->links[R] ) {
            rb_transplant( replacement_parent, replacement, replacement->links[R] );
            replacement->links[R] = remove->links[R];
            replacement->links[R]->list_start->parent = replacement;
        }
        rb_transplant( parent, remove, replacement );
        replacement->links[L] = remove->links[L];
        if ( replacement->links[L] != free_nodes.black_nil ) {
            replacement->links[L]->list_start->parent = replacement;
        }
        replacement->list_start->parent = parent;
    }
    paint_node( replacement, get_color( remove->header ) );
    paint_node( free_nodes.black_nil, BLACK );
    paint_node( free_nodes.tree_root, BLACK );
    free_nodes.total--;
    return remove;
}

/// @brief delete_rb_topdown  performs a topdown deletion on a red-black tree fixing violations on
///                           the way down. It will return the node removed from the tree or a
///                           duplicate from a doubly linked list if a duplicate is waiting.
/// @param key                the size_t representing the node size in bytes we are in search of.
/// @return                   the node we have removed from the tree or doubly linked list.
static rb_node *delete_rb_topdown( size_t key )
{
    rb_node *gparent = free_nodes.black_nil;
    rb_node *parent = free_nodes.black_nil;
    rb_node *child = free_nodes.black_nil;
    rb_node *best = free_nodes.black_nil;
    rb_node *best_parent = free_nodes.black_nil;
    size_t best_fit_size = ULLONG_MAX;
    tree_link link = R;
    child->links[R] = free_nodes.tree_root;
    child->links[L] = free_nodes.black_nil;

    while ( child->links[link] != free_nodes.black_nil ) {
        tree_link prev_link = link;
        gparent = parent;
        parent = child;
        child = child->links[link];
        size_t child_size = get_size( child->header );
        link = child_size < key;

        // Best fit approximation and the best choice will win by the time we reach bottom.
        if ( link == L && child_size < best_fit_size ) {
            best_parent = parent;
            best = child;
        }
        // We can cut the link off early just to save more needless fixup work if duplicate.
        if ( key == child_size && best->list_start != free_nodes.list_tail ) {
            return delete_duplicate( best );
        }

        // Double black needs our attention due to black height requirements.
        if ( get_color( child->header ) == BLACK && get_color( child->links[link]->header ) == BLACK ) {

            // We need access to six pointers, and two directions. Decomposition is difficult.
            rb_node *nxt_sibling = child->links[!link];
            rb_node *sibling = parent->links[!prev_link];
            if ( get_color( nxt_sibling->header ) == RED ) {
                gparent = nxt_sibling;
                parent = parent->links[prev_link] = single_rotation( child, parent, link );
                if ( child == best ) {
                    best_parent = gparent;
                }
                // Our black height will be altered. Recolor.
            } else if ( sibling != free_nodes.black_nil && get_color( nxt_sibling->header ) == BLACK
                        && get_color( sibling->links[!prev_link]->header ) == BLACK
                        && get_color( sibling->links[prev_link]->header ) == BLACK ) {
                paint_node( parent, BLACK );
                paint_node( sibling, RED );
                paint_node( child, RED );
                // Another black is waiting down the tree. Red violations and path violations possible.
            } else if ( sibling != free_nodes.black_nil && get_color( nxt_sibling->header ) == BLACK ) {
                tree_link to_parent = gparent->links[R] == parent;
                rb_node *new_gparent = free_nodes.black_nil;

                // These two cases may ruin lineage of node to be removed. Repair if necessary.
                if ( get_color( sibling->links[prev_link]->header ) == RED ) {
                    new_gparent = gparent->links[to_parent] = double_rotation( parent, gparent, prev_link );
                    if ( best == parent ) {
                        best_parent = new_gparent;
                    }
                } else if ( get_color( sibling->links[!prev_link]->header ) == RED ) {
                    new_gparent = gparent->links[to_parent] = single_rotation( parent, gparent, prev_link );
                    if ( best == parent ) {
                        best_parent = sibling;
                    }
                }
                paint_node( child, RED );
                paint_node( gparent->links[to_parent], RED );
                paint_node( gparent->links[to_parent]->links[L], BLACK );
                paint_node( gparent->links[to_parent]->links[R], BLACK );
                // Either single or double rotation has adjusted grandparent position.
                gparent = new_gparent;
            }
        }
    }
    return remove_node( best_parent, best, parent, child );
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
        return delete_rb_topdown( get_size( tree_node->header ) );
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

        // Coalesce the head of a doubly linked list in the tree. Remove and make a new head.
    } else {
        remove_head( tree_node, lft_tree_node, tree_node->links[R] );
    }
    free_nodes.total--;
    return to_coalesce;
}

///    Static Heap Helper Function

/// @brief init_free_node  initializes a newly freed node and adds it to a red black tree.
/// @param *to_free        the heap_node to add to the red black tree.
/// @param block_size      the size we use to initialize the node and find the right place in tree.
static void init_free_node( rb_node *to_free, size_t block_size )
{
    to_free->header = block_size | LEFT_ALLOCATED | RED_PAINT;
    to_free->list_start = free_nodes.list_tail;
    init_footer( to_free, block_size );
    get_right_neighbor( to_free, block_size )->header &= LEFT_FREE;
    insert_rb_topdown( to_free );
}

/// @brief *split_alloc  determines if a block should be taken entirely or split into two blocks. If
///                      split, it will add the newly freed split block to the free red black tree.
/// @param *free_block   a pointer to the node for a free block in its entirety.
/// @param request       the user request for space.
/// @param block_space   the entire space that we have to work with.
/// @return              a void pointer to generic space that is now ready for the client.
void *split_alloc( rb_node *free_block, size_t request, size_t block_space )
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

///    Shared Heap Functions

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

    // Two sentinels will point to same address. Makes it clear which we refer to and saves space.
    free_nodes.black_nil = heap.client_end;
    free_nodes.list_tail = heap.client_end;
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
        rb_node *found_node = delete_rb_topdown( client_request );
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
        // Better to memmove than not coalesce left, give up, and leave possible space behind.
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

///      Shared Debugging

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

///   Shared Printing Debugger

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
