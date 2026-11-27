#ifndef AI_BRIDGE_H
#define AI_BRIDGE_H
#include <stdint.h>
#include <string.h>
typedef struct { uint8_t *data; int64_t len; } gs_sl_u8_bridge;
#define sl_u8 gs_sl_u8_bridge
static void sl_to_cstr(sl_u8 s, char *buf, size_t max_len) {
    size_t n = (s.len > 0 && (size_t)s.len < max_len) ? (size_t)s.len : (max_len - 1);
    if (s.data && n > 0) memcpy(buf, s.data, n);
    buf[n] = '\0';
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <curl/curl.h>

#define MAX_HISTORY 16
#define MAX_MSG_LEN 16384

typedef struct {
    char role[16];
    char content[MAX_MSG_LEN];
} ChatMsg;

static ChatMsg g_history[MAX_HISTORY];
static int g_history_count = 0;

static char g_input_buf[MAX_MSG_LEN] = {0};
static char g_system_prompt[8192] = {0};
static char g_json_payload[524288] = {0};
static char g_evolve_code[262144] = {0};
static size_t g_evolve_len = 0;
static int g_capture_mode = 0;

static char g_last_reply[MAX_MSG_LEN] = {0};
static size_t g_reply_len = 0;

static void get_home_file(const char *filename, char *buf, size_t max_len) {
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        snprintf(buf, max_len, "%s/%s", home, filename);
    } else {
        snprintf(buf, max_len, "%s", filename);
    }
}

static void sanitize_slot_name(const char *arg, char *out, size_t max_len) {
    snprintf(out, max_len, "default");
    if (arg && arg[0] != '\0') {
        size_t j = 0;
        for (size_t i = 0; arg[i] != '\0' && j < max_len - 1; i++) {
            unsigned char c = (unsigned char)arg[i];
            if (isalnum(c) || c == '_' || c == '-') {
                out[j++] = (char)c;
            }
        }
        if (j > 0) out[j] = '\0';
    }
}

static void get_session_path(const char *arg, char *buf, size_t max_len) {
    char clean_name[64];
    sanitize_slot_name(arg, clean_name, sizeof(clean_name));
    char filename[128];
    snprintf(filename, sizeof(filename), ".goose_session_%s.bin", clean_name);
    get_home_file(filename, buf, max_len);
}

void handle_help_cmd(void) {
    printf("\n\x1b[1;35m=== GooseAI v1.8 Command Reference ===\x1b[0m\n");
    printf("\x1b[1;33mSession & Memory:\x1b[0m\n");
    printf("  \x1b[1;36m/save [name]\x1b[0m       Save active chat to slot (default: 'default')\n");
    printf("  \x1b[1;36m/load [name]\x1b[0m       Restore chat from saved slot\n");
    printf("  \x1b[1;36m/sessions\x1b[0m          List all saved sessions, message counts & dates\n");
    printf("  \x1b[1;36m/delete <name>\x1b[0m     Delete a saved session file\n");
    printf("  \x1b[1;36m/export [name]\x1b[0m     Export active chat (or saved slot) to Markdown (.md)\n");
    printf("  \x1b[1;36m/clear\x1b[0m             Clear active chat memory (keeps system prompt)\n\n");
    printf("\x1b[1;33mPersona & Self-Evolution:\x1b[0m\n");
    printf("  \x1b[1;36m/system [prompt]\x1b[0m   View or set & save persistent system persona\n");
    printf("  \x1b[1;36m/evolve <goal>\x1b[0m     Rewrite goose_ai.goose, compile & hot-swap live\n\n");
    printf("\x1b[1;33mHardware Diagnostics :\x1b[0m\n");
    printf("  \x1b[1;36m/health\x1b[0m            Full GPU + RAM telemetry + EDAC/MCE check\n");
    printf("  \x1b[1;36m/gpu\x1b[0m               Check VRAM, temp, utilization & power\n");
    printf("  \x1b[1;36m/top\x1b[0m               Inspect top physical RAM-consuming processes\n\n");
    printf("\x1b[1;33mGeneral:\x1b[0m\n");
    printf("  \x1b[1;36m/help\x1b[0m              Show this command reference\n");
    printf("  \x1b[1;36m/quit\x1b[0m or \x1b[1;36mexit\x1b[0m      Exit GooseAI\n");
}

