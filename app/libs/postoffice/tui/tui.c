/**
 * @file tui.c
 * @brief Main TUI library implementation
 */

#define _XOPEN_SOURCE_EXTENDED 1 // For wide character support in ncurses

#include <tui/tui.h>
#include "types.h"
#include "widgets.h" // For widget drawing

#include "perf/perf.h"
#include "perf/ringbuf.h"
#include "perf/zerocopy.h"

#include <ncurses.h>
#include <locale.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

// Internal state
static struct {
    bool initialized;
    bool running;
    int target_fps;
    tui_widget_t* root;
    tui_widget_t* focused;
    struct timespec last_frame_time;
    // Event queue resources
    po_perf_ringbuf_t* event_q;
    perf_zcpool_t* event_pool;
} g_tui = {
    .initialized = false,
    .running = false,
    .target_fps = 30,
    .root = NULL,
    .focused = NULL,
    .event_q = NULL,
    .event_pool = NULL
};

static struct {
    tui_update_cb_t cb;
    void* data;
} g_update = { NULL, NULL };

// Signal handler for resize
static void handle_winch(int sig) {
    (void)sig;
    endwin();
    refresh();
    
    // Post resize event
    tui_event_t event;
    event.type = TUI_EVENT_RESIZE;
    int h, w;
    getmaxyx(stdscr, h, w);
    event.data.resize.height = (int16_t)h;
    event.data.resize.width = (int16_t)w;
    tui_post_event(&event);
}

bool tui_init(void) {
    if (g_tui.initialized) return true;

    // Set locale for unicode support
    setlocale(LC_ALL, "");

    // Initialize ncurses
    initscr();
    cbreak();               // Disable line buffering
    noecho();               // Don't echo input
    keypad(stdscr, TRUE);   // Enable special keys
    curs_set(0);            // Hide cursor
    nodelay(stdscr, TRUE);  // Non-blocking input
    
    // Initialize colors
    if (has_colors()) {
        start_color();
        use_default_colors();
    }
    
    // Mouse support
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    printf("\033[?1003h\n"); // Enable mouse movement reporting

    // Signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_winch;
    sigaction(SIGWINCH, &sa, NULL);

    // Initialize perf resources
    g_tui.event_q = perf_ringbuf_create(256);
    g_tui.event_pool = perf_zcpool_create(256, sizeof(tui_event_t));
    if (!g_tui.event_q || !g_tui.event_pool) {
        endwin();
        return false;
    }
    
    // Attempt init perf (ignore EALREADY)
    if (po_perf_init(16, 16, 16) == 0) {
        // Register TUI metrics
        po_perf_counter_create("tui.events");
        po_perf_counter_create("tui.frames");
    }

    g_tui.initialized = true;
    g_tui.running = true;

    // Initial resize event
    tui_event_t event;
    event.type = TUI_EVENT_RESIZE;
    int h, w;
    getmaxyx(stdscr, h, w);
    event.data.resize.height = (int16_t)h;
    event.data.resize.width = (int16_t)w;
    tui_post_event(&event);

    return true;
}

void tui_cleanup(void) {
    if (!g_tui.initialized) return;

    printf("\033[?1003l\n"); // Disable mouse movement reporting
    endwin();
    
    if (g_tui.event_q) perf_ringbuf_destroy(&g_tui.event_q);
    if (g_tui.event_pool) perf_zcpool_destroy(&g_tui.event_pool);

    g_tui.initialized = false;
    g_tui.running = false;
    g_update.cb = NULL;
    g_update.data = NULL;
}

bool tui_set_target_fps(int fps) {
    if (fps <= 0) return false;
    g_tui.target_fps = fps;
    return true;
}

void tui_set_update_callback(tui_update_cb_t cb, void* data) {
    g_update.cb = cb;
    g_update.data = data;
}

void tui_quit(void) {
    g_tui.running = false;
}

bool tui_is_running(void) {
    return g_tui.running;
}

void tui_run(void) {
    if (!g_tui.initialized) return;

    g_tui.running = true;
    while (g_tui.running) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        tui_process_events();
        
        if (g_update.cb) {
            g_update.cb(g_update.data);
        }

        tui_render();
        po_perf_counter_inc("tui.frames");

        clock_gettime(CLOCK_MONOTONIC, &end);
        
        // Frame limiting
        long frame_ns = 1000000000 / g_tui.target_fps;
        long elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
        
        if (elapsed_ns < frame_ns) {
            struct timespec sleep_time;
            sleep_time.tv_sec = 0;
            sleep_time.tv_nsec = frame_ns - elapsed_ns;
            nanosleep(&sleep_time, NULL);
        }
    }
}

tui_size_t tui_get_screen_size(void) {
    tui_size_t size;
    int h, w;
    getmaxyx(stdscr, h, w);
    size.height = (int16_t)h;
    size.width = (int16_t)w;
    return size;
}

void tui_set_root(tui_widget_t* root) {
    g_tui.root = root;
    if (root) {
        // Resize root to fit screen
        tui_size_t size = tui_get_screen_size();
        tui_rect_t bounds = {{0, 0}, size};
        tui_widget_set_bounds(root, bounds);
    }
}

tui_widget_t* tui_get_root(void) {
    return g_tui.root;
}

