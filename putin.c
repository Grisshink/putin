#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <libgen.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "miniaudio.h"

#define ball(...) do { \
        printf(__VA_ARGS__); \
        abort(); \
    } while (0)

#define ARRLEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define PATH_LEN 512
#define TASK_LIST_LEN 512
#define SUB(text) "\n\033[90m -- " text "\033[0m\n"

typedef int (*TaskFunc)(int fd);

typedef struct {
    int fd;
    TaskFunc execute_task;
    bool delete;
} Task;

ma_engine audio;
ma_sound sound;

char running_filepath[PATH_LEN] = {0};
bool is_running = true;
bool loop = false;
float pitch = 100.0f;
float volume = 100.0f;

Task task_list[TASK_LIST_LEN];
int task_list_len = 0;

bool new_task(int fd, TaskFunc task_func) {
    if (task_list_len >= TASK_LIST_LEN) {
        printf("Can't create new task: Max task limit reached\n" SUB("WTF"));
        return false;
    }
    task_list[task_list_len++] = (Task) {
        .fd = fd,
        .execute_task = task_func,
        .delete = false,
    };
    return true;
}

Task* get_task(int fd) {
    for (int i = 0; i < task_list_len; i++) {
        if (task_list[i].fd != fd) continue;
        return &task_list[i];
    }
    return NULL;
}

void delete_task(int fd) {
    Task* t = get_task(fd);
    if (!t) return;
    t->delete = true;
}

char* cut_and_get_next_word(char* inp) {
    if (!*inp) return inp;
    while (*inp != ' ' && *inp != '\n' && *inp != '\0') inp++;
    if (*inp == '\0') return inp;
    *inp = '\0';
    inp++;

    while (*inp == ' ' || *inp == '\n') inp++;
    return inp;
}

void print_time(float t, FILE* f) {
    fprintf(f, "%02d:%02d", (int)fmodf(t / 60.0f, 60.0f), (int)fmodf(t, 60.0f));
}

void print_status(FILE* f) {
    if (!ma_sound_is_playing(&sound)) {
        fprintf(f, "stopped\n");
        return;
    }

    fprintf(f, "[");

    float t = 0.0f;
    ma_sound_get_cursor_in_seconds(&sound, &t);
    print_time(t, f);

    fprintf(f, "/");

    ma_sound_get_length_in_seconds(&sound, &t);
    print_time(t, f);

    fprintf(f, "] - %s", *running_filepath != '\0' ? running_filepath : "unnamed");
    if (ma_sound_is_looping(&sound)) fprintf(f, " loop");
    fprintf(f, "\n");
}

void process_commands(char* command, FILE* f) {
    char* args = cut_and_get_next_word(command);

    if (!strcmp(command, "status")) {
        print_status(f);
        return;
    } else if (!strcmp(command, "time")) {
        float cur = 0.0f, len = 0.0f;
        ma_sound_get_cursor_in_seconds(&sound, &cur);
        ma_sound_get_length_in_seconds(&sound, &len);
        fprintf(f, "%.3f\n%.3f\n", cur, len);
        return;
    } else if (!strcmp(command, "seek")) {
        if (!ma_sound_is_playing(&sound)) ma_sound_start(&sound);

        float pos = atof(args);
        float len = 0.0f;
        ma_sound_get_length_in_seconds(&sound, &len);
        if (pos < 0.0f || pos > len) {
            fprintf(f, "invalid time\n");
            return;
        }
        ma_sound_seek_to_second(&sound, pos);
        print_status(f);
        return;
    } else if (!strcmp(command, "loop")) {
        loop = !ma_sound_is_looping(&sound);
        ma_sound_set_looping(&sound, loop);
        fprintf(f, "loop %s\n", loop ? "on" : "off");
        return;
    } else if (!strcmp(command, "play")) {
        if (args[0] == '\0') {
            fprintf(f, "usage: play <music_file_path>\n");
            return;
        }

        ma_sound_uninit(&sound);
        *running_filepath = '\0';

        char* argpos = args;
        while (*argpos != '\0' && *argpos != '\n') argpos++;
        *argpos = '\0';

        if (ma_sound_init_from_file(&audio, args, MA_SOUND_FLAG_STREAM, NULL, NULL, &sound)) {
            fprintf(f, "cant load file \"%s\"\n", args);
            print_status(f);
            return;
        }
        ma_sound_set_looping(&sound, loop);
        ma_sound_set_volume(&sound, volume / 100.0f);
        ma_sound_set_pitch(&sound, pitch / 100.0f);
        strncpy(running_filepath, basename(args), PATH_LEN);

        ma_sound_start(&sound);
        print_status(f);

        return;
    } else if (!strcmp(command, "pitch")) {
        if (args[0] == '\0') {
            fprintf(f, "pitch %.3f%%\n", pitch);
            return;
        }

        float p = atof(args);
        if (p <= 0.0f || p > 300.0) {
            fprintf(f, "invalid percent\n");
            return;
        }
        pitch = p;
        ma_sound_set_pitch(&sound, p / 100.0f);
        fprintf(f, "pitch %.3f%%\n", p);
        return;
    } else if (!strcmp(command, "pause")) {
        if (!ma_sound_is_playing(&sound)) {
            ma_sound_start(&sound);
        } else {
            ma_sound_stop(&sound);
        }
        print_status(f);
        return;
    } else if (!strcmp(command, "volume")) {
        if (args[0] == '\0') {
            fprintf(f, "volume %.3f%%\n", volume);
            return;
        }

        float v = atof(args);
        if (v < 0.0f || v > 100.0f) {
            fprintf(f, "invalid percent\n");
            return;
        }
        volume = v;
        ma_sound_set_volume(&sound, v / 100.0f);
        fprintf(f, "volume %.3f%%\n", v);
        return;
    } else if (!strcmp(command, "quit")) {
        fprintf(f, "Exiting...\n");
        printf("Exit command received, exiting...\n");
        is_running = false;
        return;
    }

    fprintf(f, "invalid command: %s\n", command);
}

