#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

typedef enum {
    TARGET_LINUX,
    TARGET_WEB,
    COUNT_TARGETS,
} Target;

void usage(FILE *stream, const char *program_name) {
    fprintf(stream, "Usage: %s [OPTIONS]\n", program_name);
    fprintf(stream, "    OPTIONS:\n");
    fprintf(stream, "      -h, --help - Print this help message\n");
    fprintf(stream, "      -r - Run game after building\n");
    static_assert(COUNT_TARGETS == 2, "Please update usage after adding a new target");
    fprintf(stream, "      -t <target> - Build for a specific target. Possible targets include:\n");
    fprintf(stream, "        linux\n");
    fprintf(stream, "        web\n");
    fprintf(stream, "      If this option is not provided, the default target is `linux`\n");
}

void common_cflags(Cmd *cmd) {
    cmd_append(cmd, "-Wall", "-Wextra", "-g");
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    if (!mkdir_if_not_exists("./build/")) return 1;

    const char *program_name = nob_shift(argv, argc);

    bool run = false;
    Target target = TARGET_LINUX;
    while (argc > 0) {
        const char *arg = shift(argv, argc);
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout, program_name);
            return 0;
        } else if (strcmp(arg, "-t") == 0) {
            if (argc == 0) {
                usage(stderr, program_name);
                nob_log(ERROR, "-t flag requires an argument");
            }
            const char *target_name = shift(argv, argc);
            static_assert(COUNT_TARGETS == 2, "Please update the -t flag when adding a new target");
            if (strcmp(target_name, "linux") == 0) {
                target = TARGET_LINUX;
            } else if (strcmp(target_name, "web") == 0) {
                target = TARGET_WEB;
            } else {
                usage(stderr, program_name);
                nob_log(ERROR, "unknown target %s", target_name);
                return 1;
            }
        } else if (strcmp(arg, "-r") == 0) {
            run = true;
        } else {
            usage(stderr, program_name);
            nob_log(ERROR, "unknown flag %s", arg);
            return 1;
        }
    }

    Cmd cmd = {0};
    static_assert(COUNT_TARGETS == 2, "Please update this `switch` statement when adding a new target");
    switch (target) {
        case TARGET_LINUX:
            cmd_append(&cmd, "cc");
            common_cflags(&cmd);
            cmd_append(&cmd, "-o", "./build/main");
            cmd_append(&cmd, "./src/main.c");
            cmd_append(&cmd, "-I.", "-I./raylib/");
            cmd_append(&cmd, "-L./raylib/", "-lraylib", "-lm");
            break;
        case TARGET_WEB:
            cmd_append(&cmd, "emcc");
            common_cflags(&cmd);
            cmd_append(&cmd, "-o", "./build/index.html");
            cmd_append(&cmd, "./src/main.c");
            cmd_append(&cmd, "-I.", "-I./raylib/");
            cmd_append(&cmd, "./raylib/libraylib.web.a");
            cmd_append(&cmd, "-s", "USE_GLFW=3", "-DPLATFORM_WEB", "--shell-file", "raylib/minshell.html");
            break;
        default:
            UNREACHABLE("invalid target");
    }
    if (!cmd_run_sync_and_reset(&cmd)) return 1;

    static_assert(COUNT_TARGETS == 2, "Please update this `switch` statement when adding a new target");
    if (run) {
        switch (target) {
            case TARGET_LINUX:
                cmd_append(&cmd, "./build/main");
                break;
            case TARGET_WEB:
                cmd_append(&cmd, "emrun", "./build/index.html");
                break;
            default:
                UNREACHABLE("invalid target");
        }
        if (!cmd_run_sync_and_reset(&cmd)) return 1;
        return 0;
    }
}
