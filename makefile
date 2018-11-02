all: a2sdn FIFO

clean:
	rm -rf a2sdn submit.tar

tar:
	tar -czf submit.tar a2sdn.cpp makefile report.pdf 
		 
compile:
	gcc a2sdn.c -o a2sdn

FIFO:
	
