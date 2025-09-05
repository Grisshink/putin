#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define ball(...) do { \
        printf(__VA_ARGS__); \
        abort(); \
    } while (0)

#define ARRLEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define PATH_LEN 512
#define SUB(text) "\n\033[90m -- " text "\033[0m\n"

ma_engine audio;
ma_sound sound;

char running_filepath[PATH_LEN] = {0};
bool is_running = true;
bool loop = false;
float pitch = 100.0f;
float volume = 100.0f;

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

    float t;
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

    if (command[0] == 'c') {
        print_status(f);
        return;
    } else if (command[0] == 't') {
        float cur, len;
        ma_sound_get_cursor_in_seconds(&sound, &cur);
        ma_sound_get_length_in_seconds(&sound, &len);
        fprintf(f, "%.3f\n%.3f\n", cur, len);
        return;
    } else if (command[0] == 's') {
        if (!ma_sound_is_playing(&sound)) ma_sound_start(&sound);

        ma_sound_seek_to_second(&sound, atof(args));
        print_status(f);
        return;
    } else if (command[0] == 'l') {
        loop = !ma_sound_is_looping(&sound);
        ma_sound_set_looping(&sound, loop);
        fprintf(f, "loop %s\n", loop ? "on" : "off");
        return;
    } else if (command[0] == 'o') {
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
        strncpy(running_filepath, args, PATH_LEN);

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
    } else if (command[0] == 'p') {
        if (!ma_sound_is_playing(&sound)) {
            ma_sound_start(&sound);
        } else {
            ma_sound_stop(&sound);
        }
        print_status(f);
        return;
    } else if (command[0] == 'v') {
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
    } else if (command[0] == 'q') {
        fprintf(f, "Exiting...\n");
        is_running = false;
        return;
    }

    fprintf(f, "invalid command: %s\n", command);
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
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
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

    char inp_buf[256];

    while (is_running) {
        int client = accept(sock, NULL, NULL);
        if (client == -1) {
            printf("Cannot read connection: %s\n" SUB("Unacceptable >:("), strerror(errno));
            break;
        }

        int len = read(client, inp_buf, sizeof(inp_buf) - 1);
        if (len == -1) {
            printf("Cannot read socket: %s\n" SUB("Reading is forbidden by PKN"), strerror(errno));
            break;
        }
        inp_buf[len] = '\0';

        FILE* f = fdopen(client, "w");
        char* command = inp_buf;
        while (*command == ' ' || *command == '\n') command++;
        process_commands(command, f);
        fflush(f);

        close(client);
    }

    close(sock);
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
        strncpy(running_filepath, argv[1], PATH_LEN);
    }
    
    print_status(stdout);
    serve_socket();

    ma_sound_uninit(&sound);
    ma_engine_uninit(&audio);
    return 0;
}
