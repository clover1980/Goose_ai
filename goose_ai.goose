// goose_ai.goose — v1.8 with Multi-Turn Memory + /save, /load, /sessions, /delete, /export & /help
extern fn load_system_prompt();
extern fn handle_help_cmd();
extern fn handle_system_cmd();
extern fn handle_save_cmd();
extern fn handle_load_cmd();
extern fn handle_sessions_cmd();
extern fn handle_delete_cmd();
extern fn handle_export_cmd();
extern fn clear_history();
extern fn is_help_cmd() -> i32;
extern fn is_system_cmd() -> i32;
extern fn is_save_cmd() -> i32;
extern fn is_load_cmd() -> i32;
extern fn is_sessions_cmd() -> i32;
extern fn is_delete_cmd() -> i32;
extern fn is_export_cmd() -> i32;
extern fn is_clear_cmd() -> i32;
extern fn read_user_prompt() -> i32;
extern fn is_quit_cmd() -> i32;
extern fn is_health_cmd() -> i32;
extern fn is_evolve_cmd() -> i32;
extern fn input_equals(cmd: const u8*) -> i32;
extern fn prepare_diag_prompt(diag_type: const u8*, question: const u8*);
extern fn build_health_prompt();
extern fn query_local_llm(endpoint: const u8*, model_name: const u8*);
extern fn self_evolve(endpoint: const u8*, model_name: const u8*);

fn main() {
    load_system_prompt();

    print("\x1b[1;35m=========================================================================================================\x1b[0m");
    print("\x1b[1;37m  GOOSE-AI v1.8  |  Self-Evolving Zero-Heap Assistant (Type /help for all commands)\x1b[0m");
    print("\x1b[2m  Commands: /help | /system | /save | /load | /sessions | /delete | /export | /clear | /health | /quit\x1b[0m");
    print("\x1b[1;35m=========================================================================================================\x1b[0m");

    let endpoint = "http://127.0.0.1:8080/v1/chat/completions\0";
    let model = "local-model\0";

    while true {
        if read_user_prompt() == 0 {
            break;
        }
        if is_quit_cmd() == 1 {
            print("\x1b[36mShutting down Goose-AI.\x1b[0m");
            break;
        }
        if is_help_cmd() == 1 {
            handle_help_cmd();
            continue;
        }
        if is_sessions_cmd() == 1 {
            handle_sessions_cmd();
            continue;
        }
        if is_export_cmd() == 1 {
            handle_export_cmd();
            continue;
        }
        if is_delete_cmd() == 1 {
            handle_delete_cmd();
            continue;
        }
        if is_save_cmd() == 1 {
            handle_save_cmd();
            continue;
        }
        if is_load_cmd() == 1 {
            handle_load_cmd();
            continue;
        }
        if is_clear_cmd() == 1 {
            clear_history();
            continue;
        }
        if is_system_cmd() == 1 {
            handle_system_cmd();
            continue;
        }
        if is_evolve_cmd() == 1 {
            self_evolve(endpoint, model);
            continue;
        }
        if is_health_cmd() == 1 {
            print("\x1b[33m[Gathering CUDA + RAM + EDAC telemetry...]\x1b[0m");
            build_health_prompt();
        }
        if input_equals("/gpu\0") == 1 {
            print("\x1b[33m[Checking CUDA status...]\x1b[0m");
            prepare_diag_prompt("gpu\0", "Check my VRAM usage, temperature, and power draw:\0");
        }
        if input_equals("/top\0") == 1 {
            print("\x1b[33m[Checking top RAM-consuming processes...]\x1b[0m");
            prepare_diag_prompt("top\0", "Which processes are using the most physical RAM right now?\0");
        }

        query_local_llm(endpoint, model);
    }
}
