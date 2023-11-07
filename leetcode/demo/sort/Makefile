CC = gcc
TARGET = sort_test
LIBS = -lpthread
CLFAGS = -g
DIRS = .
FILES = $(foreach dir, $(DIRS), $(wildcard $(dir)/*.c))
OBJ = $(patsubst %.c,%.o, $(FILES))

$(TARGET):$(OBJ)
	$(CC) -o $@ $^ $(CLFAGS) $(LIBS) 

$(OBJ):%.o:%.c
	$(CC) -c $< -o $@ $(CLFAGS)

clean:
	rm -rf $(OBJ) $(TARGET)

