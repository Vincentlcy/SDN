
# include <stdio.h>
# include <stdlib.h>
# include <poll.h>
# include <unistd.h>
# include <stdarg.h>
# include <assert.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <sys/types.h>
# include <signal.h>
# include <string.h>
# include <malloc.h>


# define MAX_IP 1000;
# define MIN_PRI 4;


typedef enum {ACK, OPEN, QUERY, ADD, RELAY} KIND; // kind of message from lab

// Messages structs
typedef struct {
    int swID;
    int port1;
    int port2;
    int IPlo;
    int Iphi;
} MSG_OPEN;

typedef struct {
    int swID;
    int dstIP;
    int srcIP;
    int port1;
    int port2;
} MSG_QUERY;

typedef struct {
    int swID;
    int dstIP;
    int dstIPlo;
    int dstIPhi;
    int srcIP;
    int action;
    int actionVal;
    int pri;
} MSG_ADD;

typedef struct{
    int srcIP;
    int dstIP;
} MDG_RELAY;

// FRAME struct for trans data
typedef union { MSG_OPEN mOpen; MSG_QUERY mQuery; MSG_ADD mAdd; MDG_RELAY mRelay;} MSG;
typedef struct { KIND kind; MSG msg; } FRAME;

// Base switch infomation work as parameter in the code
typedef struct {
    int swID;
    int port1;
    int port2;
    int IPlo;
    int Iphi;
} SwitchInfo;

//typedef struct {
//    int fd;        /* file descriptor */
//    short events;     /* requested events */
//    short revents;    /* returned events */
//}pollfd;

// Base flowtable infomation for switch
typedef struct {
    int srcIPlo;
    int srcIPhi;
    int dstIPlo;
    int dstIPhi;
    int actionType;
    int actionVal;
    int pri;
    int pktCount;
} FlowTable;

// Base counter infomation as parameter for switchPrint()
typedef struct {
    int admitCounter;
    int ackCounter;
    int addRuleCounter;
    int relayInCounter;
    int relayOutCounter;
    int openCounter;
    int queryCounter;
} SwitchCounter;

// base Controller information for USER1 signal handler
typedef struct {
    SwitchInfo switch_list[7];
    int numSwitch;
    int openCounter;
    int queryCounter;
    int ackCounter;
    int addCounter;
} CON;

// base Switch information for USER1 signal handler
typedef struct {
    FlowTable *flows;
    int numFlowTable;
    SwitchCounter swCounter;
} SW;

void user1Controller(int signum);
void user1Switch(int signum);
int executeswitch(SwitchInfo sw, char filename[]);
int switchAction(FlowTable flows[],int srcIP,int dstIP,int numFlowTable);
int printSwitch(FlowTable flows[],int numFlowTable,SwitchCounter swCounter);
MSG controllerRule(MSG_QUERY query, SwitchInfo switch_list[], int numSwitch);
int controller(int numSwitch);
int printController(SwitchInfo switch_list[],int numSwitch,int openCounter,int queryCounter,int ackCounter,int addCounter);
void FATAL (const char *fmt, ... );
int openFIFO(int sender, int revsiver);
FRAME rcvFrame (int fd);
void WARNING (const char *fmt, ... );
void sendFrame (int fd, KIND kind, MSG *msg);

// for USER1 handler
CON Con;
SW Sw;
// since marco do not work well in C, insteadlly using global variable
const int FORWARD = 1;
const int DROP = 0;

int main(int argc, char* argv[]) {

    char name[50];
    strcpy(name, argv[1]);

    if (strcmp(argv[1], "cont") == 0 && argc == 3) {
        /*controler mode*/
	char temp[20];
        strcpy(temp, argv[2]);
	if (atoi(&temp[0]) >= 7 ||atoi(&temp[0])<1) {
            printf("Too much Switchs"); // check number of switches
            return -2;
        }

        /* Bound singal USER1 with handler */
        signal(SIGUSR1, user1Controller);
 		
        controller(atoi(&temp[0]));

    } else if (argc == 6 && strncmp(&name[0],"s",1)==0 && strncmp(&name[1],"w",1)==0) {
        /*switch mode*/
        SwitchInfo sw;
        sw.swID = atoi(&name[2]);
         
        char filename[50];
        strcpy(filename, argv[2]);

        // get the port of the switch
        if (strcmp(argv[3],"null")==0) {
            sw.port1 = -1;
        } else {
            sw.port1 = atoi(&argv[3][2]);
        }

        if (strcmp(argv[4],"null")==0) {
            sw.port2 = -1;
        } else {
            sw.port2 = atoi(&argv[4][2]);
        }

        // get the IP range
        char *temp;
        temp = strtok(argv[5], "-");
        sw.IPlo = atoi(temp);
        temp = strtok(NULL, "-");
        sw.Iphi = atoi(temp);

        /* Bound singal USER1 with handler */
        signal(SIGUSR1, user1Switch);

        executeswitch(sw, filename);

    } else {
        printf("Invalid Command\n");
        return -1;
    }
    return 0;
}

