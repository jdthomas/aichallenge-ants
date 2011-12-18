
all: C_BOT

.PHONY: C_BOT

C_BOT:
	cd Bots/c/ && make

clean:
	cd Bots/c/ && make clean
