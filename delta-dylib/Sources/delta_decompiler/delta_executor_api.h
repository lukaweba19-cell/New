/**
 * Delta Executor API Function Declarations
 *
 * Declares the native function implementations that back the protected
 * executor API registration table in decompiler_bridge.cpp.
 */

#ifndef DELTA_EXECUTOR_API_H
#define DELTA_EXECUTOR_API_H

#include <cstddef>
#include <cstdint>

// Networking layer forward declarations (implemented in networking.cpp,
// stubbed in networking_stub.cpp when the full stack is not linked).
#ifdef __cplusplus
extern "C" {
#endif

// HTTP
void run_http_get(int handle, const char* url, const char* headers);
void run_http_post(int handle, const char* url, const char* data, const char* headers);
void run_http_request(int handle, const char* method, const char* url,
                      const char* headers, const char* body);
void cancel_http_request(int handle);

// WebSocket
void start_websocket(int handle, const char* url, const char* protocols);
void send_websocket_text(int handle, const char* data);
void send_websocket_binary(int handle, const uint8_t* data, size_t len);
void close_websocket(int handle, int code, const char* reason);

#ifdef __cplusplus
}
#endif

namespace delta_executor {

// --- Basic I/O ---
void api_print(const char* str);
void api_print_warning(const char* str);
void api_print_error(const char* str);

// --- Task / thread management ---
void api_spawn(const char* script);
void api_wait(float seconds);
void api_delay();

// --- HTTP / networking ---
int  api_http_get(const char* url, const char* headers);
int  api_http_post(const char* url, const char* data, const char* headers);
int  api_http_request(const char* method, const char* url,
                      const char* headers, const char* body,
                      int* out_status, char** out_body);
void api_http_close(int handle);
int  api_get_http_status(int handle);
const char* api_get_http_body(int handle);
const char* api_get_http_header(int handle, const char* key);

// --- WebSocket ---
int  api_ws_connect(const char* url, const char* protocols);
void api_ws_send(int handle, const char* data);
void api_ws_send_binary(int handle, const uint8_t* data, size_t len);
void api_ws_close(int handle, int code, const char* reason);
int  api_ws_get_ready_state(int handle);
const char* api_ws_get_message(int handle);
void api_ws_set_on_message(int handle, void* callback);
void api_ws_set_on_close(int handle, void* callback);
void api_ws_set_on_error(int handle, void* callback);

// --- File I/O ---
int  api_file_read(const char* path, char** out_content, int* out_size);
int  api_file_write(const char* path, const char* content);
int  api_file_exists(const char* path);
void api_file_delete(const char* path);
int  api_file_get_size(const char* path);

// --- OS / environment ---
const char* api_get_platform();
const char* api_get_device_model();
int  api_get_os_version(int* major, int* minor);
const char* api_get_bundle_id();
double api_get_battery_level();
int  api_is_jailbroken();
int  api_is_debugger_attached();
void api_get_app_info(char* out_info, size_t out_size);

// --- Memory / performance ---
size_t api_get_memory_usage();
double api_get_fps();
void  api_get_profiler_data(char* out_data, size_t out_size);

// --- Utility ---
const char* api_base64_encode(const char* data, size_t len, size_t* out_len);
int  api_base64_decode(const char* encoded, char** out_data, size_t* out_len);
const char* api_sha256(const char* data);
const char* api_md5(const char* data);
uint64_t api_get_time_ms();
uint64_t api_get_tick_count();
void  api_sleep_ms(int milliseconds);
void  api_execute_command(const char* cmd, char** out_output, int* out_size);
void  api_clipboard_get_text(char** out_text);
void  api_clipboard_set_text(const char* text);
int  api_vibrate(int pattern);
int  api_play_sound(const char* path, float volume);
void  api_set_volume(float volume);
float api_get_volume();

// --- Roblox-specific ---
const char* api_get_place_id();
const char* api_get_user_id();
const char* api_get_account_info();
int  api_join_place(const char* place_id);
void  api_leave_place();
const char* api_get_game_instance_id();

} // namespace delta_executor

#endif // DELTA_EXECUTOR_API_H
