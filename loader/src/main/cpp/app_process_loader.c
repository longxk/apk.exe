#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"

int get_arg_list_size(char** args) {
    int size = 0;
    while (args[size++] != NULL);
    return size - 1;
}

char** append_arg_list(char** args, char** extras) {
    int old_list_size = get_arg_list_size(args);
    int extra_list_size = get_arg_list_size(extras);
    int new_list_size = old_list_size + extra_list_size + 1;

    char** extended = (char **) malloc(sizeof(char*) * new_list_size);
    for (int i = 0; i < old_list_size; ++i) {
        extended[i] = args[i];
    }
    extended[new_list_size - 1] = NULL;

    int i = 0;
    for (; i < extra_list_size; ++i) {
        extended[old_list_size + i] = extras[i];
    }

    return extended;
}

void log_str_array(char** strs) {
    int i = 0;
    while (strs[i] != NULL) {
        LOGD("%s", strs[i++]);
    }
}

int main(int argc __attribute__((__unused__)), char* argv[], char* envp[]) {
    char* self_path = (char*) malloc(512);
    readlink("/proc/self/exe", self_path, 512);

    char* ld_preload = (char*) malloc(1024);
    strcpy(ld_preload, "LD_PRELOAD=");
    strcat(ld_preload, self_path);

    char* cp = (char *) malloc(1024);
    strcpy(cp, "-Djava.class.path=");
    strcat(cp, self_path);

    char* loader_argv[] = { argv[0], cp, "/data/local/tmp", "Main", NULL };
    char** argv_extended = append_arg_list(loader_argv, argv + 1);
    LOGD("argv extended: ");
    log_str_array(argv_extended);

    char* envp_extras[] = { ld_preload, NULL };
    char** envp_extended = append_arg_list(envp, envp_extras);
    LOGD("envp extended: ");
    log_str_array(envp_extended);

    int exe_ret = -1;
#if defined(__LP64__)
    exe_ret = execvpe("app_process64", argv_extended, envp_extended);
#else
    exe_ret = execvpe("app_process32", argv_extended, envp_extended);
#endif

    LOGE("failed to execute app_process: %d", exe_ret);

    return 0;
}