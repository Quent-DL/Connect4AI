#include <stdio.h>
#include "../headers/game_manager.h"

int main(int argc, char* argv[]) {

    game_t* game = game_init(PLAYER_A);
    int32_t player = 0;
    int32_t col = -1;
    printf("[Start okay]\n\n");
    print_game(game);
    printf("Input pair : ");
    scanf("%d", &col);
    while((player == PLAYER_A || player == PLAYER_B) && (col >= 0 && col < ROW_LENGTH)) {
        int8_t res = play(game, (player_t) player, (col_t) col);
        printf("Played %d %d resulted in [%d]\n", player, col, res);
        print_game(game);
        printf("Input move : ");
        scanf("%d", &col);
        player = 1 - player;
    }
    game_destroy(game);
}