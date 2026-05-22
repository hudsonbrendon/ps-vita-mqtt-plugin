/* Newlib's _exit (libc.a) drags in a reference to _free_vita_newlib that
 * normally lives in SceLibc_stub. We don't link SceLibc (the heap-backed
 * libc) to avoid the load-time crash it causes in SceShell-loaded plugins.
 * The plugin never calls _exit in practice, so a no-op stub satisfies the
 * unresolved reference. */
void _free_vita_newlib(void) {}
