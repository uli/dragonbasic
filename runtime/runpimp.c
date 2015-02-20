#include <pimpmobile.h>

/* This is executed before the DragonBASIC runtime. */
int main()
{
  /* This is not strictly necessary, but if we don't use the pimpmobile
     library here, it will be eliminated as dead code... */
  pimp_close();
  /* Jump to DragonBASIC runtime. */
  asm("b 0x80000e0");
}
