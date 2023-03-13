#include <stdlib.h>
#include <stdio.h>
#include "../headers/game_manager.h"

const grid_t BIT_ONE = (grid_t) 0b1;
const grid_t TURN_BIT = BIT_ONE<<62;
const grid_t WIN_BIT = BIT_ONE<<61;

/*
===========================================
============= HELPER FUNCTIONS ============
===========================================
*/

/**
 * Returns the grid associated to a player in a game.
 * 
 * @param game the game. Is assumed to be non-null.
 * @param player the player we want the grid of. Is assumed to be equal to PLAYER_A or PLAYER_B.
 * 
 * @return the grid in 'game' that contains the information relative to 'player'
*/
static grid_t* get_player_grid(game_t* game, player_t player) {
    return (player == PLAYER_A) ? &(game->gridA) : &(game->gridB);
}


static boolean is_their_turn_to_play(game_t* game, grid_t player_grid) {
    return (player_grid & TURN_BIT)>>TURN_BIT;
}


/**
 * Computes the offset of the bit that corresponds to the coordinates (col, row) in the grid
 * 
 * @param col Index of the column. Contained in [0, 6]
 * @param row Index of the row. Contained in [0, 5]. 
 */
static int8_t compute_offset(col_t col, int8_t row) {
    return (1+row)*ROW_LENGTH - col - 1;
}


/**
 * Checks if the addition of the latest added disk creates a Connect-4.
 * The arguments are assumed to be non-NULL pointers or valid numbers.
 * 
 * @param game the state of the game. It is assumed that the latest disk has already been added to the grid and cols_occupation has already been updated.
 * @param col the column (0-6) on which the latest disk was played
 * @param player_grid the grid of the player that added the disk. Includes the latest added disk.
 * 
 * @return 1 if the new disk creates a Connect-4; 0 otherwise
*/
static boolean makes_new_connect4(game_t* game, col_t col, grid_t player_grid) {
    int8_t row_index = game->cols_occupation[col] - 1;

    // Vertical check
    int8_t consecutive_counter = 0;
    int8_t init_offset = compute_offset(col, row_index);
    for (int8_t offset = init_offset; offset >= 0; offset -= ROW_LENGTH) {
        if ((player_grid & (BIT_ONE<<offset))) consecutive_counter++;
        else break;
    }
    if (consecutive_counter >= 4) return 1;

    //printf("DEBUG : PASSED VERTICAL CHECK\n");

    // Horizontal check
    consecutive_counter = 0;
    init_offset = row_index * ROW_LENGTH;
    for (int8_t offset = init_offset; offset < init_offset+ROW_LENGTH; offset++) {
        if (player_grid & (BIT_ONE<<offset)) consecutive_counter++;
        else consecutive_counter = 0;
        if (consecutive_counter >= 4) return 1;
    }

    //printf("DEBUG : PASSED HORIZONTAL CHECK\n");
    
    // Diagonal up-right check
    consecutive_counter = 0;
    int8_t increment = ROW_LENGTH-1;
    int8_t dist_to_closest_limit = (row_index < col) ? row_index : col;
    col_t curr_col = col - dist_to_closest_limit;
    init_offset = compute_offset(col, row_index) - increment * dist_to_closest_limit;
    for (int8_t offset = init_offset; offset < ROW_LENGTH*COL_HEIGHT && curr_col < ROW_LENGTH; offset += increment) {
        if (player_grid & (BIT_ONE<<offset)) consecutive_counter++;
        else consecutive_counter = 0;
        if (consecutive_counter >= 4) return 1;
        curr_col++;
    }

    //printf("DEBUG : PASSED UPRIGHT CHECK\n");

    // Diagonal up-left check
    consecutive_counter = 0;
    increment = ROW_LENGTH+1;
    dist_to_closest_limit = (row_index < ROW_LENGTH-1-col) ? row_index : ROW_LENGTH-1-col;
    curr_col = col + dist_to_closest_limit;
    init_offset = compute_offset(col, row_index) - increment * dist_to_closest_limit;
    for (int8_t offset = init_offset; offset < ROW_LENGTH*COL_HEIGHT && curr_col >= 0 ; offset += increment) {
        if (player_grid & (BIT_ONE<<offset)) consecutive_counter++;
        else consecutive_counter = 0;
        if (consecutive_counter >= 4) return 1;
        curr_col--;
    }

    //printf("DEBUG : PASSED UPLEFT CHECK\n");

    return 0;
}


/*
===========================================
=================== API ===================
===========================================
*/


game_t* game_init() {
    game_t* g_ptr = (game_t*) malloc(sizeof(game_t));
    if (g_ptr == NULL) return NULL;
    for (col_t col = 0; col < ROW_LENGTH; col++) g_ptr->cols_occupation[col] = 0;
    g_ptr->gridA = TURN_BIT;
    g_ptr->gridB = 0;
    return g_ptr;
}