void save_session_file(const char *slot_name, int silent) {
    char path[512];
    get_session_path(slot_name, path, sizeof(path));

    FILE *f = fopen(path, "wb");
    if (!f) {
        if (!silent) printf("\x1b[1;31m[Save Error] Could not write %s\x1b[0m\n", path);
        return;
    }
    int magic = 0x474F4F53; // "GOOS"
    fwrite(&magic, sizeof(int), 1, f);
    fwrite(&g_history_count, sizeof(int), 1, f);
    if (g_history_count > 0) {
        fwrite(g_history, sizeof(ChatMsg), (size_t)g_history_count, f);
    }
    fclose(f);
    if (!silent) {
        printf("\x1b[1;32m[Session Saved]\x1b[0m %d messages -> %s\n", g_history_count, path);
    }
}

void load_session_file(const char *slot_name, int silent) {
    char path[512];
    get_session_path(slot_name, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (!silent) printf("\x1b[1;31m[Load Error] No saved session found at %s\x1b[0m\n", path);
        return;
    }
    int magic = 0;
    int count = 0;
    if (fread(&magic, sizeof(int), 1, f) != 1 || magic != 0x474F4F53 ||
        fread(&count, sizeof(int), 1, f) != 1 || count < 0 || count > MAX_HISTORY) {
        fclose(f);
        if (!silent) printf("\x1b[1;31m[Load Error] Corrupted or incompatible session file.\x1b[0m\n");
        return;
    }
    if (count > 0) {
        size_t read_n = fread(g_history, sizeof(ChatMsg), (size_t)count, f);
        g_history_count = (int)read_n;
    } else {
        g_history_count = 0;
    }
    fclose(f);
    if (!silent) {
        printf("\x1b[1;32m[Session Loaded]\x1b[0m Restored %d messages from %s\n", g_history_count, path);
    }
}

