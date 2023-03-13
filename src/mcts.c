#include "../headers/mcts.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>


static player_t PLAYING_AS = PLAYER_B;
static uint32_t MAX_VISITS = 20;    // one visit == one game simulation
static node_t* tree_root = NULL;
static uint32_t nb_recombined_visits = 0; 

/*
===========================================
============= HELPER FUNCTIONS ============
===========================================
*/


// ============= NODES MANAGEMENT ============


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
    if (winner_check >= 0) return (winner_check != DRAW) ? winner_check : 0;    // The second clause represents a draw in init_state
    else if (winner_check == ARG_ERROR) return -1;

    game_t* playout = copy(init_state);
    if (playout == NULL) {
        return MEMERROR;
    }

    int8_t res = 0;
    while (res != 1 && res != 2 && res != ARG_ERROR) {
        col_t first_try_col = random() % ROW_LENGTH;
        res = play_auto(playout, first_try_col);
        for (col_t next = 1; next < ROW_LENGTH && res == -2; next++)
                res = play_auto(playout, (first_try_col+next)%ROW_LENGTH);
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
 * NULL in case of memory allocation error or if the argument 'state' is passed as NULL
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


// ============= DETECTING THREATS ============


/**
 * Analyses whether the next player can make a Connect4 during their direct turn.
 * 
 * @param game The game status.
 * 
 * @returns The column in [0, ROW_LENGTH[ which allows the next player to make a Connect4, if possible. In cases several columns
 * lead to a Connect4, returns the one the most on the left;
 * -1 if the player can not make a Connect4 in their direct next turn. In case of memory allocation errors,
 * false negatives may occur.
 * 
*/
static col_t can_make_connect4_now(game_t* game) {
    for (col_t col = 0; col < ROW_LENGTH; col++) {
        if (play_auto_without_update(game, col) == 1) return col;
    }
    return -1;
}


/**
 * Annalyses whether the latest player to have played threatens to make a Connect4 during their next turn.
 * 
 * @param game The game status.
 * 
 * @returns If the latest player threatens to make a Connect4 during their next turn, returns the column in [0, ROW_LENGTH[
 * which would allow them to do so. If there are multiple threats, returns the first one detected.
 * -1 if no threats are detected. In case of memory allocation errors,
 * false negatives may occur.
*/
static col_t does_latest_player_threaten_to_connect4(game_t* game) {
    // To make sure the intermediary turn does not affect the possible Connect4, we make sure that the column of
    // the intermadiary turn and the tested column do not collide. 
    uint8_t nb_valid_iterations = 0;

    for (col_t move1 = 0; move1 < ROW_LENGTH && nb_valid_iterations < 2; move1++) {
        game_t* after_move1 = copy(game);
        if (after_move1 == NULL) continue;
        if (play_auto(after_move1, move1) != 0) {
            game_destroy(after_move1);
            continue; 
        }
        // valid iteration
        nb_valid_iterations++;
        for (col_t move2 = 0; move2 < ROW_LENGTH; move2++) {
            if (move1 == move2) continue;    // first move and second move's columns collide
            if (play_auto_without_update(after_move1, move2) == 1) {
                game_destroy(after_move1);
                return move2;    // move2 threatens a Connect4 from the latest player
            }
        }
        game_destroy(after_move1);
    }

    return -1;
}


// ============= MCTS STEPS ============ 


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
    if (n != 0 && N != 0) {    // usual case
        // the currently player will always try to maximise THEIR win ratio, not the AI's
        // If now_playing != PLAYING_AS, that means the parent of node represents the turn of the AI.
        // Therefore, 'node' should have a great score if it maximises the w/n ratio.
        double ratio = (now_playing(node->state) != PLAYING_AS) ? w/n : 1-w/n;
        return ratio + sqrt(2*log(N)/n);
    }
    else return 0.0;    // empty MTCS tree or leaf with error during first simulation (during the node creation)
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
    double max_UCB = -0.1;
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

    player_t w = winner(selected_old_leaf->state);
    if (w == PLAYING_AS) {    // case selected node is a win for the ai
        incr_wins= 7;
        incr_visits = 7;
    } else if (w == 1-PLAYING_AS) {    // case selected node is a win for the human
        incr_visits = 7;
    } else {
        // Computing the total increments to backpropagate
        for (col_t col = 0; col < ROW_LENGTH; col++) {
            if (selected_old_leaf->children[col] != NULL) {
                incr_visits += selected_old_leaf->children[col]->nb_visits;
                incr_wins += selected_old_leaf->children[col]->nb_wins;    // supposedly 0 or 1 if only 1 simulation when creating node
            }
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
 * If no simulations were made for that game playout, creates a new node accordingly and sets it as the tree_root.
 * WARNING : If the move represented by selected_col is invalid, the new tree_root will be set to NULL.
 * 
 * @param selected_col the move that has been played in [0, 6]. Is assumed to be valid
*/
static void progress_in_tree(col_t selected_col) {
    /* 
    This next section fetches simulations results from the branches not chosen 
    and uses them to increment selected_col's simulations data.
    For example, if selected_col == C, 
    then the simulations data of "tree_root -> Y -> X -> C" (for any X, Y, C are all different)
    can be used into the simulations data of "tree_root -> C -> X -> Y"
    (different moves order, same grid state at the end).
    That way, we reduce the number of new simulations to compute starting from "tree_root -> C"
    */
    col_t c = selected_col;    // For shorter notations in this section
    for (col_t y = 0; y < ROW_LENGTH; y++) {
        if (y == c || tree_root->children[y] == NULL) continue;
        for (col_t x = 0; x < ROW_LENGTH; x++) {

            // Ignore Y-X pair if invalid Y-X-C trio or if no data for "tree_root -> Y -> X -> C"

            if (    x == y 
                    || x == c 
                    || tree_root->children[y]->children[x] == NULL
                    || tree_root->children[y]->children[x]->children[c] == NULL
            ) continue;

            uint32_t merged_nb_wins = tree_root->children[y]->children[x]->children[c]->nb_wins;
            uint32_t merged_nb_visits = tree_root->children[y]->children[x]->children[c]->nb_visits;

            // If no data yet for the C-X-Y trio, try creating the appropriate nodes. Ignore C-X-Y trio if it fails
            node_t* prnt = tree_root;    // parent node
            uint8_t does_node_cxy_exist = 1;    // boolean to ensure that the node "tree_root -> C -> X -> Y" exists 
            col_t chldrn[3] = {c, x, y};    // the indices of the sub-children to consider (in order) to reach "tree_root -> C -> X -> Y"
            for (uint8_t i = 0; i < 3; i++) {
                col_t idx = chldrn[i];    // the index of the child to create (if it does not already exist)
                if (prnt->children[idx] != NULL) {
                    // Node already exists
                    prnt = prnt->children[idx];
                    continue;    
                }

                // Attempt to create the new child node
                node_t* child = create_node_and_simulate(play_copy_auto(prnt->state, idx), prnt);
                if (child == NULL) {    
                    // Node creation failed
                    does_node_cxy_exist = 0;
                    break;
                }
                prnt->children[idx] = child;

                // Backpropagation of the data of the new child
                for (node_t* n = prnt; n != NULL; n = n->parent) {
                    n->nb_visits += child->nb_visits;
                    n->nb_wins += child->nb_wins;
                }
                prnt = prnt->children[idx];
            }
            if (!does_node_cxy_exist) continue;    // Failed to create the node "tree_root -> C -> X -> Y"

            /* Finally, adds the simulations data of "tree_root -> Y -> X -> C" to the data of "tree_root -> C -> X -> Y"
            and backpropagates them */
            nb_recombined_visits += merged_nb_visits;
            for (node_t* node = tree_root->children[c]->children[x]->children[y]; node != NULL; node = node->parent) {
                node->nb_visits += merged_nb_visits;
                node->nb_wins += merged_nb_wins;
            }
            
        }
    }

    // Actually rogressing into the tree
    node_t* selected_node = tree_root->children[selected_col];
    if (selected_node == NULL) selected_node = create_node_and_simulate(play_copy_auto(tree_root->state, selected_col), NULL);
    tree_root->children[selected_col] = NULL;
    recursive_node_destroy(tree_root);
    selected_node->parent = NULL;
    tree_root = selected_node;
}


/**
 * Fills the global variable tree_root using the MCTS algorithm. Then, selects the best estimated move, adapts tree_root
 * to take notice of that selection, and returns the selected move. Assumes it is the AI's turn to play.
 * 
 * @returns the column in [0-6] selected by the MCTS algorithm to play. Running this function adds to to the tree,
 * but does NOT update tree_root according to the returned column.
 * MCTS_FAIL if the MCTS algorithm applicaiton fails (extreme error)
*/
static col_t MCTS() {
    // Runs the algorithm
    uint32_t loops = 0;    // there to prevent infinite loops when the selected node won't change or in case of draw
    while (tree_root->nb_visits < MAX_VISITS-7 && loops < MAX_VISITS) {
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
    
    printf("<<<<< %d visits of root node before progression >>>>>\n", tree_root->nb_visits);   // DEBUG DEBUG DEBUG
    return selected_col;
}


/*
===========================================
==================== API ==================
===========================================
*/


col_t init_MCTS(player_t playing_as, uint32_t max_visits) {
    if (max_visits < 8 || (playing_as != PLAYER_A && playing_as != PLAYER_B)) return ARG_ERROR;
    game_t* init_game = game_init();
    if (init_game == NULL) return MEMERROR;
    PLAYING_AS = playing_as;
    MAX_VISITS = max_visits;

    tree_root = create_node_and_simulate(init_game, NULL);
    if (tree_root == NULL) {
        free(init_game);
        return MEMERROR;
    }

    // Initialising the first possible paths
    for (col_t col = 0; col < ROW_LENGTH; col++) {
        game_t* game_continuation = play_copy_auto(init_game, col);
        if (game_continuation == NULL) {    // memory alloc error, because invalid move is supposed impossible here
            recursive_node_destroy(tree_root);
        }
        node_t* new_child = create_node_and_simulate(game_continuation, tree_root);
        if (new_child != NULL) {
            tree_root->children[col] = new_child;
            tree_root->nb_visits += new_child->nb_visits;
            tree_root->nb_wins += new_child->nb_wins;
        }
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
    nb_recombined_visits = 0;
    progress_in_tree(col);

    col_t col_to_play;

    // If the AI can make a Connect4 in the immediate state -> exploit it
    col_to_play = can_make_connect4_now(tree_root->state);
    if (col_to_play >= 0) {
        progress_in_tree(col_to_play);
        MCTS();
        return col_to_play;
    }
    // If the human is threatening to make a connect4, and the AI absolutely needs to avert it
    col_to_play = does_latest_player_threaten_to_connect4(tree_root->state);
    if (col_to_play >= 0) {
        progress_in_tree(col_to_play);
        MCTS();
        return col_to_play;
    }

    // Else, the AI is not forced to play any column, so it runs the MCTS to decide its next turn.
    col_to_play = MCTS();
    progress_in_tree(col_to_play);
    return col_to_play;
}


void print_state() {
    print_game(tree_root->state);
    printf("=> Confidence : %.1f %% (%d simulations, including %d merged)\n", 
            100.0*(double) tree_root->nb_wins/(double) tree_root->nb_visits, 
            tree_root->nb_visits, 
            nb_recombined_visits);
}