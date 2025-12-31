#ifndef POSTOFFICE_RENDERER_CLAY_NCURSES_RENDERER_H
#define POSTOFFICE_RENDERER_CLAY_NCURSES_RENDERER_H

#include <clay/clay.h>
#include <ncurses.h>

// Font styles for Clay text configuration
#define CLAY_NCURSES_FONT_BOLD 1
#define CLAY_NCURSES_FONT_UNDERLINE 2

// Custom key definitions for input processing
#define CLAY_NCURSES_KEY_SCROLL_UP 123456
#define CLAY_NCURSES_KEY_SCROLL_DOWN 123457
#define CLAY_NCURSES_KEY_MOUSE_CLICK 123458

/**
 * @brief Initializes the Ncurses library and internal renderer state.
 * Sets up locale, screen, keyboard input, and color support.
 */
void Clay_Ncurses_Initialize(void);

/**
 * @brief Terminates the Ncurses library and cleans up.
 * Returns the terminal to its normal state.
 */
void Clay_Ncurses_Terminate(void);

/**
 * @brief Returns the layout dimensions of the current Ncurses screen.
 * @return The dimensions in Clay logical units.
 */
Clay_Dimensions Clay_Ncurses_GetLayoutDimensions(void);

/**
 * @brief Measures text for layout purposes (assumes fixed-width cells).
 * @param text The text to measure.
 * @param config Text configuration (style flags).
 * @param userData Custom user data (unused).
 * @return The dimensions of the text in Clay logical units.
 */
Clay_Dimensions Clay_Ncurses_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config,
                                         void *userData);

/**
 * @brief Main rendering entry point. Processes the Clay RenderCommandBuffer and draws to the
 * terminal.
 * @param renderCommands The array of commands produced by Clay_EndLayout().
 */
void Clay_Ncurses_Render(Clay_RenderCommandArray renderCommands);

/**
 * @brief Handles Ncurses input and updates Clay's internal pointer state.
 * Use this instead of standard getch() in your main loop to enable mouse interaction.
 * @param window The Ncurses window to read input from (e.g. stdscr).
 * @return The key code pressed, or ERR if no input.
 */
int Clay_Ncurses_ProcessInput(WINDOW *window);

/**
 * @brief Helper to attach an OnClick listener to the current element.
 * Registers a hover callback.
 * @param onClickFunc Function pointer to call.
 * @param userData User data passed to the callback.
 */
void Clay_Ncurses_OnClick(void (*onClickFunc)(Clay_ElementId elementId,
                                              Clay_PointerData pointerData, void *userData),
                          void *userData);

/**
 * @brief Enables or disables raw mode for key capture.
 * In raw mode, interrupt processing (Ctrl+C) and flow control (Ctrl+S/Q) are disabled
 * at the terminal driver level, allowing the application to handle them.
 * @param enable true to enable raw mode, false to disable (cbreak mode).
 */
void Clay_Ncurses_SetRawMode(bool enable);

/**
 * @brief Prepares the terminal for application suspension (e.g. SIGTSTP).
 * Restores terminal settings and disables mouse tracking to allow shell interaction.
 * Should be called before raising SIGSTOP or SIGTSTP.
 */
void Clay_Ncurses_PrepareSuspend(void);

/**
 * @brief Resumes the terminal state after suspension.
 * Re-initializes Ncurses context, clears the screen to remove shell artifacts,
 * and re-enables mouse tracking.
 */
void Clay_Ncurses_ResumeAfterSuspend(void);

/**
 * @brief Processes a single input event from the standard screen.
 * Proxies to internal Ncurses wgetch(stdscr).
 * @return The key code pressed, or ERR if no input.
 */
int Clay_Ncurses_ProcessInputStandard(void);

#endif // POSTOFFICE_RENDERER_CLAY_NCURSES_RENDERER_H
