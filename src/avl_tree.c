// AVL tree implementation

#if 0
#pragma mark Type definitions
#endif

typedef struct avl_node {
	struct avl_node *parent;
	struct avl_node *left;
	struct avl_node *right;
	int8_t balance;
	unsigned long key;
	void * value;
} avl_node_t;


#if 0
#pragma mark Helper functions
#endif

//   u           u        u          u
//   ║           │        │          │
//   r           p        2          4
// ┌─╨─╖       ┌─┴─┐    ┌─┴─┐      ┌─┴─┐
// x   p       r   y    1   4      2   5
//   ╓─╨─┐   ┌─┴─┐        ┌─┴─┐  ┌─┴─┐
//   c   y   x   c        3   5  1   3
//
//
// Before:
//   Constraints:
//     H(y) = X
//     H(c) ∈ [ X - 2, X + 2 ]
//     H(p) = max( H(y), H(c) ) + 1 ⇒ H(p) ∈ [ X + 1, X + 3 ]
//     H(x) ∈ [H(p) - 2, H(p) + 2] ⇔ H(x) ∈ [ X - 1, X + 5 ]
//     H(r) = max( H(p), H(x) ) + 1 ⇒ H(r) ∈ [ X + 2, X + 6 ]
//   Values:
//     H(y) = X
//     H(c) = X - B(p)
//     H(p) = max( X, X - B(p) ) + 1
//          = X + 1 + max( 0, -B(p) )
//     H(x) = H(p) - B(r)
//          = X + 1 - B(r) + max( 0, -B(p) )
//     H(r) = max( H(p), H(x) ) + 1 = max( H(p), H(p) - B(r) ) + 1 = H(p) + max( 0, -B(r) ) + 1
//          = X + 1 + max( 0, -B(p) ) + max( 0, -B(r) ) + 1
//          = X + 2 + max( 0, -B(p) ) + max( 0, -B(r) )
// After:
//  Values:
//     H(y) = X
//     H(c) = X - oB(p)
//     H(x) = X + 1 - oB(r) + max( 0, -oB(p) )
//     H(r) = max( H(c), H(x) ) + 1 = max( X - oB(p), X + 1 - oB(r) + max( 0, -oB(p) ) ) + 1
//          = X + 1 + max( -oB(p), 1 - oB(r) + max( 0, -oB(p) ) )
//          = (oB(p) ≤ 0) X + 1 + max( -oB(p), 1 - oB(r) - oB(p) ) = X + 1 - oB(p) + max( 0, 1 - oB(r) )
//          = (oB(p) ≥ 0) X + 1 + max( -oB(p), 1 - oB(r) )

//     B(r) = H(c) - H(x) = X - oB(p) - (X + 1 - oB(r) + max( 0, - oB(p) ))
//          = X - oB(p) - X - 1 + oB(r) - max( 0, -oB(p) )
//          = -1 - oB(p) + oB(r) - max( 0, -oB(p) )
//          = -1 + oB(r) - max( oB(p), 0 )
//     B(p) = H(y) - H(r) = X - (X + 1 - oB(r) + max( -oB(p), max( 0, -oB(p) ) ) + 1)
//                        = X - (X + 1 + max( -oB(p), 1 - oB(r) + max( 0, -oB(p) ) ) )
//                        = -1 - max( -oB(p), 1 - oB(r) + max( 0, -oB(p) ) )
//          = -1 - max(  -oB(p), max( 0, 1 - oB(p) ) - oB(r) )
//
// Let's try
//
//   u           u    B(r) = 1 - max( 0, 1 ) - 1 + 0 = -1
//   ║           │    B(p) = -max( 1, max( 0, 1 ) + 1 - 0 ) - 1 = -2 - 1 = -3
//  (0)         (-3)  => OK, except we unbalanced the tree
// ┌─╨─╖       ┌─┴─┐
// 3  (-1)    (-1) 1
//   ╓─╨─┐   ┌─┴─┐
//   2   1   3   2
//
//   u           u    B(r) =  1
//   ║           │    B(p) = -3
//  (2)         (-3)
// ┌─╨─╖       ┌─┴─┐
// 2  (-2)    (1)  1
//   ╓─╨─┐   ┌─┴─┐
//   3   1   2   3