void user1Controller(int signum) {
    // handle the user1 singals when in controller mode
    printf("\n");
    printf("\nUSER1 singal received \n");
    printController(Con.switch_list, Con.numSwitch, Con.openCounter, Con.queryCounter, Con.ackCounter, Con.addCounter);
    return;
}

void user1Switch(int signum) {
    // handle the user1 singals when in switch mode
    printf("\n");
    printf("\nUSER1 singal received \n");
    printSwitch(Sw.flows, Sw.numFlowTable, Sw.swCounter);
    return;
}

int executeswitch(SwitchInfo sw, char filename[]) {
    FILE *filefp;
    filefp = fopen(filename, "r");
    if (filefp == NULL) {
        printf("Fail to read file");
        return -1;
    }

    FlowTable flows[50];

    flows[0].srcIPlo = 0;
    flows[0].srcIPhi = MAX_IP;
    flows[0].dstIPhi = sw.Iphi;
    flows[0].dstIPlo = sw.IPlo;
    flows[0].actionType = FORWARD;
    flows[0].actionVal = 3;
    flows[0].pri = 4;
    flows[0].pktCount = 0;

    int admitCounter = 0;
    int ackCounter = 0;
    int addRuleCounter = 0;
    int relayInCounter =0;
    int openCounter = 0;
    int queryCounter = 0;
    int relayOutCounter = 0;
    int numFlowTable = 1;

    int fdConRead = openFIFO(0, sw.swID);
    int fdConWrite = openFIFO(sw.swID, 0);
    int fdPort1Read;
    int fdPort1Write;
    int fdPort2Read;
    int fdPort2Write;
    if (sw.port1 != -1) {
        fdPort1Read = openFIFO(sw.port1, sw.swID);
        fdPort1Write = openFIFO(sw.swID, sw.port1);
    } else if (sw.port2 != -1) {
        fdPort2Read = openFIFO(sw.port2, sw.swID);
        fdPort2Write = openFIFO(sw.swID, sw.port2);
    }

    // send open message to Controller
    MSG msg;
    msg.mOpen.swID = sw.swID;
    msg.mOpen.port1 = sw.port1;
    msg.mOpen.port2 = sw.port2;
    msg.mOpen.IPlo = sw.IPlo;
    msg.mOpen.Iphi = sw.Iphi;

    sendFrame(fdConWrite, OPEN, &msg);
    openCounter += 1;
    printf("\nTransmitted (src= sw%d, dest= cont) [OPEN]:\n", sw.swID);
    printf("    port0= cont, port1= %d, port2= %d, port3= %d-%d\n", sw.port1, sw.port2, sw.IPlo, sw.Iphi);

    while (1) {
        int aimSwith = 0;
        int srcIP = 0;
        int dstIP = 0;

        struct pollfd fifo[3];
        fifo[0].fd = fdConRead;
        fifo[0].events = POLLIN;
        fifo[1].fd = fdPort1Read;
        fifo[1].events = POLLIN;
        fifo[2].fd = fdPort2Read;
        fifo[2].events = POLLIN;
        poll(fifo, 3, 0);

        // read from controller
        if (fifo[0].revents & POLLIN) {
            FRAME frame;
            frame = rcvFrame(fifo[0].fd);
	        fifo[0].revents = -1;

            if (frame.kind == ACK) {
                ackCounter += 1;
		        printf("\nReceived (src= cont, dest= sw%d) [ACK]\n", sw.swID);
            }

            if (frame.kind == ADD) {
                numFlowTable++;
                addRuleCounter++;

                flows[numFlowTable-1].srcIPlo = 0;
                flows[numFlowTable-1].srcIPhi = MAX_IP;
                flows[numFlowTable-1].dstIPlo = frame.msg.mAdd.dstIPlo;
                flows[numFlowTable-1].dstIPhi = frame.msg.mAdd.dstIPhi;
                flows[numFlowTable-1].actionType = frame.msg.mAdd.action;
                flows[numFlowTable-1].actionVal = frame.msg.mAdd.actionVal;
                flows[numFlowTable-1].pri = frame.msg.mAdd.pri;
                flows[numFlowTable-1].pktCount = 0;

                printf("\nReceived (src= cont, dest= sw%d) [ADD]:\n", sw.swID);
                printf("    (srcIP= 0-%d, destIP= %d-%d, ", flows[numFlowTable-1].srcIPhi, flows[numFlowTable-1].dstIPlo, flows[numFlowTable-1].dstIPhi);
                if (flows[numFlowTable].actionType == FORWARD) {
                    printf("action= FORWARD:%d, pri= %d, pktCount= %d\n", flows[numFlowTable-1].actionVal, flows[numFlowTable-1].pri, flows[numFlowTable-1].pktCount);
                } else {
                    printf("action= DROP:%d, pri= %d, pktCount= %d\n", flows[numFlowTable-1].actionVal, flows[numFlowTable-1].pri, flows[numFlowTable-1].pktCount);
                }

                int n = switchAction(flows, frame.msg.mAdd.srcIP, frame.msg.mAdd.dstIP, numFlowTable);
                admitCounter++;
                if (n > 0) {
                    relayOutCounter++;
                    MSG msg;
                    msg.mRelay.srcIP = frame.msg.mAdd.srcIP;
                    msg.mRelay.dstIP = frame.msg.mAdd.dstIP;
                    if (n==1) {
                        sendFrame(fdPort1Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port1, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    } else if (n==2) {
                        sendFrame(fdPort2Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port2, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    }
                } else if (n == -1) {
                    // need to query
                    queryCounter++;
                    MSG msg;
                    msg.mQuery.swID = sw.swID;
                    msg.mQuery.dstIP = dstIP;
                    msg.mQuery.srcIP = srcIP;
                    msg.mQuery.port1 = sw.port1;
                    msg.mQuery.port2 = sw.port2;
                    sendFrame(fdConWrite, QUERY, &msg);
                    printf("\nTransmitted (src= sw%d, dest= cont)[QUERY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, srcIP, dstIP);
                } 
            }
        }

        // Poll from keyboard
        // http://blog.51cto.com/wait0804/1856818
        struct pollfd keyboard[1];
        keyboard[0].fd = STDIN_FILENO;
        keyboard[0].events = POLLIN;

        poll(keyboard, 1, 0);
        char userCmd[50];
        if ((keyboard[0].revents & POLLIN)) {
            read(STDIN_FILENO, userCmd, 4*sizeof(char));
	        keyboard[0].revents = -1;
        }

        SwitchCounter swCounter;
        swCounter.admitCounter = admitCounter;
        swCounter.ackCounter = ackCounter;
        swCounter.addRuleCounter = addRuleCounter;
        swCounter.relayInCounter = relayInCounter;
        swCounter.relayOutCounter = relayOutCounter;
        swCounter.openCounter = openCounter;
        swCounter.queryCounter = queryCounter;

        Sw.swCounter = swCounter;
        Sw.flows = flows;
        Sw.numFlowTable = numFlowTable;

        // run user cmd
        if (strcmp(userCmd,"list")==0) {
            printSwitch(flows, numFlowTable, swCounter);

        } else if (strcmp(userCmd,"exit")==0) {
            printSwitch(flows, numFlowTable, swCounter);
            return 0;
        }

        // read from file
        char line[100];
        if (fgets(line, 100, filefp)!=NULL) {
            if (strcmp(&line[0], "#")==0 || line[0] == '\0'|| line[0] == '\n') {}
            else {
                char *temp;
                temp = strtok(line, " ");
                aimSwith = atoi(&temp[2]);
                temp = strtok(NULL, " ");
                srcIP = atoi(temp);
                temp = strtok(NULL, " ");
                dstIP = atoi(temp);
            }
        }

        printf("%d %d %d\n", aimSwith, srcIP, dstIP);

        if (aimSwith == sw.swID) {
            printf("relay: %d", n);
            int n = switchAction(flows, srcIP, dstIP, numFlowTable);
            admitCounter++;
            if (n > 0) {
                relayOutCounter++;
                MSG msg;
                msg.mRelay.srcIP = srcIP;
                msg.mRelay.dstIP = dstIP;
                if (n==1) {
                    sendFrame(fdPort1Write, RELAY, &msg);
                } else if (n==2) {
                    sendFrame(fdPort2Write, RELAY, &msg);
                }
            } else if (n == -1) {
                // need to query
                queryCounter++;
                MSG msg;
                msg.mQuery.swID = sw.swID;
                msg.mQuery.dstIP = dstIP;
                msg.mQuery.srcIP = srcIP;
                msg.mQuery.port1 = sw.port1;
                msg.mQuery.port2 = sw.port2;
                sendFrame(fdConWrite, QUERY, &msg);
                printf("\nTransmitted (src= sw%d, dest= cont)[QUERY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, srcIP, dstIP);
            } 
        }
        aimSwith = 0; // reset 


        poll(fifo, 3, 0);

        // read from controller
        if (fifo[0].revents & POLLIN) {
            FRAME frame;
            frame = rcvFrame(fifo[0].fd);
	        fifo[0].revents = -1;

            if (frame.kind == ACK) {
                ackCounter += 1;
		        printf("\nReceived (src= cont, dest= sw%d) [ACK]\n", sw.swID);
            }

            if (frame.kind == ADD) {
                numFlowTable++;
                addRuleCounter++;

                flows[numFlowTable-1].srcIPlo = 0;
                flows[numFlowTable-1].srcIPhi = MAX_IP;
                flows[numFlowTable-1].dstIPlo = frame.msg.mAdd.dstIPlo;
                flows[numFlowTable-1].dstIPhi = frame.msg.mAdd.dstIPhi;
                flows[numFlowTable-1].actionType = frame.msg.mAdd.action;
                flows[numFlowTable-1].actionVal = frame.msg.mAdd.actionVal;
                flows[numFlowTable-1].pri = frame.msg.mAdd.pri;
                flows[numFlowTable-1].pktCount = 0;

                printf("\nReceived (src= cont, dest= sw%d) [ADD]:\n", sw.swID);
                printf("    (srcIP= 0-%d, destIP= %d-%d, ", flows[numFlowTable-1].srcIPhi, flows[numFlowTable-1].dstIPlo, flows[numFlowTable-1].dstIPhi);
                if (flows[numFlowTable].actionType == FORWARD) {
                    printf("action= FORWARD:%d, pri= %d, pktCount= %d\n", flows[numFlowTable-1].actionVal, flows[numFlowTable-1].pri, flows[numFlowTable-1].pktCount);
                } else {
                    printf("action= DROP:%d, pri= %d, pktCount= %d\n", flows[numFlowTable-1].actionVal, flows[numFlowTable-1].pri, flows[numFlowTable-1].pktCount);
                }

                int n = switchAction(flows, frame.msg.mAdd.srcIP, frame.msg.mAdd.dstIP, numFlowTable);
                admitCounter++;
                if (n > 0) {
                    relayOutCounter++;
                    MSG msg;
                    msg.mRelay.srcIP = frame.msg.mAdd.srcIP;
                    msg.mRelay.dstIP = frame.msg.mAdd.dstIP;
                    if (n==1) {
                        sendFrame(fdPort1Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port1, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    } else if (n==2) {
                        sendFrame(fdPort2Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port2, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    }
                } else if (n == -1) {
                    // need to query
                    queryCounter++;
                    MSG msg;
                    msg.mQuery.swID = sw.swID;
                    msg.mQuery.dstIP = dstIP;
                    msg.mQuery.srcIP = srcIP;
                    msg.mQuery.port1 = sw.port1;
                    msg.mQuery.port2 = sw.port2;
                    sendFrame(fdConWrite, QUERY, &msg);
                    printf("\nTransmitted (src= sw%d, dest= cont)[QUERY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, srcIP, dstIP);
                } 
            }
        }
        // read from port1
        if (sw.port1 != -1) {
            if (fifo[1].revents & POLLIN) {
                fifo[1].revents = -1;
                FRAME frame;
                frame = rcvFrame(fifo[1].fd);
                relayInCounter++;

                printf("\nReceived (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.port1, sw.swID, frame.msg.mQuery.srcIP, frame.msg.mQuery.dstIP);

                int n = switchAction(flows, frame.msg.mQuery.srcIP, frame.msg.mQuery.dstIP, numFlowTable);
                if (n > 0) {
                    relayOutCounter++;
                    MSG msg;
                    msg.mRelay.srcIP = frame.msg.mQuery.srcIP;
                    msg.mRelay.dstIP = frame.msg.mQuery.dstIP;
                    if (n==1) {
                        sendFrame(fdPort1Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port1, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    } else if (n==2) {
                        sendFrame(fdPort2Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port2, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    }
                } else if (n == -1) {
                    // need to query
                    queryCounter++;
                    MSG msg;
                    msg.mQuery.swID = sw.swID;
                    msg.mQuery.dstIP = dstIP;
                    msg.mQuery.srcIP = srcIP;
                    msg.mQuery.port1 = sw.port1;
                    msg.mQuery.port2 = sw.port2;
                    sendFrame(fdConWrite, QUERY, &msg);
                    printf("\nTransmitted (src= sw%d, dest= cont)[QUERY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, srcIP, dstIP);
                } 
            }
        }
        if (sw.port2 != -1) {
            if (fifo[2].revents &POLLIN) {
                fifo[2].revents = -1;
                FRAME frame;
                frame = rcvFrame(fifo[2].fd);
                relayInCounter++;

                printf("\nReceived (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.port2, sw.swID, frame.msg.mQuery.srcIP, frame.msg.mQuery.dstIP);

                int n = switchAction(flows, frame.msg.mQuery.srcIP, frame.msg.mQuery.dstIP, numFlowTable);
                if (n > 0) {
                    relayOutCounter++;
                    MSG msg;
                    msg.mRelay.srcIP = frame.msg.mQuery.srcIP;
                    msg.mRelay.dstIP = frame.msg.mQuery.dstIP;
                    if (n==1) {
                        sendFrame(fdPort1Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port1, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    } else if (n==2) {
                        sendFrame(fdPort2Write, RELAY, &msg);
                        printf("\nTransmitted (src= sw%d, dest= sw%d) [RELAY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, sw.port2, msg.mQuery.srcIP, msg.mQuery.dstIP);
                    }
                } else if (n == -1) {
                    // need to query
                    queryCounter++;
                    MSG msg;
                    msg.mQuery.swID = sw.swID;
                    msg.mQuery.dstIP = dstIP;
                    msg.mQuery.srcIP = srcIP;
                    msg.mQuery.port1 = sw.port1;
                    msg.mQuery.port2 = sw.port2;
                    sendFrame(fdConWrite, QUERY, &msg);
                    printf("\nTransmitted (src= sw%d, dest= cont)[QUERY]:  header= (srcIP= %d, destIP= %d)\n", sw.swID, srcIP, dstIP);
                } 
            }
        }
    }
}

int switchAction(FlowTable flows[],int srcIP,int dstIP,int numFlowTable) {
    // return 0 for drop; return 1 for reply; return -1 for query;
    for (int i=0; i<numFlowTable; i++) {
        if (flows[i].dstIPlo<=dstIP && dstIP<=flows[i].dstIPhi) {
            // destIP fit one flows IP range
            // action if forward and val is 3
            if (flows[i].actionType == (FORWARD) && flows[i].actionVal == 3) {
                flows[i].pktCount++;
                return 0;
            }
            // action if drop
            else if (flows[i].actionType == DROP) {
                flows[i].pktCount++;
                return 0;
            }
            // action is reply
            else if (flows[i].actionType == FORWARD) {
                flows[i].pktCount++;
                return flows[i].actionVal;
            }
        }
    }
    return -1;
}

int printSwitch(FlowTable flows[],int numFlowTable, SwitchCounter swCounter) {
    // function to show information 
    // call by executeswitch when user input list or exit
    // also could call be USER1 signal handler of switch

    printf("Flow table:\n");
    for (int i=0; i<numFlowTable; i++) {
        printf("sw[%d] (srcIP= %d-%d, destIP= %d-%d, )", i, flows[i].srcIPlo, flows[i].srcIPhi, flows[i].dstIPlo, flows[i].dstIPhi);

        // display different info base on the action type
        if (flows[i].actionType == FORWARD) {
            printf("action= FORWARD:%d, pri= %d, pktCount= %d\n", flows[i].actionVal, flows[i].pri, flows[i].pktCount);
        } else {
            printf("action= DROP:%d, pri= %d, pktCount= %d\n", flows[i].actionVal, flows[i].pri, flows[i].pktCount);
        }
    }
    // some times lines under shows very late and without change in when ssh to the lab machine
    printf("Packet Stats:\n    Received:    ADMIT:%d, ACK:%d, ADDRULE:%d, RELAYIN:%d\n", swCounter.admitCounter, swCounter.ackCounter, swCounter.addRuleCounter, swCounter.relayInCounter);
    printf("    Transmitted: OPEN:%d, QUERY:%d, RELAYOUT:%d\n", swCounter.openCounter, swCounter.queryCounter, swCounter.relayOutCounter);
    return 0;
}

int controller(int numSwitch) {
    // set up the counters
    int openCounter = 0;
    int queryCounter = 0;
    int ackCounter = 0;
    int addCounter = 0;
    int fdRead[9];
    int fdWrite[9];
    SwitchInfo switch_list[numSwitch];
    printf("Controller start: have %d switch\n", numSwitch);


    for (int i = 0; i<numSwitch; i++) {
        // set switch id = 0 means this switch have not been created
        switch_list[i].swID = 0;
        int fdread = openFIFO(i+1,0);
        int fdwrite = openFIFO(0,i+1);
        fdRead[i] = fdread;
        fdWrite[i] = fdwrite;
    }

    while (1) {

        // update global variable
	    memcpy(&Con.switch_list, &switch_list, sizeof(switch_list));
        Con.numSwitch = numSwitch;
        Con.openCounter = openCounter;
        Con.queryCounter = queryCounter;
        Con.ackCounter = ackCounter;
        Con.addCounter = addCounter;

        // Poll from keyboard
        // http://blog.51cto.com/wait0804/1856818
        struct pollfd keyboard[1];
        keyboard[0].fd = STDIN_FILENO;
        keyboard[0].events = POLLIN;

        poll(keyboard, 1, 0);
        char userCmd[5];
        if ((keyboard[0].revents & POLLIN)) {
            // scanf("%s", userCmd);
            read(STDIN_FILENO, userCmd, 4*sizeof(char));
            userCmd[4] = '\0';
	        keyboard[0].revents = -1;
        }

        // run user cmd
        if (strcmp(userCmd,"list\0")==0) {
            printController(switch_list, numSwitch, openCounter, queryCounter, ackCounter, addCounter);
        } else if (strcmp(userCmd,"exit\0")==0) {
            printController(switch_list, numSwitch, openCounter, queryCounter, ackCounter, addCounter);
            return 0;
        }

        // poll from fifo
        struct pollfd pollfifo[numSwitch];

        for (int i=0;i<numSwitch;i++) {
            pollfifo[i].fd = fdRead[i];
            pollfifo[i].events = POLLIN;
        }

        poll(pollfifo, numSwitch, 0);

        for (int i=0;i<numSwitch;i++) {
            // check each FIFO with switch
            if ((pollfifo[i].revents & POLLIN)) {
                FRAME frame;
		        pollfifo[i].revents = -1;
                frame = rcvFrame(pollfifo[i].fd);

                if (frame.kind == OPEN) {
                    // open msg
                    openCounter += 1;

                    switch_list[i].swID = frame.msg.mOpen.swID;
                    switch_list[i].port1 = frame.msg.mOpen.port1;
                    switch_list[i].port2 = frame.msg.mOpen.port2;
                    switch_list[i].Iphi = frame.msg.mOpen.Iphi;
                    switch_list[i].IPlo = frame.msg.mOpen.IPlo;

                    printf("\nReceived (src= sw%d, dest= cont) [OPEN]:\n", i+1);
                    printf("	 (port0= cont, port1= %d , port2= %d, port3= %d-%d)\n", switch_list[i].port1, switch_list[i].port2, switch_list[i].IPlo, switch_list[i].Iphi);

                    MSG msg;
                    sendFrame(fdWrite[i], ACK, &msg);
                    ackCounter += 1;

                    printf("\nTransmitted (src= cont, dest= sw%d)[ACK]\n", i+1);
                }
                else if (frame.kind == QUERY) {
                    // query msg
                    queryCounter += 1;

                    printf("\nReceived (src= sw%d, dest= cont) [QUERY]:  header= (srcIP= %d, destIP= %d)\n", i+1, frame.msg.mQuery.srcIP, frame.msg.mQuery.dstIP);
                    MSG msg = controllerRule(frame.msg.mQuery, switch_list, numSwitch);
                    sendFrame(fdWrite[i], ADD, &msg);
                    
                    addCounter += 1;
                }
            }
        }
    }
}

MSG controllerRule(MSG_QUERY query, SwitchInfo switch_list[], int numSwitch) {
    // add msg creater, decide what action the switch will do
    MSG msg;
    int find=0;
    // set up the same part of add msg
    msg.mAdd.swID = query.swID;
    msg.mAdd.srcIP = query.srcIP;
    msg.mAdd.dstIP = query.dstIP;
    msg.mAdd.pri = 4;
    printf("\nTransmitted (src= cont, dest= sw%d) [ADD]:\n	 (srcIP= 0-1000, ", query.swID);
    for (int i=0; i<numSwitch; i++) {
        if (switch_list[i].IPlo <= query.dstIP && query.dstIP <= switch_list[i].Iphi) {
            // find a IP range that fit with the destIP
	        find = 1;
            msg.mAdd.action = FORWARD;
            msg.mAdd.dstIPlo = switch_list[i].IPlo;
            msg.mAdd.dstIPhi = switch_list[i].Iphi;
            if (i > query.swID) {
                msg.mAdd.actionVal = 2;
            } else {
                msg.mAdd.actionVal = 1;
            }
            printf("destIP= %d-%d, action= FORWARD:%d", msg.mAdd.dstIPlo, msg.mAdd.dstIPhi, msg.mAdd.actionVal);
            
	    break;
        }
    }
    if (find == 0) {
        // not find good IP range create a new flow with action drop
        msg.mAdd.action = DROP;
        msg.mAdd.dstIPlo = query.dstIP;
        msg.mAdd.dstIPhi = query.dstIP;
        msg.mAdd.actionVal = 0;
        printf("destIP= %d-%d, action= DROP:%d",msg.mAdd.dstIPlo, msg.mAdd.dstIPhi, msg.mAdd.actionVal);
    }

    printf(", pri= %d, pktCount= 0)", msg.mAdd.pri);
    return msg;
}

int printController(SwitchInfo switch_list[],int numSwitch,int openCounter,int queryCounter,int ackCounter,int addCounter) {
    // function to show controller information 
    // call by executeswitch when user input list or exit
    // also could call be USER1 signal handler
    printf("Switch information:\n");
    // show all switches' info by loop
    for (int i=0; i< numSwitch; i++) {
        if (switch_list[i].swID != 0) {
            printf("[sw%d] port1= %d, port2= %d, ", switch_list[i].swID, switch_list[i].port1, switch_list[i].port2);
	    printf("port3=%d-%d\n", switch_list[i].IPlo, switch_list[i].Iphi);
        }
    }
    printf("Packet Stats:\n");
    printf("    Received:   OPEN: %d, QUERY: %d\n", openCounter, queryCounter);
    printf("    Transmitted: ACK: %d, ADD: %d\n", ackCounter, addCounter);

    return 0;
}

int openFIFO(int sender, int reciver) {
    // base function used in both controller and switch
    // to open the FIFO
    char fifoName[10];

    strcpy(fifoName, "fifo-x-y");
    fifoName[5] = sender + '0';
    fifoName[7] = reciver + '0';

    return open(fifoName, O_RDWR);
}

// The code under this comment is all from the lab and class

FRAME rcvFrame (int fd)
{
    int    len; 
    FRAME  frame;

    assert (fd >= 0);
    memset( (char *) &frame, 0, sizeof(frame) );
    len= read (fd, (char *) &frame, sizeof(frame));
    if (len != sizeof(frame))
        WARNING ("Received frame has length= %d (expected= %d)\n",
		  len, sizeof(frame));
    return frame;		  
}

void FATAL (const char *fmt, ... )
{
    va_list  ap;
    fflush (stdout);
    va_start (ap, fmt);  vfprintf (stderr, fmt, ap);  va_end(ap);
    fflush (NULL);
    exit(1);
}

void WARNING (const char *fmt, ... )
{
    va_list  ap;
    fflush (stdout);
    va_start (ap, fmt);  vfprintf (stderr, fmt, ap);  va_end(ap);
}

void sendFrame (int fd, KIND kind, MSG *msg)
{
    FRAME  frame;

    assert (fd >= 0);
    memset( (char *) &frame, 0, sizeof(frame) );
    frame.kind= kind;
    frame.msg=  *msg;
    write (fd, (char *) &frame, sizeof(frame));
}
