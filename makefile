all: clean a2sdn FIFO

clean:
	rm -rf a2sdn submit.tar fifo-0-1 fifo-1-0 mkfifo fifo-2-0 mkfifo fifo-0-2 mkfifo fifo-0-3 mkfifo fifo-3-0 mkfifo fifo-1-2 mkfifo fifo-2-1 mkfifo fifo-2-3 mkfifo fifo-3-2

tar:
	tar -czf submit.tar a2sdn.cpp makefile report.pdf 
		 
compile:
	gcc a2sdn.c -o a2sdn

FIFO:
	mkfifo fifo-0-1
	mkfifo fifo-1-0
	mkfifo fifo-2-0
	mkfifo fifo-0-2
	mkfifo fifo-0-3
	mkfifo fifo-3-0
	mkfifo fifo-1-2
	mkfifo fifo-2-1
	mkfifo fifo-2-3
	mkfifo fifo-3-2
