// Red-black tree implementation

#include <assert.h>
#include <stddef.h>

// See to also implement AVL tree:
// Use RB tree if often modified (for example, windows),
// use AVL tree if seldom modified (for example, atom names).

#if 0
#pragma mark Type definitions
#endif

typedef enum color { BLACK, RED } color_t;

typedef struct node {
	struct node *parent;
	struct node *left;
	struct node *right;
	color_t color;
	unsigned long key;
	void * value;
} node_t;


#if 0
#pragma mark Helper functions
#endif

static node_t *get_parent( node_t *a_node )
{
	return a_node ? a_node->parent : NULL;
}

static node_t *get_grand_parent( node_t *a_node )
{
	return (a_node && a_node->parent) ? a_node->parent->parent : NULL;
}

static node_t *get_sibling( node_t *a_node )
{
	node_t *result;
	node_t *parent = get_parent( a_node );
	
	if (parent) {
		if (a_node == parent->left) {
			result = parent->right;
		} else if (a_node == parent->right) {
			result = parent->left;
		} else {
			result = NULL; // Broken tree
		}
	} else {
		result = NULL;
	}

	return result;
}

static node_t *get_uncle( node_t *a_node )
{
	return get_sibling( get_parent( a_node ) );
}

//   p           p        6          6
//   │           │        │          │
//  *n           nr       2          4
// ┌─┴─┐       ┌─┴─┐    ┌─┴─┐      ┌─┴─┐
// nl *nr=t    n   tr   1   4      2   5
//   ┌─┴─┐   ┌─┴─┐        ┌─┴─┐  ┌─┴─┐
//  *tl  tr  nl  tl       3   5  1   3
//
static void rotate_left( node_t *a_node )
{
	assert( a_node );
	node_t *temp_node = a_node->right;
	assert( temp_node );
	
	a_node->right = temp_node->left;
	temp_node->left = a_node;
	a_node->parent = temp_node;

	if (a_node->right) { a_node->right->parent= a_node; }

	node_t *parent = get_parent( a_node );
	if (parent) {
		if (a_node == parent->left) {
			parent->left = temp_node;
		} else if (a_node == parent->right) {
			parent->right = temp_node;
		}
	}

	temp_node->parent = parent;
}

//      p       p            6      6
//      │       │            │      │
//     *n       nl           4      2
//    ┌─┴─┐   ┌─┴─┐        ┌─┴─┐  ┌─┴─┐
// t=*nl  nr  tl  n        2   5  1   4
//  ┌─┴─┐       ┌─┴─┐    ┌─┴─┐      ┌─┴─┐
// *tl  tr      tr  nr   1   3      3   5
//
static void rotate_right( node_t *a_node )
{
	assert( a_node );
	node_t *temp_node = a_node->left;
	assert( temp_node );
	
	a_node->left = temp_node->right;
	temp_node->right = a_node;
	a_node->parent = temp_node;

	if (a_node->left) { a_node->left->parent= a_node; }

	node_t *parent = get_parent( a_node );
	if (parent) {
		if (a_node == parent->left) {
			parent->left = temp_node;
		} else if (a_node == parent->right) {
			parent->right = temp_node;
		}
	}

	temp_node->parent = parent;
}


#if 0
#pragma mark Tree operations
#endif

// Search for the node matching the key in the subtree of root.
// Return this node if it exists.
// If the node doesn't exist and a_parent is defined, set it to the parent node
// where a node with this key could be inserted.
node_t * find_node( const node_t * a_root, unsigned long a_key, node_t ** a_parent )
{
	node_t *current = a_root;

	if (current) {
		bool end = false;
		while (!end) {
			if (a_key < current->key) {
				if (!current->left) {
					if (a_parent) { *a_parent = current; }
					end = true;
				}
				current = current->left;
			} else if (a_key > current->key) {
				if (!current->right) {
					if (a_parent) { *a_parent = current; }
					end = true;
				}
				current = current->right;
			} else /* if (a_key == current->key) */ {
				// if (a_parent) { *a_parent = current->parent; }
				end = true;
			}
		}
	} else if (a_parent) {
		*a_parent = NULL;
	}

	return current;
}


