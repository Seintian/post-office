#ifndef DIRECTOR_CLEANUP_H
#define DIRECTOR_CLEANUP_H

#include "director_config.h"

/**
 * @brief Cleanup all resources used by the director.
 * @param cfg Configuration used (to check for headless etc).
 */
void director_cleanup(const director_config_t *cfg);

#endif
