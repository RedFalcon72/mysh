mysh: src/main.c
	gcc -Wall -Wextra -g -o mysh src/main.c

clean:
	rm -f mysh
