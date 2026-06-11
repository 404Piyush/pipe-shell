/* tests/test_shell.c — parser + executor tests
 *
 * A small zero-dependency test framework is provided in
 * tests/test_util.h.  We split the test cases into:
 *   - parser-only tests (no fork)
 *   - executor tests (do fork)
 */
#include "shell.h"
#include "test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/* ============================================================
 *                       PARSER TESTS
 * ============================================================ */

static void test_parse_simple(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls -la", &c));
    ASSERT_EQ_SIZE(c.n_stages, 1);
    ASSERT_TRUE(strcmp(c.stages[0].argv[0], "ls") == 0);
    ASSERT_TRUE(strcmp(c.stages[0].argv[1], "-la") == 0);
    ASSERT_TRUE(c.stages[0].argv[2] == NULL);
    ASSERT_NULL(c.stages[0].infile);
    ASSERT_NULL(c.stages[0].outfile);
}

static void test_parse_redir_in(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("wc -l < file.txt", &c));
    ASSERT_EQ_SIZE(c.n_stages, 1);
    ASSERT_TRUE(strcmp(c.stages[0].argv[0], "wc") == 0);
    ASSERT_TRUE(strcmp(c.stages[0].argv[1], "-l") == 0);
    ASSERT_TRUE(strcmp(c.stages[0].infile, "file.txt") == 0);
}

static void test_parse_redir_out_truncate(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls > out.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].outfile, "out.txt") == 0);
    ASSERT_TRUE(c.stages[0].append == false);
}

static void test_parse_redir_out_append(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls >> out.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].outfile, "out.txt") == 0);
    ASSERT_TRUE(c.stages[0].append == true);
}

static void test_parse_pipeline(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls /usr/include | wc -l", &c));
    ASSERT_EQ_SIZE(c.n_stages, 2);
    ASSERT_TRUE(strcmp(c.stages[0].argv[0], "ls") == 0);
    ASSERT_TRUE(strcmp(c.stages[0].argv[1], "/usr/include") == 0);
    ASSERT_TRUE(strcmp(c.stages[1].argv[0], "wc") == 0);
    ASSERT_TRUE(strcmp(c.stages[1].argv[1], "-l") == 0);
}

static void test_parse_pipeline_with_redir(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls /etc | grep hosts > out.txt", &c));
    ASSERT_EQ_SIZE(c.n_stages, 2);
    ASSERT_NULL(c.stages[0].outfile);
    ASSERT_TRUE(strcmp(c.stages[1].outfile, "out.txt") == 0);
    ASSERT_TRUE(c.stages[1].append == false);
}

static void test_parse_background(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("sleep 5 &", &c));
    ASSERT_TRUE(c.background == true);
    ASSERT_EQ_SIZE(c.n_stages, 1);
    ASSERT_TRUE(strcmp(c.stages[0].argv[0], "sleep") == 0);
    ASSERT_TRUE(strcmp(c.stages[0].argv[1], "5") == 0);
}

static void test_parse_errors(void) {
    shell_cmd c;
    ASSERT_TRUE(!shell_parse("", &c));
    ASSERT_TRUE(!shell_parse("|", &c));
    ASSERT_TRUE(!shell_parse("ls |", &c));
    ASSERT_TRUE(!shell_parse("| ls", &c));
}

/* ============================================================
 *                       EXECUTOR TESTS
 * ============================================================ */

/* Run `line` through the shell, return the exit status.  Used
   by the executor tests below. */
static int exec_via_shell(const char *line) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* child: re-exec the test binary with --run "<line>" */
        execl("./pipe-shell", "pipe-shell", "--run", line, (char *)NULL);
        _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void test_exec_true(void) {
    ASSERT_EQ_INT(exec_via_shell("true"), 0);
}

static void test_exec_false(void) {
    ASSERT_EQ_INT(exec_via_shell("false"), 1);
}

static void test_exec_redir_in(void) {
    /* printf-style: write 5 lines to a file, then wc -l < file -> 5 */
    FILE *f = fopen("test_in.txt", "w");
    ASSERT_NOT_NULL(f);
    fputs("a\nb\nc\nd\ne\n", f);
    fclose(f);
    ASSERT_EQ_INT(exec_via_shell("wc -l < test_in.txt"), 0);
    unlink("test_in.txt");
}

