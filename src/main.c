#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGS 64

// 標準出力を path のファイルにつなぐ（子の中で使う）
static void redirect_out(const char *path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

int main(void) {
    char *line = NULL;
    size_t cap = 0;
    signal(SIGINT, SIG_IGN);
    while (1) {
        printf("mysh> ");
        fflush(stdout);

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

        // 組み込みコマンド
        if (strcmp(argv[0], "exit") == 0)
            break;

        if (strcmp(argv[0], "cd") == 0) {
            const char *dir = (argc > 1) ? argv[1] : getenv("HOME");
            if (chdir(dir) != 0)
                perror("cd");
            continue;
        }

        // "|" を探して、左右に分ける
        char **right = NULL;
        for (int i = 0; i < argc; i++) {
            if (strcmp(argv[i], "|") == 0) {
                argv[i] = NULL;       // 左の argv をここで終わらせる
                right = &argv[i + 1]; // 右の argv の先頭
                break;
            }
        }
        if (right && right[0] == NULL) {
            fprintf(stderr, "mysh: | の右にコマンドが必要\n");
            continue;
        }

        // ">" を探す（NULL になった "|" の位置は飛ばす）
        char *outfile = NULL;
        int bad = 0;
        for (int i = 0; i < argc; i++) {
            if (argv[i] && strcmp(argv[i], ">") == 0) {
                if (argv[i + 1] == NULL) {
                    bad = 1;
                    break;
                }
                outfile = argv[i + 1];
                argv[i] = NULL;
                break;
            }
        }
        if (bad) {
            fprintf(stderr, "mysh: > の後にファイル名が必要\n");
            continue;
        }

        if (right) {
            // ---- パイプあり：左 | 右 ----
            int fd[2];
            if (pipe(fd) < 0) {
                perror("pipe");
                continue;
            }

            pid_t p1 = fork();
            if (p1 == 0) { // 左：stdout を管の書く口へ
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
                signal(SIGINT, SIG_DFL);
                execvp(argv[0], argv);
                perror("exec");
                exit(1);
            }

            pid_t p2 = fork();
            if (p2 == 0) { // 右：stdin を管の読む口へ
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
                close(fd[1]);
                if (outfile)
                    redirect_out(outfile); // > は右（最後）のコマンドに効く
                signal(SIGINT, SIG_DFL);
                execvp(right[0], right);
                perror("exec");
                exit(1);
            }

            // 親も両端を閉じる（忘れると右が終わらず固まる）
            close(fd[0]);
            close(fd[1]);
            waitpid(p1, NULL, 0);
            waitpid(p2, NULL, 0);
        } else {
            // ---- パイプなし：今まで通り ----
            pid_t pid = fork();
            if (pid == 0) {
                if (outfile)
                    redirect_out(outfile);
                signal(SIGINT, SIG_DFL);
                execvp(argv[0], argv);
                perror("exec");
                exit(1);
            } else if (pid > 0) {
                waitpid(pid, NULL, 0);
            } else {
                perror("fork");
            }
        }
    }

    free(line);
    return 0;
}
