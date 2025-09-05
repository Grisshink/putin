#include <stdio.h>
#include <stdbool.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define ball(...) do { \
        printf(__VA_ARGS__); \
        abort(); \
    } while (0)

#define ARRLEN(arr) (sizeof(arr)/sizeof(arr[0]))

ma_engine audio;
ma_sound sound;

char* running_filepath;
bool is_running = true;

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

void print_current_time(void) {
    if (!running_filepath || !ma_sound_is_playing(&sound)) {
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

    printf("] - %s", running_filepath);
    if (ma_sound_is_looping(&sound)) printf(" loop");
    printf("\n");
}

void process_commands(char* command) {
    char* args = cut_and_get_next_word(command);

    if (command[0] == 'c') {
        print_current_time();
        return;
    } else if (command[0] == 's') {
        if (!ma_sound_is_playing(&sound)) ma_sound_start(&sound);

        ma_sound_seek_to_second(&sound, atof(args));
        print_current_time();
        return;
    } else if (command[0] == 'l') {
        bool loop = !ma_sound_is_looping(&sound);
        ma_sound_set_looping(&sound, loop);
        printf("loop %s\n", loop ? "on" : "off");
        return;
    } else if (command[0] == 'p') {
        if (args[0] == '\0') {
            printf("pitch %.3f%%\n", ma_sound_get_pitch(&sound) * 100.0f);
            return;
        }

        float p = atof(args);
        if (p <= 0.0f || p > 300.0) {
            printf("invalid percent\n");
            return;
        }
        ma_sound_set_pitch(&sound, p / 100.0f);
        printf("pitch %.3f%%\n", p);
        return;
    } else if (command[0] == 'v') {
        if (args[0] == '\0') {
            printf("volume %.3f%%\n", ma_sound_get_volume(&sound) * 100.0f);
            return;
        }

        float v = atof(args);
        if (v < 0.0f || v > 100.0f) {
            printf("invalid percent\n");
            return;
        }
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
    running_filepath = argv[1];

    if (ma_engine_init(NULL, &audio)) ball("failed to initialize audio engine.");

    if (ma_sound_init_from_file(&audio, running_filepath, MA_SOUND_FLAG_STREAM, NULL, NULL, &sound)) {
        ball("cant load file");
    }
    
    ma_sound_start(&sound);
    print_current_time();

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
