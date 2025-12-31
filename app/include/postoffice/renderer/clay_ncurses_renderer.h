#ifndef POSTOFFICE_RENDERER_CLAY_NCURSES_RENDERER_H
#define POSTOFFICE_RENDERER_CLAY_NCURSES_RENDERER_H

#include <clay/clay.h>
#include <ncurses.h>

// Font styles for Clay text configuration
#define CLAY_NCURSES_FONT_BOLD 1       /**< Bold text style flag. */
#define CLAY_NCURSES_FONT_UNDERLINE 2  /**< Underline text style flag. */

// Custom key definitions for input processing
#define CLAY_NCURSES_KEY_SCROLL_UP 123456   /**< Virtual key code for mouse scroll up. */
#define CLAY_NCURSES_KEY_SCROLL_DOWN 123457 /**< Virtual key code for mouse scroll down. */
#define CLAY_NCURSES_KEY_MOUSE_CLICK 123458 /**< Virtual key code for generic mouse click. */

/**
 * @brief Initializes the Ncurses library and internal renderer state.
 * 
 * Sets up the following:
 * - System locale (for UTF-8 support).
 * - Ncurses window (stdscr).
 * - Keypad mode (for arrow keys, F-keys).
 * - Mouse masking (all events).
 * - Non-blocking input.
 * - Colors (if supported).
 * - Internal scissor stack.
 */
void Clay_Ncurses_Initialize(void);

/**
 * @brief Terminates the Ncurses library and cleans up resources.
 * 
 * Restores the terminal to its normal state ("cooked" mode), 
 * clears the screen, and ends the Ncurses window session.
 */
void Clay_Ncurses_Terminate(void);

/**
 * @brief Returns the layout dimensions of the current Ncurses screen.
 * 
 * Used by Clay to calculate the layout tree based on the available terminal size.
 * 
 * @return The dimensions in Clay logical units (Pixels).
 *         Note: 1 Cell = 8x16 "Pixel" units by default.
 */
Clay_Dimensions Clay_Ncurses_GetLayoutDimensions(void);

/**
 * @brief Measures text for layout purposes (assumes fixed-width cells).
 * 
 * @param text The text to measure.
 * @param config Text configuration (style flags like BOLD/UNDERLINE).
 * @param userData Custom user data (unused).
 * @return The dimensions of the text in Clay logical units.
 */
Clay_Dimensions Clay_Ncurses_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config,
                                         void *userData);

/**
 * @brief Main rendering entry point.
 * 
 * Processes the Clay RenderCommandBuffer and draws primitive shapes (Rectangles, 
 * Text, Borders) to the terminal using Ncurses.
 * 
 * @param renderCommands The array of commands produced by Clay_EndLayout().
 */
void Clay_Ncurses_Render(Clay_RenderCommandArray renderCommands);

/**
 * @brief Handles Ncurses input for a specific window.
 * 
 * Reads a character from the given ncurses window. Converts mouse events 
 * into Clay internal pointer state updates (for hover/click detection).
 * 
 * @param window The Ncurses window to read input from (usually stdscr).
 * @return The key code pressed, one of CLAY_NCURSES_KEY_* pseudo-keys, 
 *         or ERR if no input is available.
 */
int Clay_Ncurses_ProcessInput(WINDOW *window);

/**
 * @brief Standard input processing helper.
 * 
 * A convenience wrapper around Clay_Ncurses_ProcessInput(stdscr).
 * 
 * @return The key code or ERR.
 */
int Clay_Ncurses_ProcessInputStandard(void);

/**
 * @brief Helper to attach an OnClick listener to the current element.
 * 
 * Registers a hover callback that triggers only when the element is clicked.
 * 
 * @param onClickFunc Function pointer to call on click.
 * @param userData User data passed to the callback.
 */
void Clay_Ncurses_OnClick(void (*onClickFunc)(Clay_ElementId elementId,
                                              Clay_PointerData pointerData, void *userData),
                          void *userData);

/**
 * @brief Enables or disables "raw" mode for key capture.
 * 
 * In raw mode:
 * - Interrupt signals (Ctrl+C, Ctrl+Z) are NOT handled by the terminal driver.
 * - Input is passed directly to the application.
 * 
 * Use this if you need to handle Ctrl+C/Ctrl+Z manually.
 * 
 * @param enable true to enable raw mode, false to disable (revert to cbreak mode).
 */
void Clay_Ncurses_SetRawMode(bool enable);

/**
 * @brief Prepares the terminal for application suspension (SIGTSTP/Ctrl+Z).
 * 
 * - Restores terminal settings (endwin).
 * - Disables mouse tracking (to prevent garbage characters in shell).
 * 
 * Call this BEFORE raising SIGTSTP or SIGSTOP.
 */
void Clay_Ncurses_PrepareSuspend(void);

/**
 * @brief Resumes the terminal state after suspension.
 * 
 * - Re-initializes Ncurses context (refresh).
 * - Clears screen artifacts.
 * - Restores Raw/Cbreak mode.
 * - Re-enables mouse tracking.
 * 
 * Call this AFTER returning from suspension (e.g., after SIGCONT).
 */
void Clay_Ncurses_ResumeAfterSuspend(void);

#endif // POSTOFFICE_RENDERER_CLAY_NCURSES_RENDERER_H
