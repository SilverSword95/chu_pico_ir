#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "config.h"
#include "air.h"
#include "slider.h"
#include "save.h"

#define SENSE_LIMIT_MAX 9
#define SENSE_LIMIT_MIN -9

#define MAX_COMMANDS 20
#define MAX_PARAMETERS 5
#define MAX_PARAMETER_LENGTH 20

const char *chu_prompt = "chu_pico>";

typedef void (*cmd_handler_t)(int argc, char *argv[]);

static const char *commands[MAX_COMMANDS];
static const char *helps[MAX_COMMANDS];
static cmd_handler_t handlers[MAX_COMMANDS];
static int max_cmd_len = 0;

static int num_commands = 0;

static void register_command(const char *cmd, cmd_handler_t handler, const char *help)
{
    if (num_commands < MAX_COMMANDS) {
        commands[num_commands] = cmd;
        handlers[num_commands] = handler;
        helps[num_commands] = help;
        num_commands++;
        if (strlen(cmd) > max_cmd_len) {
            max_cmd_len = strlen(cmd);
        }
    }
}

// return -1 if not matched, return -2 if ambiguous
static int match_prefix(const char *str[], int num, const char *prefix)
{
    int match = -1;
    bool found = false;

    for (int i = 0; (i < num) && str[i]; i++) {
        if (strncasecmp(str[i], prefix, strlen(prefix)) == 0) {
            if (found) {
                return -2;
            }
            found = true;
            match = i;
        }
    }

    return match;
}

static void handle_help(int argc, char *argv[])
{
    printf("\n   << Chu Pico Controller >>\n");
    printf(" https://github.com/whowechina\n\n");
    printf("Available commands:\n");
    for (int i = 0; i < num_commands; i++) {
        printf("%*s: %s\n", max_cmd_len + 2, commands[i], helps[i]);
    }
}

static void disp_colors()
{
    printf("[Colors]\n");
    printf("  Key upper: %06x, lower: %06x, both: %06x, off: %06x\n", 
           chu_cfg->colors.key_on_upper, chu_cfg->colors.key_on_lower,
           chu_cfg->colors.key_on_both, chu_cfg->colors.key_off);
    printf("  Gap: %06x\n", chu_cfg->colors.gap);
}

static void disp_style()
{
    printf("[Style]\n");
    printf("  Key: %d, Gap: %d, ToF: %d, Level: %d\n",
           chu_cfg->style.key, chu_cfg->style.gap,
           chu_cfg->style.tof, chu_cfg->style.level);
}

static void disp_tof()
{
    printf("[ToF]\n");
    printf("  Offset: %d, Pitch: %d\n", chu_cfg->tof.offset, chu_cfg->tof.pitch);
}

static void disp_sense()
{
    printf("[Sense]\n");
    printf("  Filter: %u, %u, %u\n", chu_cfg->sense.filter >> 6,
                                    (chu_cfg->sense.filter >> 4) & 0x03,
                                    chu_cfg->sense.filter & 0x07);
    printf("  Sensitivity (global: %+d):\n", chu_cfg->sense.global);
    printf("    | 1| 2| 3| 4| 5| 6| 7| 8| 9|10|11|12|13|14|15|16|\n");
    printf("  ---------------------------------------------------\n");
    printf("  A |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2]);
    }
    printf("\n  B |");
    for (int i = 0; i < 16; i++) {
        printf("%+2d|", chu_cfg->sense.keys[i * 2 + 1]);
    }
    printf("\n");
    printf("  Debounce (touch, release): %d, %d\n",
           chu_cfg->sense.debounce_touch, chu_cfg->sense.debounce_release);
}

static void disp_hid()
{
    printf("[HID]\n");
    printf("  Joy: %s, NKRO: %s.\n", 
           chu_cfg->hid.joy ? "on" : "off",
           chu_cfg->hid.nkro ? "on" : "off" );
}

void handle_display(int argc, char *argv[])
{
    const char *usage = "Usage: display [colors|style|tof|sense|hid]\n";
    if (argc > 1) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        disp_colors();
        disp_style();
        disp_tof();
        disp_sense();
        disp_hid();
        return;
    }

    const char *choices[] = {"colors", "style", "tof", "sense", "hid"};
    switch (match_prefix(choices, 5, argv[0])) {
        case 0:
            disp_colors();
            break;
        case 1:
            disp_style();
            break;
        case 2:
            disp_tof();
            break;
        case 3:
            disp_sense();
            break;
        case 4:
            disp_hid();
            break;
        default:
            printf(usage);
            break;
    }
}

static int fps[2];
void fps_count(int core)
{
    static uint32_t last[2] = {0};
    static int counter[2] = {0};

    counter[core]++;

    uint32_t now = time_us_32();
    if (now - last[core] < 1000000) {
        return;
    }
    last[core] = now;
    fps[core] = counter[core];
    counter[core] = 0;
}

