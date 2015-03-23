/* pimp_debug.h -- Debugging helpers
 * Copyright (C) 2005-2006 Jørn Nystad and Erik Faye-Lund
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef PIMP_DEBUG_H
#define PIMP_DEBUG_H

/* current debug level */
#define DEBUG_LEVEL 100

/* some standard debug levels */
#define DEBUG_LEVEL_INFO    10
#define DEBUG_LEVEL_WARNING 50
#define DEBUG_LEVEL_ERROR   100

#ifdef DEBUG_PRINT_ENABLE
 #define DEBUG_PRINT(debug_level, X) do { if (DEBUG_LEVEL <= debug_level) iprintf X; } while(0)
#else
 #define DEBUG_PRINT(debug_level, X)
#endif

#ifdef ASSERT_ENABLE
 #include <stdlib.h>
 #include <stdio.h>
 #define ASSERT(expr) \
	do {              \
		if (!(expr)) printf("*** ASSERT FAILED %s AT %s:%i\n", #expr, __FILE__, __LINE__); \
	} while(0)
#else
 #define ASSERT(expr)
#endif

#endif /* PIMP_DEBUG_H */
