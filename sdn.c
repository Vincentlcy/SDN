# include <stdio.h>
# include <stdlib.h>
# include <poll.h>
# include <unistd.h>
# include <stdarg.h>
# include <assert.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <signal.h>
# include <string.h>
# include <malloc.h>

# define MAX_NSW 7；
# define MAX_IP 1000;
# define MIN_PRI 4;
# define DROP 0;
# define FORWARD 1; 

typedef enum {ACK, OPEN, QUERY, ADD, RELAY} KIND; // kind of message from lab

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

typedef union { MSG_OPEN mOpen; MSG_QUERY mQuery; MSG_ADD mAdd; MDG_RELAY mRelay;} MSG;
typedef struct { KIND kind; MSG msg; } FRAME;

typedef struct {
    int swID;
    int port1;
    int port2;
    int IPlo;
    int Iphi;
} SwitchInfo;

struct pollfd {
    int fd;        /* file descriptor */
    short events;     /* requested events */
    short revents;    /* returned events */
};

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

typedef struct {
    int admitCounter;
    int ackCounter;
    int addRuleCounter;
    int relayInCounter;
    int relayOutCounter;
    int openCounter;
    int queryCounter;
} SwitchCounter;

typedef struct {
    struct SwitchInfo[] switch_list;
    int numSwitch;
    int openCounter;
    int queryCounter;
    int ackCounter;
    int addCounter;
} Con;

typedef struct {
    struct FlowTable[] flows;
    int numFlowTable;
    struct SwitchCounter swCounter;
} SW;

void user1Controller(int signum);
void user1Controller(int signum);
int executeswitch(SwitchInfo sw, char[] filename);
int switchAction(struct FlowTable[] flows,int srcIP,int dstIP,int numFlowTable);
int printSwitch(struct FlowTable[] flows,int numFlowTable,struct SwitchCounter swCounter);
MSG controllerRule(MSG_QUERY query, SwitchInfo[] switch_list, int numSwitch);
void controller(int numSwitch);
int printController(SwitchInfo[] switch_list,int numSwitch,int openCounter,int queryCounter,int ackCounter,int addCounter);
void FATAL (const char *fmt, ... );
int openFIFO(int sender, int revsiver);
FRAME rcvFrame (int fd);
void WARNING (const char *fmt, ... );
void sendFrame (int fd, KIND kind, MSG *msg);

struct Con Con;
struct SW Sw;

