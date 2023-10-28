// Link layer protocol implementation

#include "link_layer.h"


volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmTimeout = 0;
int nRetransmissions = 0;
unsigned char frame=0x00;
int fd;
LinkLayer connectionParam;
Statistics statistics;
clock_t begin;


////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters){
    begin = clock();
    printf("llopen start\n\n");

    statistics.nOfTimeouts=0;

    statistics.nOfBytesllopenSent=0;
    statistics.nOfPacketsllopenSent=0;
    statistics.nOfBytesllopenReceived=0;
    statistics.nOfPacketsllopenReceived=0;

    statistics.nOfBytesllwriteSent=0;
    statistics.nOfPacketsllwriteSent=0;
    statistics.nOfBytesllwriteReceived=0;
    statistics.nOfPacketsllwriteReceived=0;
    statistics.nOfCharStuffed=0;

    statistics.nOfBytesllreadReceived=0;
    statistics.nOfPacketsllreadReceived=0;
    statistics.nOfBytesllreadReceived=0;
    statistics.nOfPacketsllreadReceived=0;
    statistics.nOfCharDestuffed=0;

    statistics.nOfBytesllcloseSent=0;
    statistics.nOfPacketsllcloseSent=0;
    statistics.nOfBytesllcloseReceived=0;
    statistics.nOfPacketsllcloseReceived=0;

    connectionParam = connectionParameters;

    fd = connect(connectionParameters.serialPort);
    if(fd < 0) return -1;
    State state = FIRSTFLAG;
    unsigned char buf_read[2] = {0};
    switch(connectionParameters.role){
        case LlTx: {
            nRetransmissions = connectionParameters.nRetransmissions;
            (void)signal(SIGALRM,alarmHandler);
            while(nRetransmissions > 0){
                if (!alarmEnabled){ // enables the timer with timeout seconds 
                    alarm(connectionParameters.timeout);
                    alarmEnabled = TRUE;
                    int bytes = writeSupervisionFrame(0x03, SET);
                    printf("%d bytes written\n", bytes);
                    statistics.nOfBytesllopenSent+=bytes;
                    statistics.nOfPacketsllopenSent++;
                }
                STOP=FALSE;
                while(!STOP) {
                    int bytesRead = read(fd, buf_read, 1);
                    if(bytesRead!=0){
                        statistics.nOfBytesllopenReceived++;
                        printf("char= 0x%02X | state= %d\n", buf_read[0], state);
                        if (state != FINALFLAG && buf_read[0] == FLAG) {
                            state = A;
                        }
                        else if (state == A && buf_read[0] == 0x03) {
                            state = C;
                        }
                        else if (state == C && buf_read[0] == UA) {
                            state = BCC1;
                        }
                        else if (state == BCC1 && buf_read[0] == (0x03 ^ UA)) {
                            state = FINALFLAG;
                        }
                        else if (state == FINALFLAG && buf_read[0] == FLAG) {
                            STOP = TRUE;
                            nRetransmissions=-1;
                            statistics.nOfPacketsllopenReceived++;
                        }
                        else state = FIRSTFLAG;
                    }
                }
            }
            nRetransmissions = connectionParameters.nRetransmissions;
            alarmEnabled=FALSE;
            alarm(0); 
            if(nRetransmissions==0){
                printf("MASSIVE failure");
                exit(-1);
            }
            break;
        }

        case LlRx: {
            while(!STOP){
                int bytesRead = read(fd, buf_read, 1);
                if(bytesRead!=0){
                    statistics.nOfBytesllopenReceived++;
                    printf("char= 0x%02X | state= %d\n", buf_read[0], state);

                    if(state != FINALFLAG && buf_read[0]==FLAG){
                        state = A;
                    }
                    else if(state == A && buf_read[0]==0x03){
                        state = C;
                    }
                    else if(state == C && buf_read[0]==SET){
                        state = BCC1;
                    }
                    else if(state == BCC1 && buf_read[0]==(0x03 ^ SET)){
                        state = FINALFLAG;
                    }
                    else if(state == FINALFLAG && buf_read[0] == FLAG){
                        STOP=TRUE;
                        statistics.nOfPacketsllopenReceived++;
                    }
                    else state = FIRSTFLAG;
                }
            }
            int bytes = writeSupervisionFrame(0x03, UA);
            statistics.nOfBytesllopenSent+=bytes;
            statistics.nOfPacketsllopenSent++;
            printf("%d bytes written\n", bytes);
            break;
        }
    }
    STOP = FALSE;
    printf("\n\nllopen success\n\n");
    return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize){
    printf("llwrite start\n\n");
    
    State state = FIRSTFLAG;
    unsigned char* buf_write = (unsigned char*) malloc((bufSize*2)+8);;
    stuffBytes(buf_write,&bufSize,buf);

    unsigned char cField = 0x00;

    unsigned char buf_read[2] = {0};
    //sleep(1);
    
    while (nRetransmissions > 0)
    { 
        if (alarmEnabled == FALSE)
        {
            int bytes = write(fd, buf_write, bufSize);
            printf("%d bytes written\n", bytes);
            sleep(1);
            statistics.nOfBytesllwriteSent+=bufSize;
            statistics.nOfPacketsllwriteSent++;
            alarm(connectionParam.timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
            
        }
        state = FIRSTFLAG;
        
        STOP=FALSE;
        while(!STOP) {
            int bytesRead = read(fd, buf_read, 1);
            if (bytesRead!=0){
                statistics.nOfBytesllwriteReceived++;
                printf("char= 0x%02X | state= %d\n", buf_read[0], state);
                if (state != FINALFLAG && buf_read[0] == FLAG) {
                    state = A;
                }
                else if (state == A && buf_read[0] == 0x03) {
                    state = C;
                }
                else if (state == C) {
                    state = BCC1;
                    cField = buf_read[0];
                } 
                else if (state == BCC1 && buf_read[0] == (0x03 ^ cField)) {
                    state = FINALFLAG;
                } 
                else if (state == FINALFLAG && buf_read[0] == FLAG) {
                    printf("Entered FINALFLAG state\n");
                    if (cField == UA) {
                        state = SUCCESS;
                    }
                    else if (cField == RR0) {
                        if(frame==FRAME0){
                            state=FAILURE;
                        }
                        else{
                            frame = FRAME0;
                            state = SUCCESS;
                            printf("Got success in RR0");
                        }
                    }
                    else if (cField == RR1) {
                        if(frame==FRAME1){
                            state=FAILURE;
                        }
                        else{
                            frame = FRAME1;
                            state = SUCCESS;
                            printf("Got success in RR1");
                        }
                    }
                    else if (cField == REJ0 || cField == REJ1 || cField == DISC) {
                        state=FAILURE;
                    }
                }
                else state = FIRSTFLAG;
            }
            if (state==SUCCESS){
                STOP = TRUE;
                alarmEnabled=FALSE;
                nRetransmissions=-1;
                statistics.nOfPacketsllwriteReceived++;
                printf("final transfer complete\n");
            }
            else if (state==FAILURE){
                STOP = TRUE;
                printf("rej recieved or failiure in the response frame\n");
                alarmEnabled=FALSE;
                state=FIRSTFLAG;
                nRetransmissions = connectionParam.nRetransmissions;
            }
        }
    }
    nRetransmissions = connectionParam.nRetransmissions;
    alarmEnabled=FALSE;
    alarm(0);
    STOP=FALSE;
    if(nRetransmissions==0){
        printf("MASSIVE failure");
        exit(-1);
    }
    printf("\n\nllwrite success\n\n");
    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *output,int* bufSize)
{
    printf("llread start\n\n");
    unsigned char buf_read[2] = {0};
    int responseFrame=0;
    int currentIndex=0;
    unsigned char bcc2Field=0x00;
    unsigned char cField=0x00;
    State state = FIRSTFLAG;
    while(!STOP){
        int bytesRead = read(fd, buf_read, 1);
        if(bytesRead!=0){
            statistics.nOfBytesllreadReceived++;
            if(state != DATA){
                printf("char= 0x%02X | state= %d\n", buf_read[0], state);
            }
            if(state!=DATA && state!=FINALFLAG && buf_read[0] == FLAG){
                state = A;
            }
            else if(state==FIRSTFLAG){
                state = FIRSTFLAG;
            }
            else if(state == A && buf_read[0]==0x03){
                state = C;
            }
            else if(state == C){
                state=BCC1;
                cField=buf_read[0];
            }
            else if(state == BCC1 && buf_read[0]==(0x03 ^ cField)){
                if(cField == DISC){
                    state = FINALFLAG;
                }
                else if (cField==SET)
                {
                    printf("transmiter stuck in llopen :(\n");
                    state=FINALFLAG;
                }
                else{
                    state = DATA;
                }
            }
            else if(state == DATA){
                //remove if bellow to print all data bytes
                if(bytesRead == 0){
                    printf("char= 0x%02X | ", buf_read[0]);
                    printf("bcc2Field= 0x%02X ", bcc2Field);
                    printf("from= 0x%02X | ", output[currentIndex-2]);
                    printf("state= %d\n",state);
                }
                if(currentIndex!=0 && buf_read[0]==0x5E && output[currentIndex-1]==0x7D){
                    output[currentIndex-1]=0x7E;
                    bcc2Field ^= output[currentIndex-2];
                    statistics.nOfCharDestuffed++;
                }
                else if(currentIndex!=0 && buf_read[0]==0x5D && output[currentIndex-1]==0x7D){
                    output[currentIndex-1]=0x7D;
                    bcc2Field ^= output[currentIndex-2];
                    statistics.nOfCharDestuffed++;
                }
                else{
                    output[currentIndex]=buf_read[0];
                    if(currentIndex>1){
                        if(output[currentIndex-1]!=0x7E && output[currentIndex-1]!=0x7D){
                            bcc2Field ^= output[currentIndex-2];
                        }
                    }
                    if(buf_read[0] != FLAG) {
                        currentIndex++;
                    }
                }
                if(buf_read[0] == FLAG) {
                    printf("char = 0x%02X | bcc2Field = 0x%02X | output[firstDataByte-1] = 0x%02X | state = %d | number of bytes received = %-d", buf_read[0],bcc2Field,output[currentIndex-1],state,statistics.nOfBytesllreadReceived);
                    if(bcc2Field==output[currentIndex-1]){
                        state = SUCCESS; 
                        output[currentIndex-1]=0;
                        currentIndex--;
                    }
                    else{
                        state=FAILURE;
                    }
                }
            }
            else if(state == FINALFLAG && buf_read[0] == FLAG) {
                if(cField==SET){
                    int bytes = writeSupervisionFrame(0x03, UA);
                    statistics.nOfBytesllopenSent+=bytes;
                    statistics.nOfPacketsllopenSent++;
                    printf("UA sent by llread\n");
                    state=FIRSTFLAG;
                }
                else if(cField == frame){
                    state = SUCCESS;
                }
                else if (cField == DISC) {
                    state = DISCONNECTING;
                }
                else{
                    state = FAILURE;
                }
            }
            else state = FAILURE;
        }
        if(state==SUCCESS){
            *bufSize=currentIndex;
            if(frame==FRAME0){
                frame=FRAME1;
                responseFrame = RR1;
            }
            else{
                frame=FRAME0;
                responseFrame = RR0;
            }
            int bytes =writeSupervisionFrame(0x03, responseFrame);
            printf("sucsess frame sent with 0x%02X\n",responseFrame);
            statistics.nOfPacketsllreadReceived++;
            statistics.nOfBytesllreadSent+=bytes;
            statistics.nOfPacketsllreadSent++;
            state=FIRSTFLAG;
            STOP=TRUE;
        }
        else if (state==FAILURE) {
            if(cField==FRAME0){
                responseFrame = REJ0;
            }
            else{
                responseFrame = REJ1;
            }
            writeSupervisionFrame(0x03, responseFrame);
            printf("failure frame sent with 0x%02X\n",responseFrame);
            bcc2Field=0x00;
            currentIndex=0;
            state=FIRSTFLAG;
        }
        else if (state==DISCONNECTING){
            STOP=TRUE;
            *bufSize=-1;
        }
    }
    STOP=FALSE;
    printf("llread success\n\n");
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics){
    printf("llclose start\n\n");

    unsigned char buf_read[2] = {0};

    unsigned char aField=0x00;
    unsigned char cField=0x00;

    State state = FIRSTFLAG;
    
    (void)signal(SIGALRM,alarmHandler);
    switch(connectionParam.role) {
        case LlTx:{
            while(nRetransmissions > 0){
                if (!alarmEnabled){ // enables the timer with timeout seconds 
                    alarm(connectionParam.timeout);
                    alarmEnabled = TRUE;
                    int bytes = writeSupervisionFrame(0x03,DISC);
                    printf("%d bytes written\n", bytes);
                    statistics.nOfBytesllcloseSent+=bytes;
                    statistics.nOfPacketsllcloseSent++;
                }
                STOP=FALSE;
                while(!STOP) {
                    int bytesRead = read(fd, buf_read, 1);
                    if(bytesRead!=0){
                        statistics.nOfBytesllcloseReceived++;
                        printf("char= 0x%02X | state= %d\n", buf_read[0], state);
                        if (state != FINALFLAG && buf_read[0] == FLAG) {
                            state = A;
                        }
                        else if (state == A && buf_read[0] == 0x01) {
                            state = C;
                        }
                        else if (state == C && buf_read[0] == DISC) {
                            state = BCC1;
                        }
                        else if (state == BCC1 && buf_read[0] == (0x01 ^ DISC)) {
                            state = FINALFLAG;
                        }
                        else if (state == FINALFLAG && buf_read[0] == FLAG) {
                            STOP = TRUE;
                            statistics.nOfPacketsllcloseReceived++;
                            nRetransmissions = -1;
                            alarmEnabled=FALSE;
                            printf("success");
                            state=SUCCESS;
                        }
                        else if (state==FAILURE){
                            STOP=TRUE;
                            alarmEnabled=FALSE;
                            state=FIRSTFLAG;
                        }
                        else state = FAILURE;
                    }
                }
            }
            printf("activate noise :3");
            sleep(1);
            int bytes = writeSupervisionFrame(0x01,UA);
            statistics.nOfBytesllcloseSent+=bytes;
            statistics.nOfPacketsllcloseSent++;
            nRetransmissions = connectionParam.nRetransmissions;
            if(nRetransmissions==0){
                printf("MASSIVE failure");
                exit(-1);
            }
            break;
        }
        case LlRx:{
            int bytes = writeSupervisionFrame(0x01,DISC);
            statistics.nOfBytesllcloseSent+=bytes;
            statistics.nOfPacketsllcloseSent++;
            nRetransmissions = 5;
            if (!alarmEnabled){  
                alarm(5);
                alarmEnabled = TRUE;
                statistics.nOfBytesllcloseSent+=bytes;
                statistics.nOfPacketsllcloseSent++;
            }
            while(!STOP){
                int bytesRead = read(fd, buf_read, 1);
                if(bytesRead!=0){
                    statistics.nOfBytesllcloseReceived++;
                    printf("char= 0x%02X | state= %d\n", buf_read[0], state);
                    if(state != FINALFLAG && buf_read[0] == FLAG){
                        state = A;
                    }
                    else if(state == A && (buf_read[0]==0x03 || buf_read[0]==0x01)){
                        state = C;
                        aField=buf_read[0];
                    }
                    else if(state == C && ((aField==0x03 && buf_read[0]==DISC) || (aField==0x01 && buf_read[0]==UA))){
                        state = BCC1;
                        cField=buf_read[0];
                    }
                    else if(state == BCC1 && buf_read[0]==(aField ^ cField)){
                        state = FINALFLAG;
                    }
                    else if(state == FINALFLAG && buf_read[0] == FLAG){
                        if(cField==UA){
                            STOP=TRUE;
                            printf("success");
                            statistics.nOfPacketsllcloseReceived++;
                        }
                        else{
                            int bytes = writeSupervisionFrame(0x01,DISC);
                            statistics.nOfBytesllcloseSent+=bytes;
                            statistics.nOfPacketsllcloseSent++;
                            state=FIRSTFLAG;
                        }
                    }
                    else state = FIRSTFLAG;
                }
            }
            if(nRetransmissions==0){
                printf("llclose finished due to timeout\n");
            break;
            }
        }
    }

    close(fd); //closes the serial port
    printf("\n\nllclose success\n\n");

    clock_t end = clock();    

    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Execution time : %lf seconds",time_spent);
    return 0;
}


int connect(const char *serialPort){
    // Open serial port device for reading and writing and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    int file = open(serialPort, O_RDWR | O_NOCTTY);
    printf("connected\n");
    if (file < 0)
    {
        perror(serialPort);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(file, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0.1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)
    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by file but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(file, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(file, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    return file;
}

void alarmHandler(int signal){
    alarmEnabled = FALSE;
    nRetransmissions--;
    statistics.nOfTimeouts++;
    STOP=TRUE;
    printf("Alarm #%d\n", nRetransmissions);
}

int writeSupervisionFrame(unsigned char A, unsigned char C){
    unsigned char supervisionFrame[5] = {FLAG, A, C, A ^ C, FLAG};
    int bytesWritten = 0;
    if((bytesWritten = write(fd, supervisionFrame, 5)) < 0) return -1;
    sleep(1);
    return bytesWritten;
}

int stuffBytes(unsigned char *buf_write, int *bufSize, const unsigned char *buf){
    buf_write[0] = FLAG;
    buf_write[1] = 0x03;
    buf_write[2] = frame;
    buf_write[3] = buf_write[1] ^ buf_write[2];
    int offset=4;
    unsigned char bcc2Field=0x00;
    for(int i=0;i<*bufSize;i++){
        if(buf[i]==0x7E){
            buf_write[i+offset]=0x7D;
            offset++;
            buf_write[i+offset]=0x5E;
            statistics.nOfCharStuffed++;
        }
        else if (buf[i]==0x7D)
        {
            buf_write[i+offset]=0x7D;
            offset++;
            buf_write[i+offset]=0x5D;
            statistics.nOfCharStuffed++;
        }
        else{
            buf_write[i+offset]=buf[i];
        } 
        bcc2Field^=buf[i];
    }
    printf("bcc2Field= 0x%02X\n", bcc2Field);
    if(bcc2Field==0x7E){
        buf_write[*bufSize+offset] = 0x7D;
        offset++;
        buf_write[*bufSize+offset] = 0x5E;
        statistics.nOfCharStuffed++;
    }
    else if (bcc2Field==0x7D)
    {
        buf_write[*bufSize+offset] = 0x7D;
        offset++;
        buf_write[*bufSize+offset] = 0x5D;
        statistics.nOfCharStuffed++;
    }
    else{
        buf_write[*bufSize+offset] = bcc2Field;
    }
    offset++;
    buf_write[*bufSize+offset] = FLAG;
    offset++;
    *bufSize+=offset;
    return 0;
}
