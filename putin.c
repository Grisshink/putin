#include <stdio.h>
#include <stdbool.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define ball(...) do { \
        printf(__VA_ARGS__); \
        abort(); \
    } while (0)

#define ARRLEN(arr) (sizeof(arr)/sizeof(arr[0]))
#define PATH_LEN 512

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

void print_time(float t) {
    printf("%02d:%02d", (int)fmodf(t / 60.0f, 60.0f), (int)fmodf(t, 60.0f));
}

void print_status(void) {
    if (!ma_sound_is_playing(&sound)) {
        printf("stopped\n");
        return;
    }

    printf("[");

    float t;
    ma_sound_get_cursor_in_seconds(&sound, &t);
    print_time(t);

    printf("/");

    ma_sound_get_length_in_seconds(&sound, &t);
    print_time(t);

    printf("] - %s", *running_filepath != '\0' ? running_filepath : "unnamed");
    if (ma_sound_is_looping(&sound)) printf(" loop");
    printf("\n");
}

void process_commands(char* command) {
    char* args = cut_and_get_next_word(command);

    if (command[0] == 'c') {
        print_status();
        return;
    } else if (command[0] == 's') {
        if (!ma_sound_is_playing(&sound)) ma_sound_start(&sound);

        ma_sound_seek_to_second(&sound, atof(args));
        print_status();
        return;
    } else if (command[0] == 'l') {
        loop = !ma_sound_is_looping(&sound);
        ma_sound_set_looping(&sound, loop);
        printf("loop %s\n", loop ? "on" : "off");
        return;
    } else if (command[0] == 'o') {
        ma_sound_uninit(&sound);
        *running_filepath = '\0';

        char* argpos = args;
        while (*argpos != '\0' && *argpos != '\n') argpos++;
        *argpos = '\0';

        if (ma_sound_init_from_file(&audio, args, MA_SOUND_FLAG_STREAM, NULL, NULL, &sound)) {
            printf("cant load file \"%s\"\n", args);
            return;
        }
        ma_sound_set_looping(&sound, loop);
        ma_sound_set_volume(&sound, volume / 100.0f);
        ma_sound_set_pitch(&sound, pitch / 100.0f);
        strncpy(running_filepath, args, PATH_LEN);

        ma_sound_start(&sound);
        print_status();

        return;
    } else if (!strcmp(command, "pitch")) {
        if (args[0] == '\0') {
            printf("pitch %.3f%%\n", pitch);
            return;
        }

        float p = atof(args);
        if (p <= 0.0f || p > 300.0) {
            printf("invalid percent\n");
            return;
        }
        pitch = p;
        ma_sound_set_pitch(&sound, p / 100.0f);
        printf("pitch %.3f%%\n", p);
        return;
    } else if (command[0] == 'p') {
        if (!ma_sound_is_playing(&sound)) {
            ma_sound_start(&sound);
        } else {
            ma_sound_stop(&sound);
        }
        print_status();
        return;
    } else if (command[0] == 'v') {
        if (args[0] == '\0') {
            printf("volume %.3f%%\n", volume);
            return;
        }

        float v = atof(args);
        if (v < 0.0f || v > 100.0f) {
            printf("invalid percent\n");
            return;
        }
        volume = v;
        ma_sound_set_volume(&sound, v / 100.0f);
        printf("volume %.3f%%\n", v);
        return;
    } else if (command[0] == 'q') {
        is_running = false;
        return;
    }

    printf("invalid command: %s\n", command);
}

int main(int argc, char** argv) {
    if (argc == 1) {
        printf("usage: %s <music_file_path>\n", argv[0]);
        return 1;
    }

    if (ma_engine_init(NULL, &audio)) ball("failed to initialize audio engine.");

    if (ma_sound_init_from_file(&audio, argv[1], MA_SOUND_FLAG_STREAM, NULL, NULL, &sound)) {
        printf("cant load file %s\n", argv[1]);
    } else {
        ma_sound_start(&sound);
        strncpy(running_filepath, argv[1], PATH_LEN);
    }
    
    print_status();

    char inp_buf[256];
    while (is_running) {
        fgets(inp_buf, ARRLEN(inp_buf), stdin);
        char* command = inp_buf;
        while (*command == ' ' || *command == '\n') command++;
        process_commands(command);
    }

    printf("Exiting...\n");
    ma_sound_uninit(&sound);
    ma_engine_uninit(&audio);
    return 0;
}