int main(int argc, char* argv[]) {

    char name[50];
    strcpy(name, argv[1]);

    if (strcmp(argv[1], "cont") == 0 && argc == 3) {
        /*controler mode*/

        if (argv[2] > MAX_NSW) {
            printf("Too much Switchs"); // check number of switches
            return -2;
        }

        /* Bound singal USER1 with handler */
        singal(SIGUSR1, user1Controller);
        
        controller(argv[2]);

    } else if (argc == 6 && name[0] == "s" && name[1] == "w") {
        /*switch mode*/
        struct SwitchInfo sw;
        sw.swID = int(name[3]);
        
        char filename[50];
        strcpy(filename, argv[2]);

        if (strcmp(argv[3],"null")) {
            sw.port1 = -1;
        } else {
            sw.port1 = int(argv[3][3]);
        }

        if (strcmp(argv[4],"null")) {
            sw.port2 = -1;
        } else {
            sw.port2 = int(argv[4][3]);
        }

        char *s = "-";
        char *sp = NULL;
        sw.IPlo = int(strtok_r(argv[5], s, &sp));
        sw.Iphi = int(strtok_r(NULL, s, &sp));

        /* Bound singal USER1 with handler */
        singal(SIGUSR1, user1Switch);

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

void user1Controller(int signum) {
    printf("\n");
    printf("\nUSER1 singal received \n");
    printSwitch(Sw.flows, Sw.numFlowTable, Sw.swCounter);
    return;
}

int executeswitch(SwitchInfo sw, char[] filename) {
    FILE *filefp;
    filefp = fopen(filename, "r");
    if (filefp == NULL) {
        printf("Fail to read file");
        return -1;
    }

    struct FlowTable *flows;
    flows = malloc(sizeof(FlowTable));

    flows.srcIPlo = 0;
    flows.srcIPhi = MAX_IP;
    flows.dstIPhi = sw.IPhi;
    flows.dstIPlo = sw.Iphi;
    flows.actionType = FORWARD;
    flows.actionVal = 3;
    flows.pri = 4;
    flows.pktCount = 0;

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
    if (sw.port1 != -1) {
        int fdPort1Read = openFIFO(sw.port1, sw.swID);
        int fdPort1Write = openFIFO(sw.swID, sw.port1);
    } else if (sw.port2 != -1) {
        int fdPort2Read = openFIFO(sw.port2, sw.swID);
        int fdPort2Write = openFIFO(sw.swID, sw.port2);
    }

    // send open message to Controller
    MSG msg;
    msg.mOpen.swID = sw.swID;
    msg.mOpen.port1 = sw.port1;
    msg.mOpen.port2 = sw.port2;
    msg.mOpen.IPlo = sw.IPlo;
    msg.mOpen.Iphi = sw.Iphi;

    sendFrame(fdConWrite, OPEN, %msg);
    openCounter += 1;

    while (1) {
        int aimSwith = 0;
        int srcIP = 0;
        int dstIP = 0;

        // Poll from keyboard
        // http://blog.51cto.com/wait0804/1856818
        struct pollfd keyboard;
        keyboard.fd = STDIN_FILENO;
        keyboard.events = POLLIN;

        poll(keyboard, 1, 0);
        userCmd[50];
        if ((keyboard.revents & POLLIN)) {
            scanf("%s", userCmd);
        }

        struct SwitchCounter swCounter;
        swCounter.admitCounter = addRuleCounter
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

        } else if (strcmp(userCmd,"exit")==0)) {
            printSwitch(flows, numFlowTable, swCounter);
            return 0;
        }

        // read from file
        char line[100];
        if (fgets(line, 100, filefp)!=NULL) {
            if (strcmp(line[0], "#")==0 || strcmp(line[0], '\0') == 0) {}
            else {
                char *s = " ";
	            char *sp = NULL;
                aimSwith = int(strtok_r(line, s, &sp));
                srcIP = int(strtok_r(NULL, s, &sp));
                dstIP = int(strtok_r(NULL, s, &sp));
            }
        }

        if (aimSwith == sw.swID) {
            int n = switchAction(flows, srcIP, dstIP, numFlowTable);
            admitCounter++;
            if (n > 0) {
                relayOutCounter++;
                struct MSG msg;
                msg.mRelay.srcIP = srcIP;
                msg.mRelay.dstIP = dstIP;
                if (n==1) {
                    sendFrame(fdPort1Write, RELAY, &msg);
                } else if (i==2) {
                    sendFrame(fdPort2Write, RELAY, &msg)
                }
            } else if (n == -1) {
                // need to query
                queryCounter++;
                struct MSG msg;
                msg.mQuery.swID = sw.swID;
                msg.mQuery.dstIP = dstIP;
                msg.mQuery.srcIP = srcIP;
                msg.mQuery.port1 = sw.port1;
                msg.mQuery.port2 = sw.port2;
            } 
        }
        aimSwith = 0; // reset 

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
            struct FRAME frame;
            frame = rcvFrame(fifo[0].fd);

            if (frame.kind == ACK) {
                ackCounter += 1;
            }

            if (frame.kind == ADD) {
                numFlowTable++;
                addRuleCounter++;
                flows = realloc(flows, sizeof(struct FlowTable)*numFlowTable);

                flows[numFlowTable-1].srcIPlo = 0;
                flows[numFlowTable-1].srcIPhi = MAX_IP;
                flows[numFlowTable-1].dstIPlo = frame.msg.mAdd.dstIPlo;
                flows[numFlowTable-1].dstIPhi = frame.msg.mAdd.dstIPhi;
                flows[numFlowTable-1].actionType = frame.msg.mAdd.action;
                flows[numFlowTable-1].actionVal = frame.msg.mAdd.actionVal;
                flows[numFlowTable-1].pri = frame.msg.mAdd.pri;
                flows[numFlowTable-1].pktCount = 0;

                switchAction(flows, frame.msg.mAdd.srcIP, frame.msg.mAdd.dstIP, numFlowTable);
                admitCounter++;
                if (n > 0) {
                    relayOutCounter++;
                    struct MSG msg;
                    msg.mRelay.srcIP = frame.msg.mAdd.srcIP;
                    msg.mRelay.dstIP = frame.msg.mAdd.dstIP;
                    if (n==1) {
                        sendFrame(fdPort1Write, RELAY, &msg);
                    } else if (i==2) {
                        sendFrame(fdPort2Write, RELAY, &msg)
                    }
                } else if (n == -1) {
                    // need to query
                    queryCounter++;
                    struct MSG msg;
                    msg.mQuery.swID = sw.swID;
                    msg.mQuery.dstIP = dstIP;
                    msg.mQuery.srcIP = srcIP;
                    msg.mQuery.port1 = sw.port1;
                    msg.mQuery.port2 = sw.port2;
                } 
            }
        }
        // read from port1
        if (sw.port1 != -1) {
            if (fifo[1].revents & POLLIN) {
                struct FRAME frame;
                frame = rcvFrame(fifo[1].fd);
                relayInCounter++;
                switchAction(flows, frame.msg.mQuery.srcIP, frame.msg.mQuery.dstIP, numFlowTable);
                if (n > 0) {
                    relayOutCounter++;
                    struct MSG msg;
                    msg.mRelay.srcIP = frame.msg.mQuery.srcIP;
                    msg.mRelay.dstIP = frame.msg.mQuery.dstIP;
                    if (n==1) {
                        sendFrame(fdPort1Write, RELAY, &msg);
                    } else if (i==2) {
                        sendFrame(fdPort2Write, RELAY, &msg)
                    }
                } else if (n == -1) {
                    // need to query
                    queryCounter++;
                    struct MSG msg;
                    msg.mQuery.swID = sw.swID;
                    msg.mQuery.dstIP = dstIP;
                    msg.mQuery.srcIP = srcIP;
                    msg.mQuery.port1 = sw.port1;
                    msg.mQuery.port2 = sw.port2;
                } 
            }
        }
        if (sw.port2 != -1) {
            if (fifo[2].revents &POLLIN) {
                struct FRAME frame;
                frame = rcvFrame(fifo[2].fd);
                relayInCounter++;
                switchAction(flows, frame.msg.mQuery.srcIP, frame.msg.mQuery.dstIP, numFlowTable);
                if (n > 0) {
                    relayOutCounter++;
                    struct MSG msg;
                    msg.mRelay.srcIP = frame.msg.mQuery.srcIP;
                    msg.mRelay.dstIP = frame.msg.mQuery.dstIP;
                    if (n==1) {
                        sendFrame(fdPort1Write, RELAY, &msg);
                    } else if (i==2) {
                        sendFrame(fdPort2Write, RELAY, &msg)
                    }
                } else if (n == -1) {
                    // need to query
                    queryCounter++;
                    struct MSG msg;
                    msg.mQuery.swID = sw.swID;
                    msg.mQuery.dstIP = dstIP;
                    msg.mQuery.srcIP = srcIP;
                    msg.mQuery.port1 = sw.port1;
                    msg.mQuery.port2 = sw.port2;
                } 
            }
        }
    }
}

