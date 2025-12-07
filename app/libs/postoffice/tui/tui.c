/**
 * @file tui.c
 * @brief Main TUI library implementation
 */

#define _XOPEN_SOURCE_EXTENDED 1 // For wide character support in ncurses

#include <tui/tui.h>
#include "types.h"
#include "widgets.h" // For widget drawing

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
} g_tui = {
    .initialized = false,
    .running = false,
    .target_fps = 30,
    .root = NULL,
    .focused = NULL
};

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
    g_tui.initialized = false;
    g_tui.running = false;
}

bool tui_set_target_fps(int fps) {
    if (fps <= 0) return false;
    g_tui.target_fps = fps;
    return true;
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
        tui_render();

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

// Event queue implementation (simple circular buffer)
#define EVENT_QUEUE_SIZE 64
static tui_event_t g_event_queue[EVENT_QUEUE_SIZE];
static int g_event_head = 0;
static int g_event_tail = 0;

void tui_post_event(const tui_event_t* event) {
    int next_head = (g_event_head + 1) % EVENT_QUEUE_SIZE;
    if (next_head != g_event_tail) {
        g_event_queue[g_event_head] = *event;
        g_event_head = next_head;
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
    if (g_event_head == g_event_tail) {
        return false;
    }

    tui_event_t event = g_event_queue[g_event_tail];
    g_event_tail = (g_event_tail + 1) % EVENT_QUEUE_SIZE;

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
