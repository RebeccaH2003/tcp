CC = gcc
CFLAGS = -Wall -pthread -Ilibs

TARGET = main
SRCS = sliding_main.c sliding_helper.c libs/cJSON.c
OBJS_DIR = objects
OBJS = $(OBJS_DIR)/sliding_main.o $(OBJS_DIR)/sliding_helper.o $(OBJS_DIR)/cJSON.o

# Default target
all: $(TARGET)

# Build target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# Compile source files into object files
$(OBJS_DIR)/sliding_main.o: sliding_main.c | $(OBJS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS_DIR)/sliding_helper.o: sliding_helper.c | $(OBJS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJS_DIR)/cJSON.o: libs/cJSON.c | $(OBJS_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create objects directory if it doesn't exist
$(OBJS_DIR):
	mkdir -p $(OBJS_DIR)

# Clean target
clean:
	rm -rf $(OBJS_DIR) $(TARGET)

.PHONY: all clean


