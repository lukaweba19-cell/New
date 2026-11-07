/**
 * Delta Networking Stub Layer
 *
 * When the full cpp-httplib / ixwebsocket / OpenSSL libraries are not
 * linked into the dylib, this file provides stub implementations that
 * record request metadata into the handle table and return "not
 * implemented" responses.
 *
 * This allows the dylib to load and the API table to be complete even
 * in environments where the networking libraries are unavailable
 * (e.g., a host process without network access, or a debug build).
 *
 * The stubs mirror the full implementations in networking.cpp so that
 * the linker can resolve all symbols regardless of which variant is
 * compiled in.
 */

#include "delta_executor_api.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Forward declarations matching networking.cpp
void run_http_get(int handle, const char* url, const char* headers);
void run_http_post(int handle, const char* url, const char* data, const char* headers);
void run_http_request(int handle, const char* method, const char* url,
                      const char* headers, const char* body);
void cancel_http_request(int handle);

void start_websocket(int handle, const char* url, const char* protocols);
void send_websocket_text(int handle, const char* data);
void send_websocket_binary(int handle, const uint8_t* data, size_t len);
void close_websocket(int handle, int code, const char* reason);

// ============================================================================
// HTTP stubs
// ============================================================================

void run_http_get(int handle, const char* url, const char* headers) {
    (void)handle;
    (void)url;
    (void)headers;
    // Stub: mark the handle with a "not implemented" response.
    // When the caller polls api_get_http_status it will get -1.
    extern struct HttpRequestState g_http_handles[];
    extern std::mutex g_http_mutex;
    if (handle >= 0 && handle < 64) {
        std::lock_guard<std::mutex> lock(g_http_mutex);
        if (g_http_handles[handle].active) {
            g_http_handles[handle].status_code = -1;
            std::free(g_http_handles[handle].body);
            const char* msg = "[HTTP stub: not linked]";
            size_t len = std::strlen(msg);
            g_http_handles[handle].body =
                static_cast<char*>(std::malloc(len + 1));
            if (g_http_handles[handle].body) {
                std::memcpy(g_http_handles[handle].body, msg, len + 1);
                g_http_handles[handle].body_size = len;
            }
        }
    }
}

void run_http_post(int handle, const char* url, const char* data,
                    const char* headers) {
    (void)handle;
    (void)url;
    (void)data;
    (void)headers;
    extern struct HttpRequestState g_http_handles[];
    extern std::mutex g_http_mutex;
    if (handle >= 0 && handle < 64) {
        std::lock_guard<std::mutex> lock(g_http_mutex);
        if (g_http_handles[handle].active) {
            g_http_handles[handle].status_code = -1;
            std::free(g_http_handles[handle].body);
            const char* msg = "[HTTP stub: not linked]";
            size_t len = std::strlen(msg);
            g_http_handles[handle].body =
                static_cast<char*>(std::malloc(len + 1));
            if (g_http_handles[handle].body) {
                std::memcpy(g_http_handles[handle].body, msg, len + 1);
                g_http_handles[handle].body_size = len;
            }
        }
    }
}

void run_http_request(int handle, const char* method, const char* url,
                      const char* headers, const char* body) {
    (void)handle;
    (void)method;
    (void)url;
    (void)headers;
    (void)body;
    extern struct HttpRequestState g_http_handles[];
    extern std::mutex g_http_mutex;
    if (handle >= 0 && handle < 64) {
        std::lock_guard<std::mutex> lock(g_http_mutex);
        if (g_http_handles[handle].active) {
            g_http_handles[handle].status_code = -1;
            std::free(g_http_handles[handle].body);
            const char* msg = "[HTTP stub: not linked]";
            size_t len = std::strlen(msg);
            g_http_handles[handle].body =
                static_cast<char*>(std::malloc(len + 1));
            if (g_http_handles[handle].body) {
                std::memcpy(g_http_handles[handle].body, msg, len + 1);
                g_http_handles[handle].body_size = len;
            }
        }
    }
}

void cancel_http_request(int handle) {
    (void)handle;
}

// ============================================================================
// WebSocket stubs
// ============================================================================

void start_websocket(int handle, const char* url, const char* protocols) {
    (void)handle;
    (void)url;
    (void)protocols;
    extern struct WsState g_ws_states[];
    extern std::mutex g_ws_mutex;
    if (handle >= 0 && handle < 16) {
        std::lock_guard<std::mutex> lock(g_ws_mutex);
        if (g_ws_states[handle].active) {
            g_ws_states[handle].ready_state = 3;  // CLOSED
        }
    }
}

void send_websocket_text(int handle, const char* data) {
    (void)handle;
    (void)data;
}

void send_websocket_binary(int handle, const uint8_t* data, size_t len) {
    (void)handle;
    (void)data;
    (void)len;
}

void close_websocket(int handle, int code, const char* reason) {
    (void)handle;
    (void)code;
    (void)reason;
}
