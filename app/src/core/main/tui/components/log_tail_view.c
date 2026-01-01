#include "log_tail_view.h"
#include "../tui_state.h"
#include <clay/clay.h>

#include <renderer/clay_ncurses_renderer.h>
#include <utils/files.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOG_LINES 100
#define BUFFER_SIZE 4096

void tui_RenderLogTailView(const char* filename) {
    char path[256];
    snprintf(path, sizeof(path), "logs/%s", filename);

    FILE *f = fopen(path, "r");
    if (!f) {
        CLAY_TEXT(CLAY_STRING("Unable to open log file."), CLAY_TEXT_CONFIG({.textColor = {255, 100, 100, 255}}));
        return;
    }

    // Seek to end and backup to read last N bytes (approx logic)
    // OR use the logReadOffset if set
    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    // Initialize offset if -1 (tail)
    if (g_tuiState.logReadOffset == -1) {
        // Calculate visible lines based on screen height
        // Screen Height (cells) = dims.height / TUI_CH
        // Overhead approx: TopBar(3) + Tabs(3) + Borders/Padding(4) + BottomBar(3) = ~13
        Clay_Dimensions dims = Clay_Ncurses_GetLayoutDimensions();
        int totalLines = (int)(dims.height / TUI_CH);
        int visibleLines = totalLines - 14; 
        if (visibleLines < 5) visibleLines = 5;

        // Read a chunk from the end to find the start of the last 'visibleLines'
        long seekStart = size - BUFFER_SIZE;
        if (seekStart < 0) seekStart = 0;

        char scanBuffer[BUFFER_SIZE];
        fseek(f, seekStart, SEEK_SET);
        size_t bytesRead = fread(scanBuffer, 1, BUFFER_SIZE, f);

        int linesFound = 0;
        long newOffset = size; 

        // Scan backwards from end of buffer
        for (long i = (long)bytesRead - 1; i >= 0; i--) {
            if (scanBuffer[i] == '\n') {
                linesFound++;
                if (linesFound >= visibleLines) {
                    newOffset = seekStart + i + 1; // Start after the newline
                    break;
                }
            }
        }

        if (linesFound < visibleLines && seekStart == 0) {
             newOffset = 0; // File is smaller than visible area
        } else if (linesFound < visibleLines) {
            // If we shouldn't find enough lines, validly default to seekStart
            newOffset = seekStart;
        }

        g_tuiState.logReadOffset = newOffset;
    }

    // Clamp offset
    if (g_tuiState.logReadOffset > size) g_tuiState.logReadOffset = size;

    long startPos = g_tuiState.logReadOffset;
    long readSize = BUFFER_SIZE;
    if (startPos + readSize > size) {
        readSize = size - startPos;
    }

    static char buffer[BUFFER_SIZE + 1];
    if (readSize > 0) {
        fseek(f, startPos, SEEK_SET);
        size_t bytesRead = fread(buffer, 1, (size_t)readSize, f);
        buffer[bytesRead] = '\0';
    } else {
        buffer[0] = '\0';
    }
    fclose(f);

    CLAY(CLAY_ID("LogViewScroll"),
         {.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 0},
          .clip = {.horizontal = true, .vertical = true, .childOffset = {.x = g_tuiState.logScrollPosition.x, .y = 0}}}) {

        char *line = strtok(buffer, "\n");
        bool first = true;

        while (line) {
            if (first && startPos > 0) {
                first = false; 
                // skip potentially cut-off line
            } else {
                CLAY_TEXT(CLAY_STRING_DYN(line), CLAY_TEXT_CONFIG({.textColor = {200, 200, 200, 255}}));
            }
            line = strtok(NULL, "\n");
        }
    }
}
