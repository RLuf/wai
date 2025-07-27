# Makefile for building start-manager wrapper

CC := gcc
CFLAGS := -Wall -O2

all: start-manager

start-manager: start-manager.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f start-manager
