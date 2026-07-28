/****************************************************************************
 * VelaGuard boot entry: start the guardian automatically, then retain NSH.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/sched.h>

#include <stdio.h>

extern int velaguard_main(int argc, char *argv[]);
extern int nsh_main(int argc, char *argv[]);

int velaguard_boot_main(int argc, char *argv[])
{
  int pid;

  pid = task_create("velaguard",
                    CONFIG_CONTEST2026_148_VELAGUARD_PRIORITY,
                    CONFIG_CONTEST2026_148_VELAGUARD_STACKSIZE,
                    velaguard_main, NULL);
  if (pid < 0)
    {
      printf("VelaGuard: automatic startup failed: %d\n", pid);
    }
  else
    {
      printf("VelaGuard: automatic startup task=%d\n", pid);
    }

  /* Keep the shell available for IMU tests and field diagnostics. */

  return nsh_main(argc, argv);
}
