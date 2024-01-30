CC = gcc
CFLAGS = -march=native -pipe -std=c17 -Wall

all: smon

config-profile:
ifeq ($(profile), debug)
	@echo "Builing debug version..."
	$(eval CFLAGS += -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -O0 -g -fsanitize=address -fsanitize=undefined)
else
	@echo "Building release version..."
	$(eval CFLAGS += -flto -O3 -s)
endif

smon: config-profile smon.c
	$(CC) $(CFLAGS) $(filter-out $<,$^) $(LDFLAGS) -o $@

install: smon
	mv smon /usr/local/bin

clean:
	rm smon