// Insert a node in the tree, if its key isn't already present in the tree.
// Doesn't re-balance the tree.
// TODO: Should use find_node
static bool basic_insert( node_t *a_root, node_t *a_node )
{
	// This function can't be used to insert a subtree.
	assert( a_node->left == NULL );
	assert( a_node->right == NULL );
	// We also want to prevent insertion of a node in multiple trees:
	// A deep copy of a node must actually clear the parent pointer
	// along either creating new children nodes (tree_copy) or clearing children pointers (node_copy).
	assert( a_node->parent == NULL );

	bool dup = false;

	if (a_root) {
		bool end = false;
		node_t *current = a_root;
		while (!end) {
			if (a_node->key < current->key) {
				if (current->left) {
					current = current->left;
				} else {
					current->left = a_node;
					a_node->parent = current;
					a_node->color = RED;
					end = true;
				}
			} else if (a_node->key > current->key) {
				if (current->right) {
					current = current->right;
				} else {
					current->right = a_node;
					a_node->parent = current;
					a_node->color = RED;
					end = true;
				}
			} else /* if (a_node->key == current->key) */ {
				dup = true; // We want unique keys
				end = true;
			}
		}
	} else {
		a_node->parent = NULL;
		a_node->color = BLACK;
	}

	return dup;
}


// Unless we inserted the first node in the tree, we've inserted a red node.
// If the parent is black, there's nothing to do.
// If the parent is red, we've broken a RB tree rule, so we need to straighten it.
//
//     gB         gR   
//   ┌─┴─┐      ┌─┴─┐
//   pR  uR →   pB  uB → Then act as if gR was newly inserted 
// ┌─┴        ┌─┴
// nR         nR
//
//     r                          r          r
//     │                          │          │
//     gB                         pR         pB
//   ┌─┴─┐                      ┌─┴─┐      ┌─┴─┐
//   pR  uB → rotate GP right:  nR  gB   → nR  gR
// ┌─┴─┐                          ┌─┴─┐      ┌─┴─┐
// nR  sB                         sB  uB     sB  uB
//
//   r                           r          r
//   │                           │          │
//   gB                          pR         pB
// ┌─┴─┐                       ┌─┴─┐      ┌─┴─┐
// uB  pR  → rotate GP left:   gB  nR →   gR  nR
//   ┌─┴─┐                   ┌─┴─┐      ┌─┴─┐
//   sB  nR                  uB  sB     uB  sB
//
//     r                          r                             r                r
//     │                          │                             │                │
//     gB                         gB                            nR               nB
//   ┌─┴─┐                      ┌─┴─┐                       ┌───┴───┐        ┌───┴───┐
//   pR  uB → rotate pR left:   nR  uB → rotate gB right:   pR      gB   →   pR      gR
// ┌─┴─┐                      ┌─┴─┐                       ┌─┴─┐   ┌─┴─┐    ┌─┴─┐   ┌─┴─┐
// sB  nR                    *pR  rB                      sB  lB  rB  uB   sB  lB  rB  uB
//   ┌─┴─┐                  ┌─┴─┐
//   lB  rB                 sB  lB
//
//   r                         r                             r                r
//   │                         │                             │                │
//   gB                        gB                            nR               nB
// ┌─┴─┐                     ┌─┴─┐                       ┌───┴───┐        ┌───┴───┐
// uB  pR  → rotate P right: uB  nR  → rotate gB left:   gB      pR   →   gR      pR
//   ┌─┴─┐                     ┌─┴─┐                   ┌─┴─┐   ┌─┴─┐    ┌─┴─┐   ┌─┴─┐
//   nR  sB                    lB *pR                  uB  lB  rB  sB   uB  lB  rB  sB
// ┌─┴─┐                         ┌─┴─┐
// lB  rB                        rB  sB
//
static void repair_insert( node_t *a_node )
{
	assert( a_node );

	node_t *current = a_node;
	node_t *parent = get_parent( current );

	while (parent && parent->color == RED ) {
		node_t *uncle = get_sibling( parent );
		node_t *grandparent = get_parent( parent );

		if (uncle && uncle->color == RED) {
			parent->color = BLACK;
			uncle->color = BLACK;
			grandparent->color = RED;
			current = grandparent;
			parent = get_parent( current );
		} else /* if (!uncle || uncle->color == BLACK) */ { // If parent red and uncle black, re-balance the tree.
			node_t *tmp = current;
			if (current == parent->right && parent == grandparent->left) {
				rotate_left( parent );
				current = parent; // == current->left
				parent = tmp;
			} else if (current == parent->left && parent == grandparent->right) {
				rotate_right( parent );
				current = parent; // == current->right
				parent = tmp;
			}

			if (current == parent->left) {
				rotate_right( grandparent );
			} else {
				rotate_left( grandparent );
			}
			parent->color = BLACK;
			grandparent->color = RED;
			break; // Exit loop.
		}
	}
}