void handle_export_cmd(void) {
    const char *arg = (strlen(g_input_buf) > 8) ? g_input_buf + 8 : "";
    while (*arg == ' ') arg++;

    static ChatMsg export_buf[MAX_HISTORY];
    int export_count = 0;
    char md_filename[128];

    if (*arg == '\0') {
        if (g_history_count == 0) {
            printf("\x1b[1;33m[Export Skipped]\x1b[0m Active chat history is empty.\n");
            return;
        }
        export_count = g_history_count;
        memcpy(export_buf, g_history, (size_t)export_count * sizeof(ChatMsg));
        snprintf(md_filename, sizeof(md_filename), "goose_chat_export.md");
    } else {
        char clean_name[64];
        sanitize_slot_name(arg, clean_name, sizeof(clean_name));
        char bin_path[512];
        get_session_path(clean_name, bin_path, sizeof(bin_path));

        FILE *f = fopen(bin_path, "rb");
        if (!f) {
            printf("\x1b[1;31m[Export Error]\x1b[0m Saved session '%s' not found (%s)\n", clean_name, bin_path);
            return;
        }
        int magic = 0, count = 0;
        if (fread(&magic, sizeof(int), 1, f) != 1 || magic != 0x474F4F53 ||
            fread(&count, sizeof(int), 1, f) != 1 || count <= 0 || count > MAX_HISTORY) {
            fclose(f);
            printf("\x1b[1;31m[Export Error]\x1b[0m Session '%s' is empty or invalid.\x1b[0m\n", clean_name);
            return;
        }
        export_count = (int)fread(export_buf, sizeof(ChatMsg), (size_t)count, f);
        fclose(f);
        snprintf(md_filename, sizeof(md_filename), "goose_chat_%s.md", clean_name);
    }

    char out_path[512];
    get_home_file(md_filename, out_path, sizeof(out_path));

    FILE *md = fopen(out_path, "w");
    if (!md) {
        printf("\x1b[1;31m[Export Error]\x1b[0m Could not write %s\x1b[0m\n", out_path);
        return;
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char date_str[64] = "Unknown Date";
    if (tm_info) strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(md, "# GooseAI Chat Export\n\n");
    fprintf(md, "- **Exported:** %s\n", date_str);
    fprintf(md, "- **Messages:** %d\n", export_count);
    fprintf(md, "- **System Prompt:** %s\n\n---\n\n", g_system_prompt);

    for (int i = 0; i < export_count; i++) {
        if (strcmp(export_buf[i].role, "user") == 0) {
            fprintf(md, "### You\n\n%s\n\n---\n\n", export_buf[i].content);
        } else {
            fprintf(md, "### GooseAI\n\n%s\n\n---\n\n", export_buf[i].content);
        }
    }

    fclose(md);
    printf("\x1b[1;32m[Markdown Exported]\x1b[0m %d messages -> %s\n", export_count, out_path);
}

void handle_delete_cmd(void) {
    const char *arg = (strlen(g_input_buf) > 8) ? g_input_buf + 8 : "";
    while (*arg == ' ') arg++;
    if (*arg == '\0') {
        printf("\x1b[1;33m[Usage]\x1b[0m /delete <session_name> (e.g., /delete default)\n");
        return;
    }
    if (strcmp(arg, "hotswap") == 0) {
        printf("\x1b[1;31m[Delete Error]\x1b[0m Cannot delete reserved internal slot.\n");
        return;
    }

    char path[512];
    get_session_path(arg, path, sizeof(path));
    if (unlink(path) == 0) {
        printf("\x1b[1;32m[Session Deleted]\x1b[0m Removed %s\n", path);
    } else {
        printf("\x1b[1;31m[Delete Error]\x1b[0m Session '%s' not found (%s)\n", arg, path);
    }
}

void handle_sessions_cmd(void) {
    const char *home = getenv("HOME");
    if (!home || home[0] == '\0') home = ".";

    DIR *dir = opendir(home);
    if (!dir) {
        printf("\x1b[1;31m[Sessions Error] Could not open %s\x1b[0m\n", home);
        return;
    }

    const char *prefix = ".goose_session_";
    size_t prefix_len = strlen(prefix);
    int found = 0;

    printf("\x1b[1;35m[Saved Sessions in %s]:\x1b[0m\n", home);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > prefix_len + 4 &&
            strncmp(name, prefix, prefix_len) == 0 &&
            strcmp(name + len - 4, ".bin") == 0) {

            char slot[64] = {0};
            size_t slot_len = len - prefix_len - 4;
            if (slot_len >= sizeof(slot)) slot_len = sizeof(slot) - 1;
            strncpy(slot, name + prefix_len, slot_len);
            slot[slot_len] = '\0';

            if (strcmp(slot, "hotswap") == 0) continue;

            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", home, name);

            int magic = 0, msg_count = 0;
            FILE *f = fopen(full_path, "rb");
            if (f) {
                fread(&magic, sizeof(int), 1, f);
                fread(&msg_count, sizeof(int), 1, f);
                fclose(f);
            }

            struct stat st;
            char time_str[32] = "unknown";
            if (stat(full_path, &st) == 0) {
                struct tm *tm_info = localtime(&st.st_mtime);
if (tm_info) strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
}
if (magic == 0x474F4F53) {
printf("  \x1b[1;36m• %-18s\x1b[0m (%2d/%d msgs | %s)\n",
slot, msg_count, MAX_HISTORY, time_str);
found++;
}
}
}
closedir(dir);
if (found == 0) {
printf("  \x1b[2m(No saved sessions yet — type /save [name] to create one)\x1b[0m\n");
}
}
int is_help_cmd(void) {
return (strcmp(g_input_buf, "/help") == 0 || strcmp(g_input_buf, "/?") == 0);
}
int is_export_cmd(void) {
return (strcmp(g_input_buf, "/export") == 0 || strncmp(g_input_buf, "/export ", 8) == 0);
}
int is_delete_cmd(void) {
return (strcmp(g_input_buf, "/delete") == 0 || strncmp(g_input_buf, "/delete ", 8) == 0);
}
int is_sessions_cmd(void) {
return (strcmp(g_input_buf, "/sessions") == 0);
}
int is_save_cmd(void) {
return (strcmp(g_input_buf, "/save") == 0 || strncmp(g_input_buf, "/save ", 6) == 0);
}
int is_load_cmd(void) {
return (strcmp(g_input_buf, "/load") == 0 || strncmp(g_input_buf, "/load ", 6) == 0);
}
void handle_save_cmd(void) {
const char *arg = (strlen(g_input_buf) > 6) ? g_input_buf + 6 : "";
while (*arg == ' ') arg++;
save_session_file(arg, 0);
}
void handle_load_cmd(void) {
const char *arg = (strlen(g_input_buf) > 6) ? g_input_buf + 6 : "";
while (*arg == ' ') arg++;
load_session_file(arg, 0);
}
void append_history(const char *role, const char *content) {
if (g_history_count >= MAX_HISTORY) {
memmove(&g_history[0], &g_history[2], (MAX_HISTORY - 2) * sizeof(ChatMsg));
g_history_count -= 2;
}
strncpy(g_history[g_history_count].role, role, 15);
g_history[g_history_count].role[15] = '\0';
strncpy(g_history[g_history_count].content, content, MAX_MSG_LEN - 1);
g_history[g_history_count].content[MAX_MSG_LEN - 1] = '\0';
g_history_count++;
}
void clear_history(void) {
g_history_count = 0;
printf("\x1b[1;33m[Context Cleared] Conversation memory reset (system prompt kept).\x1b[0m\n");
}
void load_system_prompt(void) {
const char *default_sp = "You are GooseAI, a fast, concise Linux & AI infrastructure assistant running locally on a server.";
char path[512];
get_home_file(".goose_system_prompt.txt", path, sizeof(path));
FILE *f = fopen(path, "r");
if (!f) {
strncpy(g_system_prompt, default_sp, sizeof(g_system_prompt) - 1);
FILE *out = fopen(path, "w");
if (out) { fputs(g_system_prompt, out); fclose(out); }
} else {
size_t n = fread(g_system_prompt, 1, sizeof(g_system_prompt) - 1, f);
g_system_prompt[n] = '\0';
fclose(f);
if (n == 0) strncpy(g_system_prompt, default_sp, sizeof(g_system_prompt) - 1);
}
char swap_path[512];
get_session_path("hotswap", swap_path, sizeof(swap_path));
if (access(swap_path, F_OK) == 0) {
load_session_file("hotswap", 1);
unlink(swap_path);
printf("\x1b[1;32m[Hot-Swap Complete]\x1b[0m Restored %d messages from pre-evolve session.\n", g_history_count);
}
}
void handle_system_cmd(void) {
if (strcmp(g_input_buf, "/system") == 0) {
printf("\x1b[1;35m[Active System Prompt]:\x1b[0m %s\n", g_system_prompt);
return;
}
const char *new_sp = g_input_buf + 8;
while (*new_sp == ' ') new_sp++;
if (*new_sp == '\0') {
printf("\x1b[1;35m[Active System Prompt]:\x1b[0m %s\n", g_system_prompt);
return;
}
strncpy(g_system_prompt, new_sp, sizeof(g_system_prompt) - 1);
g_system_prompt[sizeof(g_system_prompt) - 1] = '\0';
char path[512];
get_home_file(".goose_system_prompt.txt", path, sizeof(path));
FILE *out = fopen(path, "w");
if (out) { fputs(g_system_prompt, out); fclose(out); }
printf("\x1b[1;32m[System Prompt Updated & Saved]:\x1b[0m %s\n", g_system_prompt);
}
int read_user_prompt(void) {
printf("\n\x1b[1;36mYou (%d/%d msgs) > \x1b[0m", g_history_count, MAX_HISTORY);
fflush(stdout);
if (!fgets(g_input_buf, sizeof(g_input_buf), stdin)) return 0;
size_t len = strlen(g_input_buf);
if (len > 0 && g_input_buf[len - 1] == '\n') g_input_buf[len - 1] = '\0';
return 1;
}
int is_quit_cmd(void) {
return (strcmp(g_input_buf, "/quit") == 0 || strcmp(g_input_buf, "exit") == 0);
}
int is_clear_cmd(void) {
return (strcmp(g_input_buf, "/clear") == 0);
}
int is_health_cmd(void) {
return (strcmp(g_input_buf, "/health") == 0);
}
int is_evolve_cmd(void) {
return (strncmp(g_input_buf, "/evolve ", 8) == 0);
}
int is_system_cmd(void) {
return (strcmp(g_input_buf, "/system") == 0 || strncmp(g_input_buf, "/system ", 8) == 0);
}