static void handle_fps(int argc, char *argv[])
{
    printf("FPS: core 0: %d, core 1: %d\n", fps[0], fps[1]);
}

static int extract_non_neg_int(const char *param, int len)
{
    if (len == 0) {
        len = strlen(param);
    }
    int result = 0;
    for (int i = 0; i < len; i++) {
        if (!isdigit(param[i])) {
            return -1;
        }
        result = result * 10 + param[i] - '0';
    }
    return result;
}

static void handle_level(int argc, char *argv[])
{
    const char *usage = "Usage: level <0..255>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    int level = extract_non_neg_int(argv[0], 0);
    if ((level < 0) || (level > 255)) {
        printf(usage);
        return;
    }

    chu_cfg->style.level = level;
    config_changed();
    disp_style();
}

static void handle_stat(int argc, char *argv[])
{
    if (argc == 0) {
        for (int col = 0; col < 4; col++) {
            printf(" %2dA |", col * 4 + 1);
            for (int i = 0; i < 4; i++) {
                printf("%6u|", slider_count(col * 8 + i * 2));
            }
            printf("\n   B |");
            for (int i = 0; i < 4; i++) {
                printf("%6u|", slider_count(col * 8 + i * 2 + 1));
            }
            printf("\n");
        }
    } else if ((argc == 1) &&
               (strncasecmp(argv[0], "reset", strlen(argv[0])) == 0)) {
        slider_reset_stat();
    } else {
        printf("Usage: stat [reset]\n");
    }
}

static void handle_hid(int argc, char *argv[])
{
    const char *usage = "Usage: hid <joy|nkro|both>\n";
    if (argc != 1) {
        printf(usage);
        return;
    }

    const char *choices[] = {"joy", "nkro", "both"};
    int match = match_prefix(choices, 3, argv[0]);
    if (match < 0) {
        printf(usage);
        return;
    }

    chu_cfg->hid.joy = ((match == 0) || (match == 2)) ? 1 : 0;
    chu_cfg->hid.nkro = ((match == 1) || (match == 2)) ? 1 : 0;
    config_changed();
    disp_hid();
}

static void handle_tof(int argc, char *argv[])
{
    const char *usage = "Usage: tof <offset> [pitch]\n"
                        "  offset: 40..255\n"
                        "  pitch: 4..50\n";
    if (argc > 2) {
        printf(usage);
        return;
    }

    if (argc == 0) {
        printf("TOF: ");
        for (int i = air_num(); i > 0; i--) {
            printf(" %4d", air_raw(i - 1) / 10);
        }
        printf("\n");
        return;
    }

    int offset = chu_cfg->tof.offset;
    int pitch = chu_cfg->tof.pitch;
    if (argc >= 1) {
        offset = extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        pitch = extract_non_neg_int(argv[1], 0);
    }

    if ((offset < 40) || (offset > 255) || (pitch < 4) || (pitch > 50)) {
        printf(usage);
        return;
    }

    chu_cfg->tof.offset = offset;
    chu_cfg->tof.pitch = pitch;

    config_changed();
    disp_tof();
}

static void handle_filter(int argc, char *argv[])
{
    const char *usage = "Usage: filter <first> <second> [interval]\n"
                        "    first: First iteration [0..3]\n"
                        "   second: Second iteration [0..3]\n"
                        " interval: Interval of second iterations [0..7]\n";
    if ((argc < 2) || (argc > 3)) {
        printf(usage);
        return;
    }

    int ffi = extract_non_neg_int(argv[0], 0);
    int sfi = extract_non_neg_int(argv[1], 0);
    int intv = chu_cfg->sense.filter & 0x07;
    if (argc == 3) {
        intv = extract_non_neg_int(argv[2], 0);
    }

    if ((ffi < 0) || (ffi > 3) || (sfi < 0) || (sfi > 3) ||
        (intv < 0) || (intv > 7)) {
        printf(usage);
        return;
    }

    chu_cfg->sense.filter = (ffi << 6) | (sfi << 4) | intv;

    slider_update_config();
    config_changed();
    disp_sense();
}

static uint8_t *extract_key(const char *param)
{
    int len = strlen(param);

    int offset;
    if (toupper(param[len - 1]) == 'A') {
        offset = 0;
    } else if (toupper(param[len - 1]) == 'B') {
        offset = 1;
    } else {
        return NULL;
    }

    int id = extract_non_neg_int(param, len - 1) - 1;
    if ((id < 0) || (id > 15)) {
        return NULL;
    }

    return &chu_cfg->sense.keys[id * 2 + offset];
}

