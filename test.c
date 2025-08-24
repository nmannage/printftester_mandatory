// test_printf.c - Clean ft_printf tester for mandatory requirements
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include "ft_printf.h"

// ANSI color codes for pretty output
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define RESET   "\033[0m"
#define BOLD    "\033[1m"

// Test tracking
static int test_number = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int tests_crashed = 0;
static int show_hints = 0;

// Simple structure to hold test results
typedef struct {
    int     return_value;
    char    output[4096];
    int     output_len;
    int     crashed;
    char    crash_reason[256];
} result_t;

typedef int (*printf_func)(const char*, ...);

// Safe execution wrapper using fork
static result_t safe_capture(printf_func func, const char *func_name,
                             const char *format, void *arg1, void *arg2, void *arg3) {
    result_t result = {0};

    int output_pipe[2];
    int status_pipe[2];

    if (pipe(output_pipe) == -1 || pipe(status_pipe) == -1) {
        result.crashed = 1;
        snprintf(result.crash_reason, sizeof(result.crash_reason), "Pipe creation failed");
        return result;
    }

    pid_t pid = fork();

    if (pid == -1) {
        result.crashed = 1;
        snprintf(result.crash_reason, sizeof(result.crash_reason), "Fork failed");
        return result;
    }

    if (pid == 0) {
        // Child process
        close(output_pipe[0]);
        close(status_pipe[0]);

        dup2(output_pipe[1], STDOUT_FILENO);

        int ret;
        if (arg1 == NULL && arg2 == NULL && arg3 == NULL) {
            ret = func(format);
        } else if (arg2 == NULL && arg3 == NULL) {
            ret = func(format, arg1);
        } else if (arg3 == NULL) {
            ret = func(format, arg1, arg2);
        } else {
            ret = func(format, arg1, arg2, arg3);
        }

        fflush(stdout);
        write(status_pipe[1], &ret, sizeof(ret));
        exit(0);
    }

    // Parent process
    close(output_pipe[1]);
    close(status_pipe[1]);

    int status;
    pid_t wait_result = waitpid(pid, &status, 0);

    if (wait_result == -1) {
        result.crashed = 1;
        snprintf(result.crash_reason, sizeof(result.crash_reason), "Wait failed");
    } else if (WIFSIGNALED(status)) {
        result.crashed = 1;
        int sig = WTERMSIG(status);
        const char *sig_name = "Unknown signal";

        switch (sig) {
            case SIGSEGV: sig_name = "Segmentation fault (SIGSEGV)"; break;
            case SIGBUS:  sig_name = "Bus error (SIGBUS)"; break;
            case SIGABRT: sig_name = "Abort (SIGABRT)"; break;
            case SIGFPE:  sig_name = "Floating point exception (SIGFPE)"; break;
            case SIGILL:  sig_name = "Illegal instruction (SIGILL)"; break;
        }

        snprintf(result.crash_reason, sizeof(result.crash_reason),
                "%s in %s", sig_name, func_name);
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        read(status_pipe[0], &result.return_value, sizeof(result.return_value));
        result.output_len = read(output_pipe[0], result.output, sizeof(result.output) - 1);
        if (result.output_len < 0) result.output_len = 0;
        result.output[result.output_len] = '\0';
    } else {
        result.crashed = 1;
        snprintf(result.crash_reason, sizeof(result.crash_reason),
                "Abnormal exit (code %d)", WEXITSTATUS(status));
    }

    close(output_pipe[0]);
    close(status_pipe[0]);

    return result;
}

// Helper to print bytes in hex
static void print_hex(const char *data, int len) {
    printf("  Hex: ");
    for (int i = 0; i < len && i < 20; i++) {
        printf("%02X ", (unsigned char)data[i]);
    }
    if (len > 20) printf("...");
    printf("\n");
}

