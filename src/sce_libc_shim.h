/* Vita-side shim: redirect libc string/print functions to SceLibKernel's
 * sceClib* equivalents so we can drop the SceLibc_stub dependency.
 * Plugins loaded into SceShell without crt0 (-nostartfiles) can't init
 * the newlib heap, so any libc call that needs malloc (snprintf etc.)
 * crashes silently on load. */
#ifndef PSVITA_SCE_LIBC_SHIM_H
#define PSVITA_SCE_LIBC_SHIM_H

#ifdef PSVITA_BUILD
#include <psp2/kernel/clib.h>
#define snprintf  sceClibSnprintf
#define vsnprintf sceClibVsnprintf
#endif

#endif
