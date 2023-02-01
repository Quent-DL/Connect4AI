#include <stdio.h>
#include "../headers/mcts.h"


static void terminate_game(game_t* game, player_t w) {
    print_game(game);
    printf("--- PLAYER %c WINS ! ---\n", (w == PLAYER_A) ? 'A' : 'B');
    game_destroy(game);
    destroy_MCTS();
    exit(0);
}

int main(int argc, char* argv[]) {

    // HUMAN IS PLAYER_A
    // MCTS IS PLAYER_B

    if (argc != 2) exit(-1);
    uint32_t max_visits = atoi(argv[1]);

    init_MCTS(PLAYER_B, max_visits);
    int32_t scanned = -1;
    col_t latest_col = -1;
    int8_t move_res;

    game_t* game = game_init(PLAYER_A);
    if (game == NULL) return -1;
    print_game(game);

    while(1) {    // TODO add actual loop condition

        // Human turn
        do {
            printf("\n>>>Input human move : ");
            scanf("%d", &scanned);
            latest_col = (col_t) scanned;
            move_res = play_auto(game, latest_col);
            if (move_res == 1) terminate_game(game, PLAYER_A);
        } while (move_res == -2 || move_res == ARG_ERROR);    // in case of repeated bad input from the human player

        // AI turn
        latest_col = input_MCTS(latest_col);
        if (latest_col < 0) exit(-1);
        move_res = play_auto(game, latest_col);
        print_state();
        if (move_res == 1) terminate_game(game, PLAYER_B);
    }
}