// Generate debug snippet for failed tests
static void generate_debug_snippet(const char *format, void *arg1, void *arg2, void *arg3) {
    printf(CYAN "\n  === Debug Snippet (save as debug.c) ===\n" RESET);
    printf("  #include <stdio.h>\n");
    printf("  #include <limits.h>\n");
    printf("  #include \"ft_printf.h\"\n");
    printf("  \n");
    printf("  int main(void) {\n");
    printf("      int ret1, ret2;\n");
    printf("      \n");
    printf("      printf(\"Standard printf:\\n\");\n");
    printf("      ret1 = printf(\"");

    // Escape format string
    for (const char *p = format; *p; p++) {
        if (*p == '"') printf("\\\"");
        else if (*p == '\\') printf("\\\\");
        else if (*p == '\n') printf("\\n");
        else if (*p == '\t') printf("\\t");
        else printf("%c", *p);
    }
    printf("\"");

    // Add arguments based on format specifiers
    const char *p = format;
    void *args[] = {arg1, arg2, arg3};
    int arg_idx = 0;

    while (*p && arg_idx < 3) {
        if (*p == '%' && *(p+1) && *(p+1) != '%') {
            p++;
            if (args[arg_idx] != NULL || strchr("cdiuxX", *p)) {
                switch (*p) {
                    case 'c':
                        printf(", '%c'", (char)(long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 's':
                        if (args[arg_idx] == NULL)
                            printf(", NULL");
                        else
                            printf(", \"%s\"", (char*)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'd':
                    case 'i':
                        printf(", %ld", (long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'u':
                        printf(", %lu", (unsigned long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'x':
                    case 'X':
                        printf(", 0x%lX", (unsigned long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'p':
                        if (args[arg_idx] == NULL)
                            printf(", NULL");
                        else
                            printf(", (void*)0x%lX", (unsigned long)args[arg_idx]);
                        arg_idx++;
                        break;
                }
            }
        }
        p++;
    }

    printf(");\n");
    printf("      printf(\"\\nReturn: %%d\\n\\n\", ret1);\n");
    printf("      \n");
    printf("      printf(\"Your ft_printf:\\n\");\n");
    printf("      ret2 = ft_printf(\"");

    // Repeat for ft_printf
    for (const char *p2 = format; *p2; p2++) {
        if (*p2 == '"') printf("\\\"");
        else if (*p2 == '\\') printf("\\\\");
        else if (*p2 == '\n') printf("\\n");
        else if (*p2 == '\t') printf("\\t");
        else printf("%c", *p2);
    }
    printf("\"");

    // Same arguments
    p = format;
    arg_idx = 0;
    while (*p && arg_idx < 3) {
        if (*p == '%' && *(p+1) && *(p+1) != '%') {
            p++;
            if (args[arg_idx] != NULL || strchr("cdiuxX", *p)) {
                switch (*p) {
                    case 'c':
                        printf(", '%c'", (char)(long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 's':
                        if (args[arg_idx] == NULL)
                            printf(", NULL");
                        else
                            printf(", \"%s\"", (char*)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'd':
                    case 'i':
                        printf(", %ld", (long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'u':
                        printf(", %lu", (unsigned long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'x':
                    case 'X':
                        printf(", 0x%lX", (unsigned long)args[arg_idx]);
                        arg_idx++;
                        break;
                    case 'p':
                        if (args[arg_idx] == NULL)
                            printf(", NULL");
                        else
                            printf(", (void*)0x%lX", (unsigned long)args[arg_idx]);
                        arg_idx++;
                        break;
                }
            }
        }
        p++;
    }

    printf(");\n");
    printf("      printf(\"\\nReturn: %%d\\n\", ret2);\n");
    printf("      \n");
    printf("      return 0;\n");
    printf("  }\n");
    printf(CYAN "  === End of snippet ===\n" RESET);
	printf("  Compile with: cc debug.c libftprintf.a -o debug && ./debug\n");
}

// Main test function
static void run_test(const char *test_name, const char *format,
                     void *arg1, void *arg2, void *arg3) {
    test_number++;

    printf(BOLD "\nTest %d: %s\n" RESET, test_number, test_name);
    printf("  Format: \"%s\"\n", format);

    // Capture outputs
    result_t std_result = safe_capture((printf_func)printf, "printf", format, arg1, arg2, arg3);
    result_t ft_result = safe_capture(ft_printf, "ft_printf", format, arg1, arg2, arg3);

    // Check for crashes
    if (ft_result.crashed) {
        printf(MAGENTA "  ⚠️  CRASHED: %s" RESET "\n", ft_result.crash_reason);

        if (!std_result.crashed) {
            printf("  Expected output: \"%s\"\n", std_result.output);
        }

        if (show_hints) {
            printf(YELLOW "  Hint: Check NULL handling and format parsing\n" RESET);
        }

        generate_debug_snippet(format, arg1, arg2, arg3);
        tests_crashed++;
        tests_failed++;
        return;
    }

    // Compare results
    int return_match = (std_result.return_value == ft_result.return_value);
    int output_match = (std_result.output_len == ft_result.output_len) &&
                      (memcmp(std_result.output, ft_result.output, std_result.output_len) == 0);

    if (return_match && output_match) {
        printf(GREEN "  ✓ PASSED" RESET "\n");
        tests_passed++;
    } else {
        printf(RED "  ✗ FAILED" RESET "\n");
        tests_failed++;

        if (!return_match) {
            printf(YELLOW "  Return values differ:\n" RESET);
            printf("    printf:    %d\n", std_result.return_value);
            printf("    ft_printf: %d\n", ft_result.return_value);
        }

        if (!output_match) {
            printf(YELLOW "  Output differs:\n" RESET);
            printf("    printf:    \"%s\" (len=%d)\n", std_result.output, std_result.output_len);
            printf("    ft_printf: \"%s\" (len=%d)\n", ft_result.output, ft_result.output_len);

            if (std_result.output_len > 0 || ft_result.output_len > 0) {
                print_hex(std_result.output, std_result.output_len);
                print_hex(ft_result.output, ft_result.output_len);
            }
        }

        generate_debug_snippet(format, arg1, arg2, arg3);
    }
}

// Test macros
#define TEST(name, fmt) \
    run_test(name, fmt, NULL, NULL, NULL)

#define TEST1(name, fmt, a) \
    run_test(name, fmt, (void*)(long)(a), NULL, NULL)

#define TEST2(name, fmt, a, b) \
    run_test(name, fmt, (void*)(long)(a), (void*)(long)(b), NULL)

#define TEST3(name, fmt, a, b, c) \
    run_test(name, fmt, (void*)(long)(a), (void*)(long)(b), (void*)(long)(c))

// ====== TEST SUITES ======

static void test_char(void) {
    printf("\n" BLUE "====== CHARACTER (%%c) ======" RESET "\n");

    TEST1("%%c basic", "%c", 'A');
    TEST1("%%c space", "%c", ' ');
    TEST1("%%c null byte", "%c", '\0');
    TEST1("%%c newline", "%c", '\n');
    TEST1("%%c with text", "char: %c!", 'X');
    TEST2("%%c multiple", "%c%c", 'A', 'B');
    TEST3("%%c mixed", "a%cb%cc", '1', '2', '3');
}

static void test_string(void) {
    printf("\n" BLUE "====== STRING (%%s) ======" RESET "\n");

    TEST1("%%s basic", "%s", "Hello");
    TEST1("%%s empty", "%s", "");
    TEST1("%%s with spaces", "%s", "Hello World");
    TEST1("%%s with newline", "%s", "Line1\nLine2");
    TEST2("%%s multiple", "%s %s", "Hello", "World");
    TEST1("%%s in brackets", "[%s]", "test");
}

static void test_pointer(void) {
    printf("\n" BLUE "====== POINTER (%%p) ======" RESET "\n");

    TEST1("%%p NULL", "%p", NULL);
    TEST1("%%p zero", "%p", (void*)0);
    TEST1("%%p small", "%p", (void*)0x42);
    TEST1("%%p large", "%p", (void*)0xDEADBEEF);
    TEST1("%%p actual address", "%p", &test_number);
    TEST2("%%p mixed NULL", "%p %p", NULL, (void*)0x123);
    TEST1("%%p max", "%p", (void*)ULONG_MAX);
}

static void test_decimal(void) {
    printf("\n" BLUE "====== DECIMAL (%%d) ======" RESET "\n");

    TEST1("%%d zero", "%d", 0);
    TEST1("%%d positive", "%d", 42);
    TEST1("%%d negative", "%d", -42);
    TEST1("%%d INT_MAX", "%d", INT_MAX);
    TEST1("%%d INT_MIN", "%d", INT_MIN);
    TEST1("%%d minus one", "%d", -1);
    TEST2("%%d multiple", "%d %d", 123, -456);
    TEST3("%%d three nums", "%d, %d, %d", 1, 2, 3);
}

static void test_integer(void) {
    printf("\n" BLUE "====== INTEGER (%%i) ======" RESET "\n");

    TEST1("%%i zero", "%i", 0);
    TEST1("%%i positive", "%i", 42);
    TEST1("%%i negative", "%i", -42);
    TEST1("%%i INT_MAX", "%i", INT_MAX);
    TEST1("%%i INT_MIN", "%i", INT_MIN);
    TEST2("%%i multiple", "%i %i", 100, -200);
}

static void test_unsigned(void) {
    printf("\n" BLUE "====== UNSIGNED (%%u) ======" RESET "\n");

    TEST1("%%u zero", "%u", 0);
    TEST1("%%u small", "%u", 42);
    TEST1("%%u INT_MAX", "%u", INT_MAX);
    TEST1("%%u INT_MIN", "%u", INT_MIN);
    TEST1("%%u UINT_MAX", "%u", UINT_MAX);
    TEST1("%%u negative as unsigned", "%u", -1);
    TEST1("%%u LONG_MAX", "%u", LONG_MAX);
    TEST1("%%u LONG_MIN", "%u", LONG_MIN);
    TEST1("%%u ULONG_MAX", "%u", ULONG_MAX);
    TEST2("%%u multiple", "%u %u", 0, UINT_MAX);
}

static void test_hex_lower(void) {
    printf("\n" BLUE "====== HEX LOWERCASE (%%x) ======" RESET "\n");

    TEST1("%%x zero", "%x", 0);
    TEST1("%%x small", "%x", 15);
    TEST1("%%x 255", "%x", 255);
    TEST1("%%x large", "%x", 0xDEADBEEF);
    TEST1("%%x UINT_MAX", "%x", UINT_MAX);
    TEST1("%%x negative", "%x", -1);
    TEST1("%%x negative small", "%x", -10);
    TEST1("%%x INT_MIN", "%x", INT_MIN);
    TEST1("%%x LONG_MIN", "%x", LONG_MIN);
    TEST2("%%x multiple", "%x %x", 0xABC, 0xDEF);
}

static void test_hex_upper(void) {
    printf("\n" BLUE "====== HEX UPPERCASE (%%X) ======" RESET "\n");

    TEST1("%%X zero", "%X", 0);
    TEST1("%%X small", "%X", 15);
    TEST1("%%X 255", "%X", 255);
    TEST1("%%X large", "%X", 0xCAFEBABE);
    TEST1("%%X UINT_MAX", "%X", UINT_MAX);
    TEST1("%%X negative", "%X", -1);
    TEST2("%%X multiple", "%X %X", 0x123, 0x456);
}

static void test_percent(void) {
    printf("\n" BLUE "====== PERCENT (%%%%) ======" RESET "\n");

    TEST("%%%% single", "%%");
    TEST("%%%% double", "%%%%");
    TEST("%%%% triple", "%%%%%%");
    TEST("%%%% in text", "100%% complete");
    TEST1("%%%% with format", "%d%%", 50);
    TEST2("%%%% multiple formats", "%d%% of %d", 75, 100);
}

static void test_mixed(void) {
    printf("\n" BLUE "====== MIXED FORMATS ======" RESET "\n");

    TEST2("string and int", "Hello %s, number = %d", "World", 42);
    TEST3("all basic types", "%c %s %d", 'A', "test", 123);
    TEST2("hex and pointer", "hex=%x ptr=%p", 0xABC, (void*)0xDEF);
    TEST3("int variations", "%d %i %u", -42, 42, 42);
    TEST3("hex variations", "%d in hex: %x %X", 255, 255, 255);
    TEST1("percent and string", "Loading %s: 50%%", "file.txt");
    TEST3("complex mix", "[%c] num=%d str=\"%s\"", 'X', -999, "hello");
}

static void test_edge_cases(void) {
    printf("\n" BLUE "====== EDGE CASES ======" RESET "\n");

    TEST("empty format", "");
    TEST("only text", "Hello World!");
    TEST("spaces only", "   ");
    TEST("newlines", "\n\n\n");
    TEST("tabs", "\t\t\t");
    TEST1("null char in string", "Before%cAfter", '\0');
    TEST("long text", "This is a very long string to test buffer handling in printf implementation");

    // Format edge cases
    TEST1("no space between", "abc%ddef", 123);
    TEST3("consecutive formats", "%d%d%d", 1, 2, 3);
    TEST3("no spacing", "%c%s%d", 'A', "B", 1);
}

static void test_special_values(void) {
    printf("\n" BLUE "====== SPECIAL VALUES ======" RESET "\n");

    // Testing with all max values
    TEST3("all max", "%d %u %x", INT_MAX, UINT_MAX, UINT_MAX);

    // Testing with boundary values
    TEST2("int boundaries", "%d %d", INT_MIN, INT_MAX);
    TEST2("unsigned boundaries", "%u %u", 0, UINT_MAX);

    // Mix of special values
    TEST3("mixed special", "%d %u %x", -1, -1, -1);
}

static void print_summary(void) {
    printf("\n" BOLD "========================================\n");
    printf("           TEST SUMMARY                \n");
    printf("========================================" RESET "\n");
    printf("  Total tests: %d\n", test_number);
    printf(GREEN "  Passed: %d\n" RESET, tests_passed);

    if (tests_crashed > 0) {
        printf(MAGENTA "  Crashed: %d\n" RESET, tests_crashed);
    }

    if (tests_failed - tests_crashed > 0) {
        printf(RED "  Failed (no crash): %d\n" RESET, tests_failed - tests_crashed);
    }

    printf("========================================\n");

    if (tests_failed == 0) {
        printf(GREEN BOLD "\n  🎉 ALL TESTS PASSED! 🎉\n\n" RESET);
    } else {
        printf(RED BOLD "\n  ⚠️  SOME TESTS FAILED ⚠️\n" RESET);

        if (show_hints) {
            printf(YELLOW "\n  Common issues to check:\n" RESET);
            printf("  • NULL handling: %%s should print \"(null)\"\n");
            printf("  • NULL pointer: %%p should print \"(nil)\" or \"0x0\"\n");
            printf("  • Return value must equal number of chars printed\n");
            printf("  • %%p format usually needs '0x' prefix\n");
            printf("  • Negative numbers with %%u and %%x\n");
        } else {
            printf("\n  Run with --hints for debugging suggestions\n");
        }

        printf("\n  Use the generated debug snippets to test specific cases!\n\n");
    }
}

int main(int argc, char **argv) {
    // Check for flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hints") == 0 || strcmp(argv[i], "-h") == 0) {
            show_hints = 1;
        }
    }

	printf(BOLD "\n======================================== __  4   __      \n");
	printf("  FT_PRINTF TESTER 2025                 ( _\\    /_ )     \n");
	printf("         nmannage                        \\ _\\  /_ /      \n");
	printf("========================================  \\ _\\/_ /_ _   \n" );
	printf("  Testing mandatory requirements only     |____/_/ /|     \n");
	printf("  Conversions: c s p d i u x X %%         (  (_)__)J-)    \n");
	printf("                                         (  /`.,   /      \n");
	printf("                                          \\/  ;   /        \n");
	printf("========================================    | === |        \n");
    printf(BOLD "\n========================================\n" RESET);

    if (show_hints) {
        printf("  " YELLOW "Hints enabled" RESET "\n");
    } else {
        printf("  Run with --hints for debugging tips\n");
    }
    printf("\n");

    // Run all test suites
    test_char();
    test_string();
    test_pointer();
    test_decimal();
    test_integer();
    test_unsigned();
    test_hex_lower();
    test_hex_upper();
    test_percent();
    test_mixed();
    test_edge_cases();
    test_special_values();

    print_summary();

    return (tests_failed > 0) ? 1 : 0;
}