int input_starts_with(sl_u8 prefix_sl) {
    char prefix;
    sl_to_cstr(prefix_sl, prefix, sizeof(prefix));
    size_t len = strlen(prefix);
    return (strncmp(g_input_buf, prefix, len) == 0) ? 1 : 0;
}

void prepare_file_prompt(sl_u8 q_sl) {
    char question;
    sl_to_cstr(q_sl, question, sizeof(question));
    const char *path = strchr(g_input_buf, ' ');
    if (!path) {
        snprintf(g_input_buf, sizeof(g_input_buf), "%s (Error: no file path provided)", question);
        return;
    }
    while (*path == ' ') path++;
    char file_buf[12288] = {0};
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(g_input_buf, sizeof(g_input_buf), "%s\n\n[Error: Could not open file '%s']", question, path);
        return;
    }
    size_t n = fread(file_buf, 1, sizeof(file_buf) - 1, f);
    file_buf[n] = '\0';
    fclose(f);
    snprintf(g_input_buf, sizeof(g_input_buf), "%s\n\nFile (%s):\n```\n%s\n```", question, path, file_buf);
}

void prepare_exec_prompt(sl_u8 q_sl) {
    char question;
    sl_to_cstr(q_sl, question, sizeof(question));
    const char *cmd = strchr(g_input_buf, ' ');
    if (!cmd) {
        snprintf(g_input_buf, sizeof(g_input_buf), "%s (Error: no shell command provided)", question);
        return;
    }
    while (*cmd == ' ') cmd++;
    char cmd_red[1024];
    snprintf(cmd_red, sizeof(cmd_red), "%s 2>&1", cmd);
    char out_buf[12288] = {0};
    FILE *p = popen(cmd_red, "r");
    if (p) {
        size_t n = fread(out_buf, 1, sizeof(out_buf) - 1, p);
        out_buf[n] = '\0';
        pclose(p);
    }
    snprintf(g_input_buf, sizeof(g_input_buf), "%s\n\nCommand (`%s`) output:\n```\n%s\n```", question, cmd, out_buf);
}

