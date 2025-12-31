#ifndef LOG_TAIL_VIEW_H
#define LOG_TAIL_VIEW_H

/**
 * @brief Renders the last N lines of a log file.
 * 
 * @param filename Name of the file in the logs/ directory.
 */
void tui_RenderLogTailView(const char* filename);

#endif // LOG_TAIL_VIEW_H
