#include "../headers/mcts.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>


#define MAX_LOOPS 20000


static player_t PLAYING_AS = PLAYER_B;
static uint32_t MAX_ITER = 20;
static node_t* tree_root = NULL;

/*
===========================================
============= HELPER FUNCTIONS ============
===========================================
*/


/**
 * Compute the UCB weight of a non-leaf node according to Kocsis and Szepesvári (UCB).
 * 
 * @param node the MCTS node whose weight we want to compute. Must not be leaf.
 * 
 * @return the weight of that node computed via UCB. A relatively high value makes it very likely to be selected;
 * 0.0 if 'node' is NULL.
*/
static double compute_UCB(node_t* node) {
    // Preliminary check
    if (node == NULL) return 0.0;

    double N = (double) node->parent->nb_visits;
    double n = (double) node->nb_visits;
    double w = (double) node->nb_wins;
    if (n != 0 && N != 0) return w/n + sqrt(2*log(N)/n);    // usual case
    if (N == 0) return 0.0;    // empty MTCS tree
    return 2*log(N);    // leaf who encountered an error during its first simulation (during the node creation)
}


/**
 * Applies the simulation of the MCTS algorithm on one node. All moves are random amongst the valid ones.
 * The node values are NOT modifid
 * 
 * @param init_state the initial state of the game.
 * 
 * @returns 1 if the simulation results in a win;
 * 0 if the simulation results in a loss or a draw;
 * -1 if the simulation results in an exception;
 * MEMERROR if the memory allocation is unsuccesful or if init_state is NULL
*/
static int8_t MTCS_simulation(game_t* init_state) {
    if (init_state == NULL) return MEMERROR;
    player_t winner_check = winner(init_state);
    if (winner_check >= 0) return winner_check;
    else if (winner_check == ARG_ERROR) return -1;

    game_t* playout = copy(init_state);
    if (playout == NULL) {
        return MEMERROR;
    }

    int8_t res = 0;
    while (res != 1 && res != ARG_ERROR) {
        col_t first_try_col = random() % ROW_LENGTH;
        res = play_auto(playout, first_try_col);
        for (col_t next = 1; next < ROW_LENGTH && res == -2; next++)
                res = play_auto(playout, (first_try_col+next)%ROW_LENGTH);
        if (res == -2) {    // all columns are full but no winner: draw
            game_destroy(playout);
            return 0;
        }
    }
    if (res == ARG_ERROR) {
        game_destroy(playout);
        return -1;
    }
    // res == 1 : victory achieved
    player_t w = winner(playout);
    game_destroy(playout);

    if (w == PLAYING_AS) return 1;
    else if (w == ARG_ERROR) return -1;
    else return 0;
}


/**
 * Creates a MCTS node. 
 * 
 * @param state is the state of a paused game. Is assumed non-null.
 * @param parent is the parent node. Is NULL for the root tree, and assumed non-null for all other nodes.
 * 
 * @returns The pointer to the newly created node in case of success;
 * NULL in case of memory allocation error.
*/
static node_t* create_node_and_simulate(game_t* state, node_t* parent) {
    node_t* new_node = (node_t*) malloc(sizeof(node_t));
    if (new_node == NULL) return NULL;
    for (col_t col = 0; col < ROW_LENGTH; col++) new_node->children[col] = NULL;
    new_node->state = state;
    new_node->parent = parent;

    int8_t sim = MTCS_simulation(state);
    if (sim == MEMERROR) {
        free(new_node);
        return NULL;
    }
    else if (sim == -1) {
        new_node->nb_visits = 0;
        new_node->nb_wins = 0;
    } else {
        new_node->nb_visits = 1;
        new_node->nb_wins = sim;
    }
    return new_node;
}


/**
 * Destroys a MCTS node and recursively frees its content and its children's
*/
static void recursive_node_destroy(node_t* node) {
    if (node == NULL) return;
    game_destroy(node->state);
    for (col_t c = 0; c < ROW_LENGTH; c++) 
            recursive_node_destroy(node->children[c]);
    free(node);

}


