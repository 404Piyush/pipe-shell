/* tests/test_shell.c — v2 tests
 *
 * Same as v1's test suite, plus 4 new cases for stderr
 * redirection and multi-redirect-per-stage.
 */
#include "shell.h"
#include "test_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ---------- parser tests ---------- */

static void test_parse_simple(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls -la", &c));
    ASSERT_EQ_SIZE(c.n_stages, 1);
    ASSERT_TRUE(strcmp(c.stages[0].argv[0], "ls") == 0);
    ASSERT_NULL(c.stages[0].infile);
    ASSERT_NULL(c.stages[0].outfile);
    ASSERT_NULL(c.stages[0].errfile);
}

static void test_parse_redir_in(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("wc -l < file.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].infile, "file.txt") == 0);
    ASSERT_NULL(c.stages[0].outfile);
    ASSERT_NULL(c.stages[0].errfile);
}

static void test_parse_redir_out_truncate(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls > out.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].outfile, "out.txt") == 0);
    ASSERT_TRUE(c.stages[0].out_append == false);
}

static void test_parse_redir_out_append(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls >> out.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].outfile, "out.txt") == 0);
    ASSERT_TRUE(c.stages[0].out_append == true);
}

static void test_parse_redir_err_truncate(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls /no 2> err.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].errfile, "err.txt") == 0);
    ASSERT_TRUE(c.stages[0].err_append == false);
    ASSERT_NULL(c.stages[0].outfile);
}

static void test_parse_redir_err_append(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls /no 2>> err.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].errfile, "err.txt") == 0);
    ASSERT_TRUE(c.stages[0].err_append == true);
}

static void test_parse_multi_redirect(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("cmd < in.txt > out.txt 2> err.txt", &c));
    ASSERT_TRUE(strcmp(c.stages[0].infile,  "in.txt")  == 0);
    ASSERT_TRUE(strcmp(c.stages[0].outfile, "out.txt") == 0);
    ASSERT_TRUE(c.stages[0].out_append == false);
    ASSERT_TRUE(strcmp(c.stages[0].errfile, "err.txt") == 0);
    ASSERT_TRUE(c.stages[0].err_append == false);
}

static void test_parse_pipeline(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls /usr/include | grep stdio | wc -l", &c));
    ASSERT_EQ_SIZE(c.n_stages, 3);
}

static void test_parse_pipeline_with_redir(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("ls /etc | grep hosts > out.txt 2> err.txt", &c));
    ASSERT_EQ_SIZE(c.n_stages, 2);
    ASSERT_NULL(c.stages[0].outfile);
    ASSERT_NULL(c.stages[0].errfile);
    ASSERT_TRUE(strcmp(c.stages[1].outfile, "out.txt") == 0);
    ASSERT_TRUE(strcmp(c.stages[1].errfile, "err.txt") == 0);
}

static void test_parse_background(void) {
    shell_cmd c;
    ASSERT_TRUE(shell_parse("sleep 5 &", &c));
    ASSERT_TRUE(c.background == true);
}

static void test_parse_errors(void) {
    shell_cmd c;
    ASSERT_TRUE(!shell_parse("", &c));
    ASSERT_TRUE(!shell_parse("|", &c));
    ASSERT_TRUE(!shell_parse("ls |", &c));
    ASSERT_TRUE(!shell_parse("| ls", &c));
    /* bare redirect with no command is also a parse error */
    ASSERT_TRUE(!shell_parse("> out.txt", &c));
    ASSERT_TRUE(!shell_parse("2> err.txt", &c));
}

/* ---------- executor tests ---------- */

static int exec_via_shell(const char *line) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execl("./pipe-shell", "mini-shell", "--run", line, (char *)NULL);
        _exit(127);
    }
    int st;
    waitpid(pid, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

static void test_exec_true(void)  { ASSERT_EQ_INT(exec_via_shell("true"), 0); }
static void test_exec_false(void) { ASSERT_EQ_INT(exec_via_shell("false"), 1); }

static void test_exec_redir_in(void) {
    FILE *f = fopen("test_in.txt", "w");
    ASSERT_NOT_NULL(f);
    fputs("a\nb\nc\nd\ne\n", f);
    fclose(f);
    ASSERT_EQ_INT(exec_via_shell("wc -l < test_in.txt"), 0);
    unlink("test_in.txt");
}

static void test_exec_redir_out(void) {
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

/* NEW for v2: stderr redirect. */
static void test_exec_redir_err(void) {
    /* `ls /no/such/dir` writes to stderr, exits 1. */
    int rc = exec_via_shell("ls /no/such/dir 2> test_err.txt");
    ASSERT_EQ_INT(rc, 1);
    FILE *f = fopen("test_err.txt", "r");
    ASSERT_NOT_NULL(f);
    char buf[256] = {0};
    size_t got = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    ASSERT_TRUE(got > 0);
    ASSERT_TRUE(strstr(buf, "No such file") != NULL
             || strstr(buf, "No such directory") != NULL
             || strstr(buf, "cannot access") != NULL);
    unlink("test_err.txt");
}

/* NEW for v2: stdout and stderr to different files. */
static void test_exec_redir_out_and_err(void) {
    int rc = exec_via_shell("ls /tmp /no/such 1> test_o.txt 2> test_e.txt");
    /* ls may exit 1 or 2 depending on whether /tmp exists;
       we only care that the redirects worked. */
    (void)rc;
    struct stat st_o, st_e;
    ASSERT_EQ_INT(stat("test_o.txt", &st_o), 0);
    ASSERT_EQ_INT(stat("test_e.txt", &st_e), 0);
    ASSERT_TRUE(st_o.st_size > 0);   /* /tmp listing */
    ASSERT_TRUE(st_e.st_size > 0);   /* /no/such error  */
    unlink("test_o.txt");
    unlink("test_e.txt");
}

/* NEW for v2: 2>> appends. */
static void test_exec_redir_err_append(void) {
    exec_via_shell("ls /no 2> test_e2.txt");
    exec_via_shell("ls /also/no 2>> test_e2.txt");
    FILE *f = fopen("test_e2.txt", "r");
    ASSERT_NOT_NULL(f);
    char buf[512] = {0};
    fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    /* both "No such file" lines should be present */
    int count = 0;
    for (char *p = buf; (p = strstr(p, "No such")) != NULL; p += 8) count++;
    ASSERT_TRUE(count >= 2);
    unlink("test_e2.txt");
}

/* ---------- main ---------- */

int main(void) {
    fprintf(stderr, "=== parser tests ===\n");
    RUN(test_parse_simple);
    RUN(test_parse_redir_in);
    RUN(test_parse_redir_out_truncate);
    RUN(test_parse_redir_out_append);
    RUN(test_parse_redir_err_truncate);
    RUN(test_parse_redir_err_append);
    RUN(test_parse_multi_redirect);
    RUN(test_parse_pipeline);
    RUN(test_parse_pipeline_with_redir);
    RUN(test_parse_background);
    RUN(test_parse_errors);

    fprintf(stderr, "\n=== executor tests ===\n");
    if (access("./pipe-shell", X_OK) == 0) {
        RUN(test_exec_true);
        RUN(test_exec_false);
        RUN(test_exec_redir_in);
        RUN(test_exec_redir_out);
        RUN(test_exec_redir_err);
        RUN(test_exec_redir_out_and_err);
        RUN(test_exec_redir_err_append);
    } else {
        fprintf(stderr, "(skipping executor tests; build mini-shell first)\n");
    }

    TEST_SUMMARY();
    return g_fail > 0 ? 1 : 0;
}