void avl_rotate( avl_node_t *a_pivot )
{
	assert( a_pivot );
	avl_node_t *root  = a_pivot->parent;
	
	if (root) {
		avl_node_t *child;
		if (a_pivot == root->left) {
			child = a_pivot->right;
			a_pivot->right = root;
			root->left = child;
		} else if (a_pivot == root->right) {
			child = a_pivot->left;
			root->right = child; // 3
			a_pivot->left = root; // 5
		} else {
			// Broken tree
		}
		avl_node_t *upper = root->parent;
		root->parent = a_pivot; // 2
		a_pivot->parent = upper; // 4
		if (child) child->parent = root; // 6
		if (upper) {
			if (upper->left == root) {
				upper->left = pivot; // 1
			} else if (upper->right == root) {
				upper->right = pivot;
			} else {
				// Broken tree
			}
		} else {
			// Tree root has changed.
		}
		if (pivot->balance == 0) { // When do we have these ? Deletion probably ?
			root->balance = 1;
			pivot->balance = -1;
		} else {
			root->balance = 0;
			pivot->balance = 0;
		}
	}
}


#if 0
#pragma mark Tree operations (working with nodes)
#endif

avl_node_t * avl_find_node( const avl_node_t * a_root, unsigned long a_key, avl_node_t ** a_parent )
{
	avl_node_t *current = a_root;

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

// Adding a node :
//
//  (0→-1) (1→0) (0→1) (-1→0)
//  ┌─┘    ┌─┴─┐   └─┐  ┌─┴─┐
//  0      0   0     0  0   0
//
// If balance ≠ 0, see parent
//
//   (1→0)   (0→-1) (-1→-2)
//   ┌─┴─┐   ┌─┴─┐   ┌─┘    (-1|1→0)
// -1|1  … -1|1  0 -1|1   →  ┌─┴─┐    after rotation, tree depth hasn't changed, only (0→-1) increased the depth.
//   │       │       │       0 (-2→0)
//   0       0       0
//
// When the depth increased, we need to check the parent…
//
//      (1→0)        (0→-1)      (-1→-2)     (-1→0)
//     ┌──┴──┐      ┌──┴──┐      ┌──┴──┐     ┌──┴──┐
//   (0→-1)  …    (0→-1)  …    (0→-1)  0 → -1|1 (-2→0)  after rotation, tree depth hasn't changed,
//   ┌─┴─┐        ┌─┴─┐        ┌─┴─┐         │   ┌─┴─┐  only (0→-1) increased the depth. Etc.
// -1|1  0      -1|1  0      -1|1  0         0   0   0
//   │            │            │
//   0            0            0
//
// When the depth increased, we need to check the parent…
//
//          (-1→-2)              (-1→0)
//        ┌────┴────┐          ┌────┴────┐
//      (0→-1)      ?         -1      (-2→0)
//     ┌──┴──┐   ┌──┴──┐     ┌─┴─┐   ┌───┴───┐
//   (0→-1)  *   c     d → -1|1  0   *       ?
//   ┌─┴─┐ ┌─┴─┐             │     ┌─┴─┐  ┌──┴──┐
// -1|1  0 a   b             0     a   b  c     d
//   │
//   0

// Removing a node :
//
// (0→1) (-1→0) (1→2) (0→-1) (1→0) (-1→-2)
//   └─┐          └─┐  ┌─┘          ┌─┘
//     0            …  0            …
//
// If abs(balance) == 2, rotate:
//
//   2         (0→-1)  2        (1→0)    2     2          a(1→0)
//   └─┐   →   ┌─┴─┐   └─┐      ┌─┴─┐    └─┐   └─┐        ┌──┴──┐
//     0     (2→1) b     1  → (2→0) b     -1 → a(0→1) → (2→0) (0→0)
//   ┌─┴─┐     └─┐       └─┐             ┌─┘     └─┐
//   a   b       a         b             a      (-1→0)
//
// If abs(balance) == 0, the depth decreased, we need to check the parent

// next:
// If node->right, next = node->right->while(left) else next = node->parent(while child == parent->right)

// prev:
// If node->left, prev = node->left->while(right) else next = node->parent(while child == parent->left)


#if 0
#pragma mark Map operations (working with keys and values)
#endif

