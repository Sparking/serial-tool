COMPILER_PREFIX=x86_64-w64-mingw32-
CC = $(COMPILER_PREFIX)gcc

RM=del

CFLAGS = -g -O0
CPPFLAGS = -std=c99 -I..
LDFLAGS = $(CFLAGS)
LIBS = -ladvapi32 -lcomdlg32 -lgdi32 -limm32 \
		-lole32 -lshell32 -luser32

target = winser.exe
objs = prog_entry.o winser.o winser_handle.o

.PHONY: all
all: $(target)
$(target): $(objs)
	$(CC) $(LDFLAGS) -o $@ $(objs) $(LIBS)
$(objs): %.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
.PHONY: clean
clean:
	-$(RM) $(objs)
	-$(RM) $(target)