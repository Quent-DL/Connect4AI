#include <stdio.h>
#include "../headers/mcts.h"


/**
 * Properly frees all the memory used in the program.
 * 
 * @param game the game to destroy
 * @param w the player who won the game
*/
static void terminate_game(game_t* game, player_t w) {
    print_game(game);
    game_destroy(game);
    destroy_MCTS();
    exit(0);
}


/**
 * Asks for input from user and updates the game accordingly.
 * Terminates the program if the game end with that move.
 * 
 * @param game the current state of the game for which the user makes their choice. It is updated if the chosen move is valid.
 * 
 * @returns the column in [0, ROW_LENGTH[ that the human player chose for their turn.
 * Terminates the program if the chosen move results in a win for the user.
*/
static col_t human_turn(game_t* game) {
    int32_t scanned_int;
    col_t chosen_col;
    int8_t move_res;
    do {
        printf("\n>>>Input human move : ");
        scanf("%d", &scanned_int);
        chosen_col = (col_t) scanned_int;
        move_res = play_auto(game, chosen_col);
        if (move_res == 1) terminate_game(game, PLAYER_A);
        else if (move_res == 2) terminate_game(game, DRAW);
    } while (move_res == -2 || move_res == ARG_ERROR);    // in case of repeated bad input from the human player
    return chosen_col;
}


/**
 * Runs the MCTS algorithm to decide what move the AI should play, and updates the game accordingly.
 * Terminates the program if the game ends with that move.
 * 
 * @param game the current state of the game. It is updated according to the move decided by the MCTS algorithm.
 * It is assumed that the game has already been updated to include the user's latest move.
 * @param the latest column chosen by the user.
*/
static void ai_turn(game_t* game, col_t chosen_col) {
    col_t ai_col = input_MCTS(chosen_col);
    if (ai_col < 0) exit(-1);
    int8_t move_res = play_auto(game, ai_col);
    if (move_res == 1) terminate_game(game, PLAYER_B);
    else if (move_res == 2) terminate_game(game, DRAW);
}


int main(int argc, char* argv[]) {

    if (argc < 2 || argc > 3) exit(-1);
    uint32_t max_visits = atoi(argv[1]);
    player_t ai_plays_as = (argc == 3 && atoi(argv[2])) ? PLAYER_B : PLAYER_A;

    game_t* game = game_init();
    if (game == NULL) exit(-1);

    col_t ia_first_move = init_MCTS(ai_plays_as, max_visits);
    if (ia_first_move == MEMERROR || ia_first_move == ARG_ERROR) {
        game_destroy(game);
        exit(-1);
    }
    if (ai_plays_as == PLAYER_A) play_auto(game, ia_first_move);
    col_t chosen_human_col;

    while(winner(game) != DRAW) {
        print_state();
        chosen_human_col = human_turn(game);
        ai_turn(game, chosen_human_col);
    }
}