static void sense_do_op(int8_t *target, char op)
{
    if (op == '+') {
        if (*target < SENSE_LIMIT_MAX) {
            (*target)++;
        }
    } else if (op == '-') {
        if (*target > SENSE_LIMIT_MIN) {
            (*target)--;
        }
    } else if (op == '0') {
        *target = 0;
    }
}

static void handle_sense(int argc, char *argv[])
{
    const char *usage = "Usage: sense [key|*] <+|-|0>\n"
                        "Example:\n"
                        "  >sense +\n"
                        "  >sense -\n"
                        "  >sense 1A +\n"
                        "  >sense 13B -\n";
                        "  >sense * 0\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    const char *op = argv[argc - 1];
    if ((strlen(op) != 1) || !strchr("+-0", op[0])) {
        printf(usage);
        return;
    }

    if (argc == 1) {
        sense_do_op(&chu_cfg->sense.global, op[0]);
    } else {
        if (strcmp(argv[0], "*") == 0) {
            for (int i = 0; i < 32; i++) {
                sense_do_op(&chu_cfg->sense.keys[i], op[0]);
            }
        } else {
            uint8_t *key = extract_key(argv[0]);
            if (!key) {
                printf(usage);
                return;
            }
            sense_do_op(key, op[0]);
        }
    }

    slider_update_config();
    config_changed();
    disp_sense();
}

static void handle_debounce(int argc, char *argv[])
{
    const char *usage = "Usage: debounce <touch> [release]\n"
                        "  touch, release: 0..7\n";
    if ((argc < 1) || (argc > 2)) {
        printf(usage);
        return;
    }

    int touch = chu_cfg->sense.debounce_touch;
    int release = chu_cfg->sense.debounce_release;
    if (argc >= 1) {
        touch = extract_non_neg_int(argv[0], 0);
    }
    if (argc == 2) {
        release = extract_non_neg_int(argv[1], 0);
    }

    if ((touch < 0) || (release < 0) ||
        (touch > 7) || (release > 7)) {
        printf(usage);
        return;
    }

    chu_cfg->sense.debounce_touch = touch;
    chu_cfg->sense.debounce_release = release;

    slider_update_config();
    config_changed();
    disp_sense();
}

static void handle_raw()
{
    printf("Key raw readings:\n");
    const uint16_t *raw = slider_raw();
    printf("|");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", raw[i * 2]);
    }
    printf("\n|");
    for (int i = 0; i < 16; i++) {
        printf("%3d|", raw[i * 2 + 1]);
    }
    printf("\n");
}

static void handle_save()
{
    save_request(true);
}

static void handle_factory_reset()
{
    config_factory_reset();
    printf("Factory reset done.\n");
}

void cmd_init()
{
    register_command("?", handle_help, "Display this help message.");
    register_command("display", handle_display, "Display all config.");
    register_command("fps", handle_fps, "Display FPS.");
    register_command("level", handle_level, "Set LED brightness level.");
    register_command("stat", handle_stat, "Display or reset statistics.");
    register_command("hid", handle_hid, "Set HID mode.");
    register_command("tof", handle_tof, "Set ToF config.");
    register_command("filter", handle_filter, "Set pre-filter config.");
    register_command("sense", handle_sense, "Set sensitivity config.");
    register_command("debounce", handle_debounce, "Set debounce config.");
    register_command("raw", handle_raw, "Show key raw readings.");
    register_command("save", handle_save, "Save config to flash.");
    register_command("factory", config_factory_reset, "Reset everything to default.");
}

static char cmd_buf[256];
static int cmd_len = 0;

static void process_cmd()
{
    char *argv[MAX_PARAMETERS];
    int argc;

    char *cmd = strtok(cmd_buf, " \n");

    if (strlen(cmd) == 0) {
        return;
    }

    argc = 0;
    while ((argc < MAX_PARAMETERS) &&
           (argv[argc] = strtok(NULL, " \n")) != NULL) {
        argc++;
    }

    int match = match_prefix(commands, num_commands, cmd);
    if (match == -2) {
        printf("Ambiguous command.\n");
        return;
    }
    if (match == -1) {
        printf("Unknown command.\n");
        handle_help(0, NULL);
        return;
    }

    handlers[match](argc, argv);
}

void cmd_run()
{
    int c = getchar_timeout_us(0);
    if (c == EOF) {
        return;
    }

    if (c == '\b' || c == 127) { // both backspace and delete
        if (cmd_len > 0) {
            cmd_len--;
            printf("\b \b");
        }
        return;
    }

    if ((c != '\n') && (c != '\r')) {

        if (cmd_len < sizeof(cmd_buf) - 2) {
            cmd_buf[cmd_len] = c;
            printf("%c", c);
            cmd_len++;
        }
        return;
    }


    cmd_buf[cmd_len] = '\0';
    cmd_len = 0;

    printf("\n");

    process_cmd();

    printf(chu_prompt);
}
