#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H


#include <stdint.h>


#define PLAYER_A 0
#define PLAYER_B 1
#define ARG_ERROR -64
#define ROW_LENGTH 7
#define COL_HEIGHT 6


typedef int8_t player_t;
typedef int8_t col_t;
typedef int64_t grid_t;
typedef int64_t boolean;

/**
 * Bit 62 : 1 if it's the player's turn
 * Bit 61 : 1 if the player has won the game
 * 
 * Bits 41 -> 35 : last row (highest)
 * ...
 * Bits 6 -> 0 : first row (lowest)
*/
typedef struct game {
    grid_t gridA;
    grid_t gridB;
    col_t cols_occupation[ROW_LENGTH];
} game_t;


/**
 * Creates a game of Connect4
 * 
 * @param starting_player the first player to start. Must be PLAYER_A or PLAYER_B.
 * 
 * @returns a pointer to the initialised game;
 * NULL if the memory allocation for the game unsuccesful or if the arguments are invalid
*/
game_t* game_init(player_t starting_player);


/**
 * Deletes a game of Connect4 and frees all the associated pointers
*/
void game_destroy(game_t* game);


/**
 * Copies a game state.
 * 
 * @param game the game to copy
 * 
 * @return a copy of the game;
 * NULL in case of memory allocation exception or if 'game' is NULL.
*/
game_t* copy(game_t* game);


/**
 * Returns the winner of the game.
 * 
 * @param game the game to check
 * 
 * @returns PLAYER_A if player A has won;
 * PLAYER_B if player B has won;
 * -1 if the game is not finished yet (no player has won yet);
 * ARG_ERROR if the arguments are invalid
*/
player_t winner(game_t* game);


/**
 * Makes a move for a game of Connect4 and applies the result to the given game.
 * 
 * @param game the current state of the game to which apply the move and (if successful) the results
 * @param player the player who attempts to play the move. Must be PLAYER_A or PLAYER_B
 * @param col the column in which the player attempts to add a disk. Must be contained in [0, 6].
 * 
 * @returns 1 if the move is valid and results in a win for 'player', and 'game' is updated to implement this move;
 * 0 if the move is valid, and 'game' is updated to implement this move;
 * -1 if it is not 'player''s turn to play;
 * -2 if the targetted column is full;
 * -3 if the game is already finished;
 * ARG_ERROR if the arguments are invalid
*/
int8_t play(game_t* game, player_t player, col_t col);


/**
 * Just like 'play', but the move is automatically played by the player whose turn it is. 
*/
int8_t play_auto(game_t* game, col_t col);


/**
 * Makes a move for a game of Connect4. The result of the move is applied onto a new game, and the one passed as an argument is left untouched.
 * 
 * @param game the current state of game
 * @param player the player who attempts to play the move. Must be PLAYER_A or PLAYER_B
 * @param col the column in which the player attempts to add a disk
 * 
 * @returns The new (updated) game if the move is valid (even if it ends the game), and 'game' is left untouched;
 * NULL if any kind of error occurs (e.g. if the move is invalid or if the memory allocation for the new game is unsuccesful)
*/
game_t* play_copy(game_t* game, player_t player, col_t col);


/**
 * Just like 'play_copy', but the move is automatically played by the player whose turn it is.
*/
game_t* play_copy_auto(game_t* game, col_t col);


void print_game(game_t* game);


// HELPER FUNCTIONS :
//static uint64_t* get_player_grid(game_t* game, int8_t player);
//static uint64_t is_their_turn_to_play(game_t* game, uint64_t player_grid);
//static int8_t compute_offset(int8_t col, int8_t row);
//static char makes_new_connect4(game_t* game, int8_t col, uint64_t player_grid);


#endif /* GAME_MANAGER_H */