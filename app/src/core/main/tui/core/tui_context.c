#include "tui_context.h"
#include <clay/clay.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @file tui_context.c
 * @brief Implementation of TUI Context lifecycle management.
 */

tui_context_t* tui_context_create(size_t arena_size) {
    tui_context_t* ctx = (tui_context_t*)malloc(sizeof(tui_context_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(tui_context_t));

    // Initialize defaults
    ctx->current_screen = TUI_SCREEN_SIMULATION;
    ctx->is_running = true;

    // Initialize Arena
    // Note: Clay_CreateArenaWithCapacityAndMemory expects a raw pointer.
    // In valid C, we handle allocation here.
    void* memory = malloc(arena_size);
    if (!memory) {
        free(ctx);
        return NULL;
    }
    ctx->arena = Clay_CreateArenaWithCapacityAndMemory(arena_size, memory);

    return ctx;
}

void tui_context_destroy(tui_context_t* ctx) {
    if (!ctx) return;

    // Free arena memory (Clay doesn't own the pointer itself in that sense, we passed it in)
    // internalArena.memory is the pointer.
    if (ctx->arena.memory) {
        free(ctx->arena.memory);
    }

    free(ctx);
}

void tui_context_update(tui_context_t* ctx) {
    if (!ctx) return;
    // Per-frame updates if needed
    // e.g. decaying error messages or resetting frame counters
}