void save_last_reply(sl_u8 path_sl) {
    char path;
    sl_to_cstr(path_sl, path, sizeof(path));
    if (g_reply_len == 0) {
        printf("\x1b[1;33m[Save Skipped]\x1b[0m No assistant reply in buffer yet.\n");
        return;
    }
    FILE *f = fopen(path, "w");
    if (!f) {
        printf("\x1b[1;31m[Save Error]\x1b[0m Could not write %s\n", path);
        return;
    }
    fwrite(g_last_reply, 1, g_reply_len, f);
    fclose(f);
    printf("\x1b[1;32m[Saved Reply]\x1b[0m Wrote %zu bytes -> %s\n", g_reply_len, path);
}

int input_equals(sl_u8 cmd_sl) {
    char cmd[128]; sl_to_cstr(cmd_sl, cmd, sizeof(cmd));
return (strcmp(g_input_buf, cmd) == 0);
}
void prepare_diag_prompt(sl_u8 dt_sl, sl_u8 q_sl) {
    char diag_type[64], question[1024];
    sl_to_cstr(dt_sl, diag_type, sizeof(diag_type));
    sl_to_cstr(q_sl, question, sizeof(question));
char raw_stats[4096] = {0};
const char *shell_cmd = NULL;
if (strcmp(diag_type, "gpu") == 0) {
shell_cmd = "nvidia-smi --query-gpu=name,temperature.gpu,utilization.gpu,memory.used,memory.total,power.draw --format=csv,noheader 2>/dev/null";
} else if (strcmp(diag_type, "temps") == 0) {
shell_cmd = "sensors 2>/dev/null | grep -E 'Package|Core 0|Composite|Sensor 2|temp1'";
} else if (strcmp(diag_type, "ecc") == 0) {
shell_cmd = "tail -n 5 /var/log/ram_telemetry.csv 2>/dev/null; dmesg | grep -iE 'edac|mce|hardware error' | tail -n 5";
} else if (strcmp(diag_type, "top") == 0) {
shell_cmd = "ps -eo pid,user,rss,comm --sort=-rss | head -n 8";
} else if (strcmp(diag_type, "uptime") == 0) {
shell_cmd = "uptime && free -h";
}
if (shell_cmd) {
FILE *p = popen(shell_cmd, "r");
if (p) {
size_t n = fread(raw_stats, 1, sizeof(raw_stats) - 1, p);
raw_stats[n] = '\0';
pclose(p);
}
}
snprintf(g_input_buf, sizeof(g_input_buf), "%s\n\nLive Server Telemetry:\n%s", question, raw_stats);
}
void json_escape(const char *src, char *dst, size_t max_len) {
size_t j = 0;
for (size_t i = 0; src[i] != '\0' && j + 6 < max_len; i++) {
unsigned char c = (unsigned char)src[i];
if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = c; }
else if (c == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
else if (c == '\r') { dst[j++] = '\\'; dst[j++] = 'r'; }
else if (c == '\t') { dst[j++] = '\\'; dst[j++] = 't'; }
else if (c >= 32) { dst[j++] = c; }
}
dst[j] = '\0';
}
void build_health_prompt(void) {
char raw_stats[4096] = {0};
FILE *p = popen(
"echo '=== CUDA ==='; "
"nvidia-smi --query-gpu=name,temperature.gpu,utilization.gpu,memory.used,memory.total,power.draw --format=csv,noheader 2>/dev/null; "
"echo '=== RAM TELEMETRY ==='; "
"tail -n 5 /var/log/ram_telemetry.csv 2>/dev/null; "
"echo '=== EDAC / MCE KERNEL LOGS ==='; "
"dmesg | grep -iE 'edac|mce|hardware error' | tail -n 5",
"r"
);
if (p) {
size_t n = fread(raw_stats, 1, sizeof(raw_stats) - 1, p);
raw_stats[n] = '\0';
pclose(p);
}
snprintf(g_input_buf, sizeof(g_input_buf),
"Analyze my hardware telemetry:\n%s",
raw_stats);
}
size_t stream_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
(void)userdata;
size_t total = size * nmemb;
char *pos = ptr;
char *end = ptr + total;
while (pos < end) {
char *key = strstr(pos, "\"content\":\"");
if (!key || key >= end) break;
key += 11;
while (key < end && *key != '"') {
char out_c = *key;
if (*key == '\\' && key + 1 < end) {
key++;
if (*key == 'n') out_c = '\n';
else if (*key == 't') out_c = '\t';
else out_c = *key;
}
putchar(out_c);
if (g_capture_mode) {
if (g_evolve_len + 1 < sizeof(g_evolve_code)) {
g_evolve_code[g_evolve_len++] = out_c;
g_evolve_code[g_evolve_len] = '\0';
}
} else {
if (g_reply_len + 1 < sizeof(g_last_reply)) {
g_last_reply[g_reply_len++] = out_c;
g_last_reply[g_reply_len] = '\0';
}
}
key++;
}
fflush(stdout);
pos = key + 1;
}
return total;
}
void query_local_llm(sl_u8 ep_sl, sl_u8 mod_sl) {
    char endpoint[512], model_name[256];
    sl_to_cstr(ep_sl, endpoint, sizeof(endpoint));
    sl_to_cstr(mod_sl, model_name, sizeof(model_name));
static char escaped_sys[16384];
static char escaped_msg[MAX_MSG_LEN * 2];
json_escape(g_system_prompt, escaped_sys, sizeof(escaped_sys));
size_t max_payload = sizeof(g_json_payload);
size_t offset = (size_t)snprintf(g_json_payload, max_payload,
"{\"model\":\"%s\",\"stream\":true,\"messages\":["
"{\"role\":\"system\",\"content\":\"%s\"}",
model_name, escaped_sys);
if (!g_capture_mode) {
append_history("user", g_input_buf);
for (int i = 0; i < g_history_count && offset + 64 < max_payload; i++) {
json_escape(g_history[i].content, escaped_msg, sizeof(escaped_msg));
int written = snprintf(g_json_payload + offset, max_payload - offset,
",{\"role\":\"%s\",\"content\":\"%s\"}",
g_history[i].role, escaped_msg);
if (written > 0) offset += (size_t)written;
}
} else {
json_escape(g_input_buf, escaped_msg, sizeof(escaped_msg));
if (offset + 64 < max_payload) {
int written = snprintf(g_json_payload + offset, max_payload - offset,
",{\"role\":\"user\",\"content\":\"%s\"}", escaped_msg);
if (written > 0) offset += (size_t)written;
}
}
if (offset + 4 < max_payload) {
snprintf(g_json_payload + offset, max_payload - offset, "]}");
}
CURL *curl = curl_easy_init();
if (!curl) return;
struct curl_slist *headers = NULL;
headers = curl_slist_append(headers, "Content-Type: application/json");
g_reply_len = 0;
g_last_reply[0] = '\0';
if (!g_capture_mode) {
printf("\x1b[1;32mGooseAI > \x1b[0m");
fflush(stdout);
}
curl_easy_setopt(curl, CURLOPT_URL, endpoint);
curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
curl_easy_setopt(curl, CURLOPT_POSTFIELDS, g_json_payload);
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, stream_callback);
curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);
CURLcode res = curl_easy_perform(curl);
if (res != CURLE_OK) {
printf("\n\x1b[1;31m[Connection Error: %s]\x1b[0m\n", curl_easy_strerror(res));
if (!g_capture_mode && g_history_count > 0) g_history_count--;
} else {
printf("\n");
if (!g_capture_mode && g_reply_len > 0) {
append_history("assistant", g_last_reply);
}
}
curl_slist_free_all(headers);
curl_easy_cleanup(curl);
}
void self_evolve(sl_u8 ep_sl, sl_u8 mod_sl) {
    char endpoint[512], model_name[256];
    sl_to_cstr(ep_sl, endpoint, sizeof(endpoint));
    sl_to_cstr(mod_sl, model_name, sizeof(model_name));
char goal[2048] = {0};
strncpy(goal, g_input_buf + 8, sizeof(goal) - 1);
const char *home = getenv("HOME");
if (home) chdir(home);
char src_code[32768] = {0};
FILE *f = fopen("goose_ai.goose", "r");
if (!f) {
printf("\x1b[1;31m[Evolve Error] Cannot find ~/goose_ai.goose.\x1b[0m\n");
return;
}
size_t bytes_read = fread(src_code, 1, sizeof(src_code) - 1, f);
src_code[bytes_read] = '\0';
fclose(f);
snprintf(g_input_buf, sizeof(g_input_buf),
"Here is my current Goose programming language source code:\n\n%s\n\n"
"Modify this Goose code to implement: %s\n"
"Available C helpers you can call:\n"
"- input_equals(\"/cmd\") -> i32\n"
"- prepare_diag_prompt(\"gpu\"|\"temps\"|\"ecc\"|\"top\"|\"uptime\", \"question\")\n"
"- input_starts_with(\"/prefix \") -> i32\n"
"- prepare_file_prompt(\"question\") (reads path after command space into prompt)\n"
"- prepare_exec_prompt(\"question\") (runs shell command after command space into prompt)\n"
"- save_last_reply(\"filepath\") (writes last LLM reply to disk)\n"
"CRITICAL RULES: Output ONLY raw valid Goose code. No markdown fences (```), no explanations. Keep all extern fn declarations intact.",
src_code, goal);
printf("\x1b[1;33m[Evolving] Asking model to rewrite goose_ai.goose...\x1b[0m\n\n");
g_evolve_len = 0;
g_evolve_code[0] = '\0';
g_capture_mode = 1;
query_local_llm(ep_sl, mod_sl);
g_capture_mode = 0;
if (g_evolve_len == 0) {
    printf("\n\x1b[1;31m[Evolve Aborted] No code received from model.\x1b[0m\n");
    return;
}
char *code_start = g_evolve_code;
char *think_end = strstr(code_start, "</think>");
if (think_end) {
    code_start = think_end + 8;
    while (*code_start == '\n' || *code_start == '\r' || *code_start == ' ') code_start++;
}
if (strncmp(code_start, "\x60\x60\x60", 3) == 0) {
    char *nl = strchr(code_start, '\n');
    if (nl) code_start = nl + 1;
}
char *fence_end = strstr(code_start, "\x60\x60\x60");
if (fence_end) *fence_end = '\0';
FILE *out = fopen("goose_ai_next.goose", "w");
if (!out) return;
fputs(code_start, out);
fclose(out);
printf("\n\x1b[1;33m[Compiling] Verifying candidate build with Goose + GCC -O3...\x1b[0m\n");
int status = system("goose --standalone -o goose_ai_next.c goose_ai_next.goose && gcc -O3 -c goose_ai_next.c -o goose_ai_next.o && gcc -O3 goose_ai_next.o ai_bridge.o -lcurl -lm -lpthread -o goose_ai_next");
if (status != 0) {
printf("\x1b[1;31m[Rollback] Candidate failed to compile! Keeping current stable version.\x1b[0m\n");
return;
}
printf("\x1b[1;32m[Success] Compilation passed! Saving context, backing up source & hot-swapping...\x1b[0m\n");
save_session_file("hotswap", 1);
system("cp goose_ai.goose goose_ai.goose.bak && mv goose_ai_next.goose goose_ai.goose && mv goose_ai_next ./goose_ai_bin");
char *args[] = {"./goose_ai_bin", NULL};
execv("./goose_ai_bin", args);
}
#endif // AI_BRIDGE_H