/**
 * Returns whether a MCTS node is a leaf
 * 
 * @param node the node to test
 * 
 * @returns a boolean that represents whether the node is a leaf.
*/
static boolean is_leaf(node_t* node) {

    boolean leaf = 1;
    for (col_t col = 0; col < ROW_LENGTH; col++)
        if (node->children[col] != NULL) leaf = 0;
    return (node->nb_visits <= (uint32_t) 1 || leaf);
}


/**
 * Selects the leaf obtained by following the path of nodes with the highest UCB scores.
 * 
 * @param node the root node
 * 
 * @returns the selected leaf node, which will undergo the expansion step of the MCTS algorithm.
*/
static node_t* MCTS_selection(node_t* node) {
    // Preliminary check
    if (is_leaf(node)) return node;

    uint8_t nb_ties = 1;
    double max_UCB = 0.0;
    node_t* max_node = NULL;
    for (uint8_t i = 0; i < ROW_LENGTH; i++) {
        node_t* n = node->children[i];
        if (n != NULL) {
            double UCB = compute_UCB(n);
            if (UCB > max_UCB) {
                max_UCB = UCB;
                max_node = n;
                nb_ties = 1;
            } else if (UCB == max_UCB) nb_ties++;
        }
    }
    if (nb_ties == 1) return MCTS_selection(max_node);

    // else there are [nb_ties] nodes with the same UCB -> pick one at random
    col_t selected = (uint8_t) (random() % nb_ties);
    for (col_t i = 0; i < ROW_LENGTH; i++) {
        node_t* n = node->children[i];
        if (compute_UCB(n) == max_UCB) selected--;
        if (selected < 0) return MCTS_selection(n);
    }

    return MCTS_selection(max_node); // should never get there. we choose the last children with the highest UCB
}


/**
 * Applies a custom expansion step of the MCTS algorithm where *one node is created for each possible move* and simulates a playout for each of them.
 * An invalid move (including column full) or memory allocation error leads the child node to be NULL.
 * 
 * @param selected_leaf the MCTS node selected during the selection step of the MCTS algorithm. Is assumed to be non-null.
*/
static void MCTS_expansion_simulation(node_t* selected_leaf) {
    // because otherwise already finished games get selected in a loop in function MCTS() and lead to pseudo-infinite loops
    player_t w = winner(selected_leaf->state);
    if (w == PLAYING_AS) {    // case selected node is a win for the ai
        selected_leaf->nb_wins += 7;
        selected_leaf->nb_visits += 7;
        for (col_t col = 0; col < ROW_LENGTH; col++) selected_leaf->children[col] = NULL;
        return;
    } else if (w == 1-PLAYING_AS) {    // case selected node is a win for the human
        selected_leaf->nb_visits += 7;
        for (col_t col = 0; col < ROW_LENGTH; col++) selected_leaf->children[col] = NULL;
        return;
    }

    for (col_t col = 0; col < ROW_LENGTH; col++) {
        game_t* new_state = play_copy_auto(selected_leaf->state, col);
        if (new_state == NULL) selected_leaf->children[col] = NULL;
        else selected_leaf->children[col] = create_node_and_simulate(new_state, selected_leaf);
    }
}


/**
 * Applies the backpropagation step of the MCTS algorithm to the specified node.
 * 
 * @param selected_old_leaf is the node that was selected in the selection step of the MCTS. All its children are leaves.
*/
static void MTCS_backpropagation(node_t* selected_old_leaf) {
    uint32_t incr_wins = 0;
    uint32_t incr_visits = 0;

    // Computing the total increments to backpropagate
    for (col_t col = 0; col < ROW_LENGTH; col++) {
        if (selected_old_leaf->children[col] != NULL) {
            incr_visits += selected_old_leaf->nb_visits;
            incr_wins += selected_old_leaf->nb_wins;    // is supposed to be 0 or 1
        }
    }

    // Backpropagating the increments to the ancestor nodes
    for (node_t* n = selected_old_leaf; n != NULL; n = n->parent) {
        n->nb_visits += incr_visits;
        n->nb_wins += incr_wins;
    }
}

