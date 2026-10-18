#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 64

int main(void) {
    char *line = NULL;
    size_t cap = 0;

    while (1) {
        printf("mysh> ");
        fflush(stdout);

        // 1行読む。Ctrl-Dで終了
        if (getline(&line, &cap, stdin) == -1)
            break;

        // 空白で分割して argv を作る
        char *argv[MAX_ARGS];
        int argc = 0;
        for (char *tok = strtok(line, " \t\n"); tok && argc < MAX_ARGS - 1;
             tok = strtok(NULL, " \t\n")) {
            argv[argc++] = tok;
        }
        argv[argc] = NULL;
        if (argc == 0)
            continue;

        // 組み込みコマンド exit
        if (strcmp(argv[0], "exit") == 0)
            break;

        if (strcmp(argv[0], "cd") == 0) {
            const char *dir;
            if (argc > 1) {
                dir = argv[1];
            } else {
                dir = getenv("HOME");
            }
            if (chdir(dir) != 0)
                perror("cd");
            continue;
        }

        char *outfile = NULL;
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], ">") == 0) {
                outfile = argv[i + 1];
                argv[i] = NULL; // ">" 以降を argv から削除
                break;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            // 子：コマンドに化け
            if (outfile) {
                int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("open");
                    exit(1);
                }
                dup2(fd, 1);
                close(fd);
            }

            execvp(argv[0], argv);
            perror("exec"); // ここに来たら失敗
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
