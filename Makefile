TARGET_NAME=rosewm-dispatcher
BUILD_DIR=build

CPP = g++

CFLAGS =\
 -Wall -Wextra -O2 -Wno-missing-field-initializers -std=c++20\
 $(shell pkg-config --cflags asio) \
 $(shell pkg-config --cflags freetype2) \
 $(shell pkg-config --cflags fribidi) \
 $(shell pkg-config --cflags sdl2)

CLIBS =\
 $(shell pkg-config --libs asio) \
 $(shell pkg-config --libs freetype2) \
 $(shell pkg-config --libs fribidi) \
 $(shell pkg-config --libs sdl2)

FILES_SRC = $(sort $(wildcard src/*.cc))
OBJECT_FILES_SRC =\
 $(patsubst src/%.cc,$(BUILD_DIR)/%.o,$(FILES_SRC))

rosewm-dispatcher: $(OBJECT_FILES_SRC)
	$(CPP) $(CFLAGS) $^ $(CLIBS) -o $(BUILD_DIR)/$@

clean:
	rm -f $(BUILD_DIR)/$(TARGET_NAME)
	rm -f $(BUILD_DIR)/*.o

install:
	cp $(BUILD_DIR)/$(TARGET_NAME) /usr/local/bin/$(TARGET_NAME)

uninstall:
	rm /usr/local/bin/$(TARGET_NAME)

$(BUILD_DIR)/%.o: src/%.cc
	$(CPP) $(CFLAGS) -c -o $@ $<
