# include <stdio.h>
# include <stdlib.h>
# include <poll.h>
# include <signal.h>
# include <string.h>

# define MAX_NSW 7；
# define MAX_IP 1000;
# define MIN_PRI 4;

struct swInfo{
    char name[5];
    int port1;
    int port2;
    int IPlo;
    int IPhi;
}

void user1Handler(int signum);
void controller(int numSwitch);

int main(int argc, char* argv[]) {
    
    /* Bound singal USER1 with handler */
    singal(SIGUSR1, user1Handler);

    char name[50];
    strcmp(name, argv[1]);

    if (stcmp(argv[1], "cont") == 0 && argc == 3) {
        /*controler mode*/

        if (argv[2] > MAX_NSW) {
            printf("Too much Switchs"); // check number of switches
            return -2;
        }
        controller(argv[2]);

    } else if (argc == 6 && name[0] == "s" && name[1] == "w") {

    } else {
        printf("Invalid Command\n");
        return -1;
    }
    return 0;
}

void user1Handler(int signum) {}

void controller(int numSwitch) {
    int openCounter = 0;
    int queryCounter = 0;
    int ackCounter = 0;
    int addCounter = 0;


}