tui_widget_t* tui_get_focused_widget(void) {
    return g_tui.focused;
}

void tui_set_focus(tui_widget_t* widget) {
    if (g_tui.focused == widget) return;

    if (g_tui.focused) {
        g_tui.focused->has_focus = false;
        tui_event_t event = {.type = TUI_EVENT_FOCUS, .data = {.has_focus = false}};
        tui_send_event(g_tui.focused, &event);
    }

    g_tui.focused = widget;

    if (g_tui.focused) {
        g_tui.focused->has_focus = true;
        tui_event_t event = {.type = TUI_EVENT_FOCUS, .data = {.has_focus = true}};
        tui_send_event(g_tui.focused, &event);
    }
}

// Event queue implementation using perf ringbuf
void tui_post_event(const tui_event_t* event) {
    if (!g_tui.event_pool || !g_tui.event_q) return;

    tui_event_t* e = perf_zcpool_acquire(g_tui.event_pool);
    if (!e) return; // Drop if no buffers

    *e = *event;
    if (perf_ringbuf_enqueue(g_tui.event_q, e) != 0) {
        perf_zcpool_release(g_tui.event_pool, e);
    }
}

bool tui_process_event(void) {
    // 1. Check for ncurses input events
    int c = getch();
    if (c != ERR) {
        if (c == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                tui_event_t mouse_event;
                mouse_event.type = TUI_EVENT_MOUSE;
                mouse_event.data.mouse.x = event.x;
                mouse_event.data.mouse.y = event.y;
                mouse_event.data.mouse.button = event.bstate; // Simplified
                mouse_event.data.mouse.pressed = (event.bstate & (BUTTON1_PRESSED | BUTTON2_PRESSED | BUTTON3_PRESSED));
                tui_post_event(&mouse_event);
            }
        } else if (c == KEY_RESIZE) {
            // Handled by signal handler, but sometimes ncurses emits this too
            tui_event_t resize_event;
            resize_event.type = TUI_EVENT_RESIZE;
            int h, w;
            getmaxyx(stdscr, h, w);
            resize_event.data.resize.height = (int16_t)h;
            resize_event.data.resize.width = (int16_t)w;
            tui_post_event(&resize_event);
        } else {
            tui_event_t key_event;
            key_event.type = TUI_EVENT_KEY;
            key_event.data.key = c;
            tui_post_event(&key_event);
        }
    }

    // 2. Process one event from queue
    tui_event_t* ptr_event = NULL;
    if (perf_ringbuf_dequeue(g_tui.event_q, (void**)&ptr_event) != 0) {
        return false;
    }

    tui_event_t event = *ptr_event;
    // Release buffer immediately after copy
    perf_zcpool_release(g_tui.event_pool, ptr_event);

    po_perf_counter_inc("tui.events");

    // Global handling
    if (event.type == TUI_EVENT_RESIZE) {
        if (g_tui.root) {
            tui_rect_t bounds = {
                .position = {0, 0},
                .size = {(int16_t)event.data.resize.width, (int16_t)event.data.resize.height}
            };
            tui_widget_set_bounds(g_tui.root, bounds);
        }
        clear(); // Invalidate screen
    }

    // Dispatch to focused widget first (for keys)
    bool handled = false;
    if (event.type == TUI_EVENT_KEY && g_tui.focused) {
        handled = tui_send_event(g_tui.focused, &event);
    }

    // If not handled, try root (bubbling up or down depending on event type)
    if (!handled && g_tui.root) {
         // Mouse events need hit testing, others go to root
        if (event.type == TUI_EVENT_MOUSE) {
             tui_point_t p = {(int16_t)event.data.mouse.x, (int16_t)event.data.mouse.y};
             tui_widget_t* target = tui_widget_find_at(g_tui.root, p);
             if (target) {
                 // Update focus on click
                 if (event.data.mouse.pressed && target->focusable) {
                     tui_set_focus(target);
                 }
                 handled = tui_send_event(target, &event);
             }
        } else {
            handled = tui_send_event(g_tui.root, &event);
        }
    }
    
    return true;
}

void tui_process_events(void) {
    while (tui_process_event()) {
        // Keep processing until empty
    }
}

void tui_render(void) {
    if (!g_tui.root) return;

    erase();
    tui_widget_draw(g_tui.root);
    refresh();
}

void tui_sleep(unsigned int ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

void tui_redraw(void) {
    clear();
    tui_render();
}

tui_color_pair_t tui_create_color_pair(int16_t fg, int16_t bg) {
    static int16_t pair_count = 1;
    if (pair_count >= COLOR_PAIRS) return 0;
    
    init_pair(pair_count, fg, bg);
    init_pair(pair_count, fg, bg);
    return (tui_color_pair_t)(pair_count++);
}

// Utility drawing functions wrapper around ncurses
void tui_draw_box(tui_rect_t bounds, bool has_border) {
    (void)bounds; (void)has_border;
    // Basic implementation stub
}

void tui_draw_text(tui_point_t pos, const char* text, uint16_t attrs) {
    (void)attrs;
    move(pos.y, pos.x);
    // apply attrs...
    addstr(text);
}

// ... other implementations
bool tui_send_event(tui_widget_t* widget, const tui_event_t* event) {
    if (!widget || !widget->handle_event) return false;
    return widget->handle_event(widget, event);
}
