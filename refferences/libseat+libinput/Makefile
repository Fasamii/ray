CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
PKGCONFIG = pkg-config

# Package dependencies
PACKAGES = libinput libseat libudev

# Get compiler flags from pkg-config
CFLAGS += $(shell $(PKGCONFIG) --cflags $(PACKAGES))
LDFLAGS += $(shell $(PKGCONFIG) --libs $(PACKAGES))

TARGET = input_reader
SOURCE = input_reader.c

.PHONY: all clean install deps-arch deps-ubuntu deps-fedora

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

# Dependency installation helpers for different distros
deps-arch:
	sudo pacman -S libinput libseat libudev0

deps-ubuntu:
	sudo apt-get update
	sudo apt-get install libinput-dev libseat-dev libudev-dev pkg-config

deps-fedora:
	sudo dnf install libinput-devel libseat-devel systemd-devel pkgconfig

# Check if dependencies are available
check-deps:
	@echo "Checking dependencies..."
	@$(PKGCONFIG) --exists $(PACKAGES) && echo "✓ All dependencies found" || \
	(echo "✗ Missing dependencies. Install with:" && \
	 echo "  Arch Linux: make deps-arch" && \
	 echo "  Ubuntu/Debian: make deps-ubuntu" && \
	 echo "  Fedora: make deps-fedora")

help:
	@echo "Available targets:"
	@echo "  all          - Build the input reader"
	@echo "  clean        - Remove built files"
	@echo "  install      - Install to /usr/local/bin"
	@echo "  check-deps   - Check if dependencies are installed"
	@echo "  deps-arch    - Install dependencies on Arch Linux"
	@echo "  deps-ubuntu  - Install dependencies on Ubuntu/Debian"
	@echo "  deps-fedora  - Install dependencies on Fedora"
