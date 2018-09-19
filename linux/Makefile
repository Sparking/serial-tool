CC = gcc
LD = gcc

CFLAGS = -O0 -g
CPPFLAGS = -Wall -Werror -std=gnu99 -I../ -I../common/
LDFLAGS = $(CFLAGS)
LIBS = -lpthread

target = uxser
objs = uxser.o uxser_entry.o uxser_handler.o

.PHONY: all
all: $(target)
$(target): $(objs)
	@echo  "    OUTPUT	$(target)"
	@$(LD) $(LDFLAGS) -o $@ $(objs) $(LIBS)
$(objs): %.o: %.c
	@echo  "      CC	$@"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<
.PHONY: clean
clean:
	$(RM) $(objs)
	$(RM) $(target)