static void rec_insert_repair_tree( node_t *a_node )
{
	node_t *parent = get_parent( a_node );

	if (!parent) {
		a_node->color = BLACK; // Managed in basic insert, check that a rotation couldn't cause a change of colour.
	} else if (parent->color == RED) {
		node_t *uncle = get_sibling( parent );
		node_t *grandparent = get_parent( parent );
		if (uncle && uncle->color == RED) { // If parent and uncle are red, color them black and grandparent red, then check upstream
			parent->color = BLACK;
			uncle->color = BLACK;
			grandparent->color = RED;
			rec_insert_repair_tree( grandparent );
		} else /* if (!uncle || uncle->color == BLACK) */ { // If parent red and uncle black, re-balance the tree.
			node_t *current = a_node;
			if (a_node == parent->right && parent == grandparent->left) {
				rotate_left( parent );
				current = parent; // == a_node->left
				parent = a_node;
			} else if (a_node == parent->left && parent == grandparent->right) {
				rotate_right( parent );
				current = parent; // == a_node->right
				parent = a_node;
			}

			if (current == parent->left) {
				rotate_right( grandparent );
			} else {
				rotate_left( grandparent );
			}
			parent->color = BLACK;
			grandparent->color = RED;
		}
	}
}

node_t *insert( node_t *a_root, node_t *a_node )
{
	basic_insert( a_root, a_node );
	rec_insert_repair_tree( a_node );

	node_t *root = a_node;
	while (get_parent( root )) {
		root = get_parent( root );
	}

	return root;
}

// Puts a_child subtree where a_node subtree was.
void replace_node( node_t *a_node, node_t *a_child )
{
	assert( a_node );
	assert( a_child );

	a_child->parent = a_node->parent;
	if (a_node == a_node->parent->left) {
		a_node->parent->left = a_child;
	} else if (a_node == a_node->parent->right) {
		a_node->parent->right = a_child;
	} else {
		// broken tree
	}
}

void *delete_one_child( node_t *a_node )
{
	assert( a_node );
	assert( !(a_node->left && a_node->right) );

	anode_t *child = a_node_right ? a_node->right : a_node->left;
	assert( child ); // Doesn't make sense, "at most one non-leaf child" allows for 0, i.e. 2 leaf child

	replace_node( a_node, child );
	if (a_node->color == BLACK) {
		if (child->color == RED) {
			child->color == BLACK
		} else if (child->parent) {
			node_t *sibling = get_sibling( child );
			if (sibling->color == RED) {
				child->parent->color = RED;
				sibling->color = BLACK;
				if (child == child->parent->left) {
					rotate_left( child->parent );
				} else if (child == child->parent->right) {
					rotate_right( child->parent );
				} else {
					// Broken tree
				}
			}
			// delete_case_3( child )
			sibling = get_sibling( child ); // could have changed after rotation (?)
			if (child->parent->color == BLACK && sibling->color == BLACK && sibling->left->color == BLACK && sibling->right->color == BLACK) (
				sibling->color = RED;
				// delete_case_1( child->parent ); // recursion
			} else if (child->parent->color == RED && sibling->color == BLACK && sibling->left->color == BLACK && sibling->right->color == BLACK) {
				sibling->color == RED;
				child->parent->color = BLACK;
			} else {
				if (sibling->color == BLACK) { // True
					if (child == child->parent->left && sibling->right->color == BLACK && sibling->left->color == RED) { // last && == True
						sibling->color = RED;
						sibling->left->color = BLACK;
						rotate_right( sibling );
					} else if (child == child->parent->right && sibling->left->color == BLACK && sibling->right->color == RED) { // last && == True
						sibling->color = RED;
						sibling->right->color = BLACK
						rotate_left( sibling );
					}
				}
				sibling = get_sibling( child ); // could have changed after rotation (?)
				sibling->color = child->parent->color;
				child->parent->color = BLACK;
				if (child == child->parent->left) {
					sibling->right->color = BLACK;
					rotate_left( child->parent );
				} else if (child == child->parent->right) {
					sibling->left->color = BLACK;
					rotate_right( child->parent );
				} else {
					// Broken tree
				}
			}
		}
	}

	void *result = a_node->value;
	free( a_node );
	return result;
}


void * map_lookup( const node_t * a_root, unsigned long a_key, bool *a_found )
{
	node_t *node = find_node( a_root, a_key, NULL );
	if (a_found) { *a_found = node ? true : false; } // Or simply *a_found = node
	return node ? node->value : NULL;
}

