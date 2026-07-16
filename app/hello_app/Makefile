############################################################################
# Contest 2026 team 148 VelaGuard - Makefile
############################################################################

include $(APPDIR)/Make.defs

PROGNAME  = velaguard
PRIORITY  = $(CONFIG_CONTEST2026_148_VELAGUARD_PRIORITY)
STACKSIZE = $(CONFIG_CONTEST2026_148_VELAGUARD_STACKSIZE)
MODULE    = $(CONFIG_LVX_USE_DEMO_CONTEST2026_148_VELAGUARD)

MAINSRC = core$(DELIM)velaguard_main.c
CSRCS = algorithm$(DELIM)velaguard_fall.c
CSRCS += sensors$(DELIM)velaguard_imu.c
CSRCS += ui$(DELIM)velaguard_font_18.c

CFLAGS += -DSOC_BF0_HCPU
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)$(DELIM)packages$(DELIM)demos$(DELIM)contest2026_148_hello_app$(DELIM)core
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)$(DELIM)packages$(DELIM)demos$(DELIM)contest2026_148_hello_app$(DELIM)algorithm
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)$(DELIM)packages$(DELIM)demos$(DELIM)contest2026_148_hello_app$(DELIM)sensors
CFLAGS += ${INCDIR_PREFIX}$(APPDIR)$(DELIM)packages$(DELIM)demos$(DELIM)contest2026_148_hello_app$(DELIM)ui
CFLAGS += ${INCDIR_PREFIX}$(BOARD_DIR)$(DELIM)src
CFLAGS += ${INCDIR_PREFIX}$(BOARD_DIR)$(DELIM)include
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)..$(DELIM)vendor$(DELIM)sifli$(DELIM)chips$(DELIM)drivers$(DELIM)Include
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)..$(DELIM)vendor$(DELIM)sifli$(DELIM)chips$(DELIM)drivers$(DELIM)cmsis$(DELIM)Include
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)..$(DELIM)vendor$(DELIM)sifli$(DELIM)chips$(DELIM)drivers$(DELIM)cmsis$(DELIM)sf32lb52x
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)..$(DELIM)vendor$(DELIM)sifli$(DELIM)chips$(DELIM)external$(DELIM)CMSIS$(DELIM)Include
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)..$(DELIM)vendor$(DELIM)sifli$(DELIM)chips$(DELIM)sf32lb52
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)..$(DELIM)vendor$(DELIM)sifli$(DELIM)chips$(DELIM)sf32lb52$(DELIM)include
CFLAGS += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)..$(DELIM)vendor$(DELIM)sifli$(DELIM)chips$(DELIM)boards$(DELIM)include

include $(APPDIR)/Application.mk