/**
 * Progress in the tree by one level of depths, designing the children of tree_root at index selected_col as the new tree_root.
 * Updates global variable tree_root and frees the other, unused children.
 * 
 * @param selected_col the move that has been played in [0, 6].
*/
static void progress_in_tree(col_t selected_col) {
    node_t* selected_node = tree_root->children[selected_col];
    tree_root->children[selected_col] = NULL;
    recursive_node_destroy(tree_root);
    tree_root = selected_node;
    selected_node->parent = NULL;
}

/**
 * Fills the global variable tree_root using the MCTS algorithm. Then, selects the best estimated move, adapts tree_root
 * to take notice of that selection, and returns the selected move. Assumes it is the AI's turn to play.
 * 
 * @returns the column in [0-6] selected by the MCTS algorithm to play.
 * MCTS_FAIL if the MCTS algorithm applicaiton fails (extreme error)
*/
static col_t MCTS() {
    // Runs the algorithm
    uint32_t loops = 0;    // there to prevent infinite loops when the selected node won't change
    while (tree_root->nb_visits < MAX_ITER-7 && loops < MAX_LOOPS) {
        node_t* selected = MCTS_selection(tree_root);
        MCTS_expansion_simulation(selected);
        MTCS_backpropagation(selected);
        loops++;
    }

    // Selects the most visited move
    uint32_t max_visits = 0;
    col_t selected_col = -1;
    for (col_t col = 0; col < ROW_LENGTH; col++) {
        node_t* tested = tree_root->children[col];
        if (tested == NULL) continue;
        boolean has_more_visits = (tested->nb_visits > max_visits);
        boolean has_same_visits_more_wins = (tested->nb_visits == max_visits && tested->nb_wins > tree_root->children[selected_col]->nb_wins);
        if (has_more_visits || has_same_visits_more_wins) {
            max_visits = tested->nb_visits;
            selected_col = col;
        }
    }
    if (selected_col == -1) return MCTS_FAIL;
    
    // Updates the tree to take it into account
    progress_in_tree(selected_col);
    
    return selected_col;
}


/*
===========================================
==================== API ==================
===========================================
*/



col_t init_MCTS(player_t playing_as, uint32_t max_visits) {
    if (max_visits < 8 || (playing_as != PLAYER_A && playing_as != PLAYER_B)) return ARG_ERROR;
    game_t* init_game = game_init(PLAYER_A);
    if (init_game == NULL) return -1;
    PLAYING_AS = playing_as;
    MAX_ITER = max_visits;

    tree_root = create_node_and_simulate(init_game, NULL);
    if (tree_root == NULL) {
        free(init_game);
        return -1;
    }
    for (col_t col = 0; col < ROW_LENGTH; col++) {
        game_t* game_continuation = play_copy_auto(init_game, col);
        if (game_continuation == NULL) {    // memory alloc error, because invalid move is supposed impossible here
            recursive_node_destroy(tree_root);
        }
        tree_root->children[col] = create_node_and_simulate(game_continuation, tree_root);
    }

    // If the IA is PLAYER_A : runs the MCTS from empty game, updates the tree and returns result
    if (playing_as == PLAYER_A) {
        col_t first_move = MCTS();
        progress_in_tree(first_move);
        return first_move;
    }
    else return ROW_LENGTH;    // implicit playing_as == PLAYER_B
}


void destroy_MCTS() {
    recursive_node_destroy(tree_root);
}


col_t input_MCTS(col_t col) {
    if (col < 0 || col >= ROW_LENGTH) return ARG_ERROR;
    if (tree_root->children[col] == NULL) return -1;
    progress_in_tree(col);
    return MCTS();
}


void print_state() {
    print_game(tree_root->state);
    printf("=> Confidence : %.3f (%d visits)\n", (double) tree_root->nb_wins / (double) tree_root->nb_visits, tree_root->nb_visits);
}