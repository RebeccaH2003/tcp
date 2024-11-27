# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Targets and source files
TARGET = tcp_program
SRCS = tcp_main.c tcp_helper.c
HEADERS = tcp_helper.h
OBJS = $(SRCS:.c=.o)

# Build the program
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile source files into object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)
