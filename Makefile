main:
	gcc -Wall -Werror -g -o out src/interactive_mcts.c src/mcts.c src/game_manager.c -lm

vs:
	gcc -Wall -Werror -g -o out src/terminal_interactive_game.c src/mcts.c src/game_manager.c -lm
run:
	./out 200000