static void test_exec_pipeline(void) {
    /* printf 'a\nb\nc\n' | wc -l  -> wc exits 0 */
    ASSERT_EQ_INT(exec_via_shell("printf 'a\\nb\\nc\\n' | wc -l"), 0);
}

static void test_exec_redir_out(void) {
    /* echo hi > test_out.txt; cat test_out.txt -> "hi" */
    int rc = exec_via_shell("echo hi > test_out.txt");
    ASSERT_EQ_INT(rc, 0);
    FILE *f = fopen("test_out.txt", "r");
    ASSERT_NOT_NULL(f);
    char buf[16] = {0};
    fgets(buf, sizeof buf, f);
    fclose(f);
    ASSERT_TRUE(strcmp(buf, "hi\n") == 0);
    unlink("test_out.txt");
}

static void test_exec_redir_append(void) {
    /* echo a > f; echo b >> f; cat f -> "a\nb\n" */
    int rc = exec_via_shell("echo a > test_app.txt");
    ASSERT_EQ_INT(rc, 0);
    rc = exec_via_shell("echo b >> test_app.txt");
    ASSERT_EQ_INT(rc, 0);
    FILE *f = fopen("test_app.txt", "r");
    ASSERT_NOT_NULL(f);
    char buf[32] = {0};
    size_t got = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    ASSERT_EQ_SIZE(got, 4);
    ASSERT_TRUE(strcmp(buf, "a\nb\n") == 0);
    unlink("test_app.txt");
}

static void test_exec_pipeline_with_redir(void) {
    /* echo a | cat > test_p.txt; cat test_p.txt -> "a\n" */
    int rc = exec_via_shell("echo a | cat > test_p.txt");
    ASSERT_EQ_INT(rc, 0);
    FILE *f = fopen("test_p.txt", "r");
    ASSERT_NOT_NULL(f);
    char buf[16] = {0};
    fgets(buf, sizeof buf, f);
    fclose(f);
    ASSERT_TRUE(strcmp(buf, "a\n") == 0);
    unlink("test_p.txt");
}

static void test_exec_cd_builtin(void) {
    /* `cd /tmp` should succeed (exit 0). */
    pid_t pid = fork();
    if (pid == 0) {
        execl("./pipe-shell", "pipe-shell", "--run", "cd /tmp", (char *)NULL);
        _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    ASSERT_EQ_INT(WEXITSTATUS(st), 0);

    /* `cd /no/such/dir` should fail (exit 1). */
    pid = fork();
    if (pid == 0) {
        execl("./pipe-shell", "pipe-shell", "--run",
              "cd /no/such/dir/please/ignore", (char *)NULL);
        _exit(127);
    }
    waitpid(pid, &st, 0);
    ASSERT_EQ_INT(WEXITSTATUS(st), 1);
}

/* ============================================================
 *                       MAIN
 * ============================================================ */

int main(void) {
    fprintf(stderr, "=== parser tests ===\n");
    RUN(test_parse_simple);
    RUN(test_parse_redir_in);
    RUN(test_parse_redir_out_truncate);
    RUN(test_parse_redir_out_append);
    RUN(test_parse_pipeline);
    RUN(test_parse_pipeline_with_redir);
    RUN(test_parse_background);
    RUN(test_parse_errors);

    fprintf(stderr, "\n=== executor tests ===\n");
    /* Re-exec the binary with --run for executor tests.  We
       need the binary to be built first; the Makefile wires
       this up via `make test`. */
    if (access("./pipe-shell", X_OK) == 0) {
        RUN(test_exec_true);
        RUN(test_exec_false);
        RUN(test_exec_redir_in);
        RUN(test_exec_pipeline);
        RUN(test_exec_redir_out);
        RUN(test_exec_redir_append);
        RUN(test_exec_pipeline_with_redir);
        RUN(test_exec_cd_builtin);
    } else {
        fprintf(stderr, "(skipping executor tests; build pipe-shell first)\n");
    }

    TEST_SUMMARY();
    return g_fail > 0 ? 1 : 0;
}
