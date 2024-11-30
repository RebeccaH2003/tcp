CC = gcc
CFLAGS = -Wall -pthread

OUTPUT_DIR = output
TARGET = server
OBJS = $(OUTPUT_DIR)/server.o $(OUTPUT_DIR)/cJSON.o

.PHONY: all
all: $(TARGET)

# Create output directory if it doesn't exist
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

# Link object files to create the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# Compile .c files into .o files
$(OUTPUT_DIR)/server.o: server.c ../libs/cJSON.h | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT_DIR)/cJSON.o: ../libs/cJSON.c ../libs/cJSON.h | $(OUTPUT_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)