int switchAction(struct FlowTable[] flows,int srcIP,int dstIP,int numFlowTable) {
    // return 0 for drop return 1 for reply return -1 for query
    for (int i=0; i<numFlowTable; i++) {
        if (flows[i].dstIPlo<=dstIP<=flows[i].dstIPhi) {
            if (flows[i].actionType == FORWARD && flows[i].actionVal == 3) {
                flows[i].pktCount++;
                return 0;
            }
            else if (flows[i].actionType == DROP) {
                flow[i].pktCount++;
                return 0;
            }
            else if (flows[i].actionType == FORWARD) {
                flow[i].pktCount++;
                return flows[i].actionVal;
            }
        }
    }
    return -1;
}

int printSwitch(struct FlowTable[] flows,int numFlowTable,struct SwitchCounter swCounter) {
    printf("Flow table:");
    for (int i=0; i<numFlowTable; i++) {
        printf("[%d] (srcIP= %d-%d, destIP= %d-%d, )", i, flows[i].srcIPlo, flows[i].srcIPhi, flows[i].dstIPlo, flows[i].dstIPhi);
        if (flows[i].actionType == FORWARD) {
            printf("action= FORWARD:%d, pri= %d, pktCount= %d\n", flows[i].actionVal, flows[i].pri, flows[i].pktCount);
        } else {
            printf("action= DROP:%d, pri= %d, pktCount= %d\n", flows[i].actionVal, flows[i].pri, flows[i].pktCount);
        }
    }
    printf("Packet Stats:\n    Received:    ADMIT:%d, ACK:%d, ADDRULE:%d, RELAYIN:%d\n", swCounter.admitCounter, SwitchCounter.ackCounter, SwitchCounter.addRuleCounter, SwitchCounter.relayInCounter);
    printf("    Transmitted: OPEN:%d, QUERY:%d, RELAYOUT:%d", SwitchCounter.openCounter, SwitchCounter.queryCounter, SwitchCounter.relayOutCounter);
    return 0;
}

