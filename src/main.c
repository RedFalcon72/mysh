#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 64

int main(void) {
    char *line = NULL;
    size_t cap = 0;

    while (1) {
        printf("mysh> ");
        fflush(stdout);

        // 1行読む。Ctrl-Dで終了
        if (getline(&line, &cap, stdin) == -1) break;

        // 空白で分割して argv を作る
        char *argv[MAX_ARGS];
        int argc = 0;
        for (char *tok = strtok(line, " \t\n"); tok && argc < MAX_ARGS - 1;
             tok = strtok(NULL, " \t\n")) {
            argv[argc++] = tok;
        }
        argv[argc] = NULL;
        if (argc == 0) continue;

        // 組み込みコマンド exit
        if (strcmp(argv[0], "exit") == 0) break;

        pid_t pid = fork();
        if (pid == 0) {
            // 子：コマンドに化ける
            execvp(argv[0], argv);
            perror("exec");   // ここに来たら失敗
            exit(1);
        } else if (pid > 0) {
            // 親：子の終了を待つ
            waitpid(pid, NULL, 0);
        } else {
            perror("fork");
        }
    }

    free(line);
    return 0;
}