void game_destroy(game_t* game) { free(game); }


game_t* copy(game_t* game) {
    if (game == NULL) return NULL;
    game_t* new_game = (game_t*) malloc(sizeof(game_t));
    if (new_game == NULL) return NULL;
    new_game->gridA = game->gridA;
    new_game->gridB = game->gridB;
    for (col_t c = 0; c < ROW_LENGTH; c++) new_game->cols_occupation[c] = game->cols_occupation[c];
    return new_game;
}


player_t now_playing(game_t* game) {
    return (game->gridA & TURN_BIT) ? PLAYER_A : PLAYER_B;
}


player_t winner(game_t* game) {
    // Preliminary checks
    if (game == NULL) return ARG_ERROR;

    if (game->gridA & WIN_BIT) return PLAYER_A;
    if (game->gridB & WIN_BIT) return PLAYER_B;
    // The following 'if' statement translates to "if the top row is full"
    grid_t gr = ((game->gridA | game->gridB) >> 35);    // DEBUG
    gr = gr;    // DEBUG
    if ((((game->gridA | game->gridB) >> ROW_LENGTH*(COL_HEIGHT-1)) & 0x7F) == 0x7F) 
        return DRAW;
    return -1;
}


int8_t play(game_t* game, player_t player, col_t col) {
    // Preliminary checks
    if (game == NULL) return ARG_ERROR;
    if (player != PLAYER_A && player != PLAYER_B) return ARG_ERROR;
    if (col < 0 || col >= ROW_LENGTH) return ARG_ERROR;

    if (winner(game) >= 0) return -3;
    if (game->cols_occupation[col] >= COL_HEIGHT) return -2;
    grid_t* this_grid = get_player_grid(game, player);
    if (!is_their_turn_to_play(game, *this_grid)) return -1;

    // The move is valid
    
    // Add move
    grid_t* other_grid = get_player_grid(game, 1-player);
    int8_t offset = compute_offset(col, game->cols_occupation[col]);
    *this_grid = *this_grid | (BIT_ONE << offset);
    *this_grid = *this_grid & ~TURN_BIT;
    *other_grid = *other_grid | TURN_BIT;
    game->cols_occupation[col]++;

    // Check for connect 4
    if (makes_new_connect4(game, col, *this_grid)) {
        *this_grid = *this_grid | WIN_BIT;
        return 1;
    } else if (winner(game) == DRAW) return 2;

    return 0;
}


int8_t play_auto(game_t* game, col_t col) {
    if (is_their_turn_to_play(game, game->gridA)) return play(game, PLAYER_A, col);
    return play(game, PLAYER_B, col);
}


int8_t play_auto_without_update(game_t* game, col_t col) {
    game_t* cg = copy(game);
    if (cg == NULL) return MEM_ERROR;
    int8_t res = play_auto(cg, col);
    game_destroy(cg);
    return res;
}


game_t* play_copy_auto(game_t* game, col_t col) {
    game_t* new_game = copy(game);
    if (new_game == NULL) return NULL;

    int8_t res = play_auto(new_game, col);
    if (res < 0) {
        game_destroy(new_game);
        return NULL;
    }
    return new_game;
}


void print_game(game_t* game) {
    printf("\n[GAME STATE]\n\n");
    printf("0 1 2 3 4 5 6\n");
    for(int8_t r = COL_HEIGHT-1; r >= 0; r--) {
        for (col_t c = 0; c < ROW_LENGTH; c++) {
            //char disk = '_';
            int8_t offset = compute_offset(c, r);
            if (game->gridA & (BIT_ONE<<offset)) printf("● ");
            else if (game->gridB & (BIT_ONE<<offset)) printf("○ ");
            else printf("_ ");
            //printf("%c ", disk);
        }
        printf("\n");
    }
    if (game->gridA & TURN_BIT) printf("\n=== Turn : A (●)\n");
    else if (game->gridB & TURN_BIT) printf("\n=== Turn : B (○)\n");
    else printf("\n=== TURN : ERROR\n");

    player_t w = winner(game);
    char* outcome;
    switch (w) {
        case PLAYER_A:
            outcome = "A has won !!!";
            break;
        case PLAYER_B:
            outcome = "B has won !!!";
            break;
        case DRAW:
            outcome = "The game is a draw !!!";
            break;
        default:
            outcome = "_";
    }
    printf("=== Outcome : %s\n", outcome);

    printf("\n");
}


void debug_print_game(game_t* game) {
    print_game(game);
    printf("%ld\n%ld\n", game->gridA, game->gridB);
    printf("[");
    for (col_t i = 0; i < ROW_LENGTH; i++) printf("%d, ", game->cols_occupation[i]);
    printf("]\n");
}