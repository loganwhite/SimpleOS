#ifndef _INIT_H__
#define _INIT_H__

#include <debug.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Page directory with kernel mappings only. */

extern uint32_t *init_page_dir;

#endif /* End of init.h */