int serve_client(int client) {
    char inp_buf[256];

    int len = read(client, inp_buf, sizeof(inp_buf) - 1);
    if (len == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        printf("Cannot read socket: %s\n" SUB("Reading is forbidden by PKN"), strerror(errno));
        delete_task(client);
        return 1;
    }
    if (len == 0) {
        delete_task(client);
        return 1;
    }
    inp_buf[len] = '\0';

    FILE* f = fdopen(dup(client), "w");
    char* command = inp_buf;
    while (*command == ' ' || *command == '\n') command++;
    process_commands(command, f);
    fclose(f);

    return 1;
}

int accept_connection(int sock) {
    int client = accept(sock, NULL, NULL);
    if (client == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;
        printf("Cannot accept connection: %s\n" SUB("Unacceptable >:("), strerror(errno));
        return 1;
    }
    
    if (fcntl(client, F_SETFL, O_NONBLOCK) == -1) {
        printf("Failed to set file descriptor flags: %s\n" SUB("This was a bad idea"), strerror(errno));
        close(client);
        return 1;
    }

    if (!new_task(client, serve_client)) return -1;

    return 0;
}

void serve_stdin(void) {
    char inp_buf[256];

    while (is_running) {
        fgets(inp_buf, ARRLEN(inp_buf), stdin);
        char* command = inp_buf;
        while (*command == ' ' || *command == '\n') command++;
        process_commands(command, stdout);
    }
}

void serve_socket(void) {
    int sock = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock == -1) {
        printf("Cannot create socket: %s\n" SUB("Your GNU is not unix"), strerror(errno));
        return;
    }

    const char* sock_path = "./putin.sock";
    unlink(sock_path);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path));
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Cannot bind socket: %s\n" SUB("Not bing - bind!"), strerror(errno));
        return;
    }

    if (listen(sock, 10) == -1) {
        printf("Cannot listen on socket: %s\n" SUB("I can't hear, i'm a DELTARUNE fan!"), strerror(errno));
        return;
    }
    printf("Listening on socket: %s\n", sock_path);

    new_task(sock, accept_connection);

    fd_set fds;

    while (is_running) {
        int max_fd = 0;
        FD_ZERO(&fds);
        for (int i = 0; i < task_list_len; i++) {
            if (task_list[i].fd > max_fd) max_fd = task_list[i].fd;
            FD_SET(task_list[i].fd, &fds);
        }

        if (select(max_fd + 1, &fds, NULL, NULL, NULL) == -1) {
            printf("Failed to select: %s\n", strerror(errno));
            break;
        }
        
        for (int i = 0; i < task_list_len; i++) {
            if (!FD_ISSET(task_list[i].fd, &fds)) continue;
            int ret;
            while ((ret = task_list[i].execute_task(task_list[i].fd)) == 0);
            if (ret == -1) goto loop_end;
        }

        for (int i = task_list_len - 1; i >= 0; i--) {
            if (!task_list[i].delete) continue;
            close(task_list[i].fd);
            memmove(&task_list[i], &task_list[i + 1], (task_list_len - i - 1) * sizeof(Task));
            task_list_len--;
        }
    }
    loop_end:

    for (int i = 0; i < task_list_len; i++) close(task_list[i].fd);
    task_list_len = 0;
}

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("usage: %s <music_file_path>\n", argv[0]);
        return 1;
    }

    if (ma_engine_init(NULL, &audio)) ball("failed to initialize audio engine.\n" SUB("I think your audio is dead"));

    if (ma_sound_init_from_file(&audio, argv[1], MA_SOUND_FLAG_STREAM, NULL, NULL, &sound)) {
        printf("cant load file %s\n" SUB("Can't even load files in this country"), argv[1]);
    } else {
        ma_sound_start(&sound);
        strncpy(running_filepath, basename(argv[1]), PATH_LEN);
    }
    
    print_status(stdout);
    serve_socket();

    ma_sound_uninit(&sound);
    ma_engine_uninit(&audio);
    return 0;
}
