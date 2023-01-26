#include <stdio.h>
#include "../headers/game_manager.h"

int main(int argc, char* argv[]) {

    /*printf("[YO]\n");
    int a;
    scanf("%d", &a);
    scanf("%d", &a);
    printf("Result : %d\n", a);
    return;*/

    const uint64_t BIT_ONE = (uint64_t) 0b1;
    const uint64_t TURN_BIT = BIT_ONE<<31;
    const uint64_t WIN_BIT = BIT_ONE<<63;

    printf("%ld\n%ld\n%ld\n", BIT_ONE, TURN_BIT, WIN_BIT);


    game_t* game = game_init(PLAYER_A);
    int32_t player = 0;
    int32_t col = -1;
    printf("[Start okay]\n\n");
    print_game(game);
    printf("Input pair : ");
    scanf("%d", &col);
    while((player == PLAYER_A || player == PLAYER_B) && (col >= 0 && col < ROW_LENGTH)) {
        int8_t res = play(game, (int8_t) player, (int8_t) col);
        printf("Played %d %d resulted in [%d]\n", player, col, res);
        print_game(game);
        printf("Input move : ");
        scanf("%d", &col);
        player = 1 - player;
    }
    game_destroy(game);
}