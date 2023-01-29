#ifndef MCTS_H
#define MCTS_H


#include <stdlib.h>
#include <math.h>
#include "./game_manager.h"


#define MEMERROR INT8_MIN
#define MCTS_FAIL -2


typedef struct mcts_node {
    game_t* state;
    uint32_t nb_wins;
    uint32_t nb_visits;
    struct mcts_node* parent;
    struct mcts_node* children[ROW_LENGTH];
} node_t;


/**
 * Initialises the MCTS algorithm. Doesn't run the MCTS algorithm yet.
 * 
 * @param playing_as the role played by the AI in a Connect4 game. Must be PLAYER_A or PLAYER_B
 * @param max_iter the maximum number of iterations by the MCTS algorithm before returning a result. The higher, the stronger the AI.
 * 
 * @returns ROW_LENGTH if playing_as == PLAYER_B;
 * A number in [0,ROW_LENGTH[ that represents the column which the AI decides to play in if playing_as == PLAYER_A (i.e. if the AI starts the game);
 * ARG_ERROR if the arguments passed are invalid.
 * -1 if a memory error occurs
*/
col_t init_MCTS(player_t playing_as, uint32_t max_iter);


void destroy_MCTS();


/**
 * Accepts an input move from the player, and returns the move played by the AI in return and decided using the MCTS algorithm.
 * 
 * @param col the column the player decides to play in. Must be in [0, 6]
 * 
 * @returns the column in [0, 6] the AI chose to make its move if everything goes well;
 * ARG_ERROR if the argments are invalid;
 * -1 if the move played by the live player is invalid (column full, ...)
 * MCTS_FAIL in case of MTCS_FAILING (no move could be computed due to an exception or an critical lack of memory)
*/
col_t input_MCTS(col_t col);


/**
 * Prints the current state of the game.
*/
void print_state();


#endif /* MCTS_H */