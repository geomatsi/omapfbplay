-include $(or $(CONFIG),$(ARCH),$(shell uname -m)).mk

$(if $(findstring y,$(OMAPFB) $(XV)),,$(error No display drivers enabled))

override O := $(and $(O),$(O:%/=%)/)

SYSROOT = $(addprefix --sysroot=,$(ROOT))

CC = $(CROSS_COMPILE)gcc

CPPFLAGS = $(SYSROOT)
CPPFLAGS += $(and $(LINUX),-I$(LINUX)/include)
CPPFLAGS += $(and $(FFMPEG),-I$(FFMPEG))

CFLAGS = -O3 -g -Wall -fomit-frame-pointer $(CPUFLAGS)

LDFLAGS = $(SYSROOT)
LDFLAGS += $(and $(FFMPEG),-L$(FFMPEG)/libavcodec -L$(FFMPEG)/libavformat -L$(FFMPEG)/libavutil)
LDLIBS = -lavformat -lavcodec -lavutil -lm -lz -lbz2 -lpthread -lrt

DRV-y                    = sysclk.o
DRV-$(NETSYNC)          += netsync.o
DRV-$(OMAPFB)           += omapfb.o yuv.o
DRV-$(XV)               += xv.o

LDLIBS-$(XV)            += -lXv -lXext -lX11

LDLIBS += $(LDLIBS-y)

OBJ = omapfbplay.o time.o magic-head.o $(DRV-y) magic-tail.o

$(O)omapfbplay: $(addprefix $(O),$(OBJ))

$(O)%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $^

$(O)%.o: %.S
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c -o $@ $^

clean:
	rm -f $(O)*.o $(O)omapfbplay