int controller(int numSwitch) {
    int openCounter = 0;
    int queryCounter = 0;
    int ackCounter = 0;
    int addCounter = 0;
    int fdRead[9];
    int fdWrite[9];
    SwitchInfo switch_list[numSwitch];


    for (int i = 1; i<numSwitch; i++) {
        // set switch id = 0 means this switch have not been created
        switch_list[i-1].swID = 0;
        int fdread = openFIFO(i,0);
        int fdwrite = openFIFO(0,i);
        fdRead[i-1] = fdread;
        fdWrite[i-1] = fdwrite;
    }

    while (1) {

        // update global variable
        Con.switch_list = switch_list;
        Con.numSwitch = numSwitch;
        Con.openCounter = openCounter;
        Con.queryCounter = queryCounter;
        Con.ackCounter = ackCounter;
        Con.addCounter = addCounter;

        // Poll from keyboard
        // http://blog.51cto.com/wait0804/1856818
        struct pollfd keyboard;
        keyboard.fd = STDIN_FILENO;
        keyboard.events = POLLIN;

        poll(keyboard, 1, 0);
        userCmd[50];
        if ((keyboard.revents & POLLIN)) {
            scanf("%s", userCmd);
        }

        // run user cmd
        if (strcmp(userCmd,"list")==0) {
            printController(switch_list, numSwitch, openCounter, queryCounter, ackCounter, addCounter);
        } else if (strcmp(userCmd,"exit")==0)) {
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
            if ((keyboard.revents & POLLIN)) {
                FRAME frame;
                frame = rcvFrame(pollfifo[i].fd);

                if (frame.kind == OPEN) {
                    openCounter += 1;

                    switch_list[i].swID = frame.msg.mOpen.swID;
                    switch_list[i].port1 = frame.msg.mOpen.port1;
                    switch_list[i].port2 = frame.msg.mOpen.port2;
                    switch_list[i].Iphi = frame.msg.mOpen.Iphi;
                    switch_list[i].IPlo = frame.msg.mOpen.IPlo;

                    MSG msg = NULL;
                    sendFrame(fdWrite[i], ACK, &msg);
                    ackCounter += 1;
                }
                else if (frame.kind == QUERY) {
                    queryCounter += 1;

                    MSG msg = controllerRule(frame.msg.mQuery, switch_list);
                    sendFrame(fdWrite[i], ADD, &msg);
                    addCounter += 1;
                }
            }
        }
    }
}

MSG controllerRule(MSG_QUERY query, SwitchInfo[] switch_list, int numSwitch) {
    MSG msg;
    msg.mAdd.swID = query.swID;
    msg.mAdd.srcIP = query.srcIP;
    msg.mAdd.dstIP = query.dstIP;
    msg.mAdd.pri = 4;
    for (int i=0; i<numSwitch; i++) {
        if (switch_list[i].IPlo <= query.dstIP <= switch_list[i].IPhi) {
            msg.mAdd.action = FORWARD;
            msg.mAdd.dstIPlo = switch_list[i].IPlo;
            msg.mAdd.dstIPhi = switch_list[i].IPhi;
            if (i > query.swID) {
                msg.mAdd.actionVal = 2;
            } else {
                msg.mAdd.actionVal = 1;
            }
            break;
        }
    }
    if (msg.mAdd.action != FORWARD) {
        msg.mAdd.action = DROP;
        msg.dstIPlo = query.dstIP;
        msg.dstIPhi = query.dstIP;
    }

    return msg;
}

int printController(SwitchInfo[] switch_list,int numSwitch,int openCounter,int queryCounter,int ackCounter,int addCounter) {
    printf("Switch information:\n");
    for (int i=0; i< numSwitch; i++) {
        if (switch_list[i].swID != 0) {
            printf("[sw%d] port1= %d, port2= %d, port3= %d-$d", switch_list[i].swID, switch_list[i].port1, switch_list[i].port2, switch_list[i].IPlo, switch_list[i].Iphi);
        }
    }
    printf("Packet Stats:\n");
    printf("    Received:   OPEN: %d, QUERY: %d", openCounter, queryCounter);
    printf("    Transmitted: ACK: %d, ADD: %d", ackCounter, addCounter);

    return 0;
}

int openFIFO(int sender, int reciver) {
    char fifoName[10];

    strcpy(fifoName, "fifo-x-y");
    fifoName[5] = sender + '0';
    fifoName[7] = reciver + '0';

    return open(fifoName, O_RDWR);
}

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