#ifndef GLOBALH
#define GLOBALH

#define _GNU_SOURCE
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
/* odkomentować, jeżeli się chce DEBUGI */
//#define DEBUG 
/* boolean */
#define TRUE 1
#define FALSE 0

/* używane w wątku głównym, determinuje jak często i na jak długo zmieniają się stany */
#define STATE_CHANGE_PROB 50
#define SEC_IN_STATE 2

#define ROOT 0

extern int upperLimit;
extern int lowerLimit;

extern int hunterTeamsNum;

extern pthread_mutex_t activeTasksMut;


/* stany procesu */
typedef enum {InActive, InOverload, InSearch,InWait, InShop,InTask, InFinish} state_t;
extern state_t stan;
extern int rank;
extern int size;

typedef enum {REJECTED, REQUEST_NOT_SEND, REQUEST_SEND, ACK_RECEIVED} taskStateNames;
extern taskStateNames taskState;

/* Ile mamy aktywnych zlecen */
extern int activeTasks;

/* stan globalny wykryty przez monitor */
extern int globalState;
/* ilu już odpowiedziało na GIVEMESTATE */
extern int numberReceived;

extern int tallowPrepared;
extern int numberReceivedP;


/* to może przeniesiemy do global... */
typedef struct {
    int ts;       /* timestamp (zegar lamporta */
    int src;      /* pole nie przesyłane, ale ustawiane w main_loop */

    int data;     /* przykładowe pole z danymi; można zmienić nazwę na bardziej pasującą */
	
	int data2;
} packet_t;
extern MPI_Datatype MPI_PAKIET_T;

/* Typy wiadomości */
#define END 1
#define BROADCAST 2
#define FIN 3
#define TASK_REQ 4
#define TASK_ACK 5
#define SHOP_REQ 6
#define SHOP_ACK 7


struct TaskQueue{
    struct TaskNode* head;
    struct TaskNode* tail;
};

struct TaskNode {
    int taskId;
    int giverId;
    struct TaskNode* next;
};

struct AckStateTask {
    struct AckStateNode* head;
    struct AckStateNode* tail;
};

struct AckStateNode {
    int taskId;
    int giverId;
    struct AckStateNode* next;
    struct AckStateNode* prev;
    taskStateNames* states;
};

void addAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
    struct AckStateNode* newAckState = (struct AckStateNode*)malloc(sizeof(struct AckStateNode));
    newAckState->taskId = taskId;
    newAckState->giverId = giverId;
    newAckState->states = (taskStateNames)malloc(sizeof(taskStateNames) * hunterTeamsNum);
    for(int i = 0 ; i<hunterTeamsNum;i++){
        newAckState->states[0] = REQUEST_NOT_SEND;
    }
    if(ackStateTask->head == NULL){
        ackStateTask->head = newAckState;
        ackStateTask->tail = newAckState;
    }
    else{
        struct AckStateNode* lastNode = ackStateTask->tail;
        ackStateTask->tail->next = newAckState;
        ackStateTask->tail = newAckState;
        ackStateTask->tail->prev = lastNode;
    }
}

struct AckStateNode* getAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            return node;
        }
    }
    return;
    if(taskQueue->head){
        struct TaskNode *tmp = taskQueue->head->next;
       
        struct TaskNode headCp;
        headCp.taskId = taskQueue->head->taskId;
        headCp.giverId = taskQueue->head->giverId;

        free(taskQueue->head);
        taskQueue->head=tmp;
        if(tmp==NULL){
            taskQueue->tail = NULL;
        }
        return headCp;
    }
    return;
}

// 0 - udalo sie usunac, -1 - nie udalo sie odnalezc wskazanego wezla
int deleteAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            struct AckStateNode* prevNode = node->prev;
            struct AckStateNode* nextNode = node->next;
            free(node);
            if(prevNode != NULL)
                prevNode->next = nextNode;
            if(nextNode != NULL)
                nextNode->prev = prevNode;
            return 0;
        }
    }
    return -1;
}




void addTask(struct TaskQueue *taskQueue, int taskId, int giverId);

struct TaskNode getTask(struct TaskQueue *taskQueue);

/* macro debug - działa jak printf, kiedy zdefiniowano
   DEBUG, kiedy DEBUG niezdefiniowane działa jak instrukcja pusta 
   
   używa się dokładnie jak printfa, tyle, że dodaje kolorków i automatycznie
   wyświetla rank

   w związku z tym, zmienna "rank" musi istnieć.

   w printfie: definicja znaku specjalnego "%c[%d;%dm [%d]" escape[styl bold/normal;kolor [RANK]
                                           FORMAT:argumenty doklejone z wywołania debug poprzez __VA_ARGS__
					   "%c[%d;%dm"       wyczyszczenie atrybutów    27,0,37
                                            UWAGA:
                                                27 == kod ascii escape. 
                                                Pierwsze %c[%d;%dm ( np 27[1;10m ) definiuje styl i kolor literek
                                                Drugie   %c[%d;%dm czyli 27[0;37m przywraca domyślne kolory i brak pogrubienia (bolda)
                                                ...  w definicji makra oznacza, że ma zmienną liczbę parametrów
                                            
*/

int incLamport();
int setMaxLamport(int);

int incLamport2();
int setMaxLamport2(int);

extern int zegar;
extern int zegar2;
#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [tid %d ts %d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank,zegar, ##__VA_ARGS__, 27,0,37);
#define debugGiver(FORMAT,...) printf("%c[%d;%dm [GIVER][tid %d ts %d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank,zegar, ##__VA_ARGS__, 27,0,37);
#define debugHunter(FORMAT,...) printf("%c[%d;%dm [HUNTER][tid %d ts %d ts2 %d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank,zegar,zegar2, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#define debugGiver(...) ;
#define debugHunter(...) ;
#endif

#define P_WHITE printf("%c[%d;%dm",27,1,37);
#define P_BLACK printf("%c[%d;%dm",27,1,30);
#define P_RED printf("%c[%d;%dm",27,1,31);
#define P_GREEN printf("%c[%d;%dm",27,1,33);
#define P_BLUE printf("%c[%d;%dm",27,1,34);
#define P_MAGENTA printf("%c[%d;%dm",27,1,35);
#define P_CYAN printf("%c[%d;%d;%dm",27,1,36);
#define P_SET(X) printf("%c[%d;%dm",27,1,31+(6+X)%7);
#define P_CLR printf("%c[%d;%dm",27,0,37);

/* printf ale z kolorkami i automatycznym wyświetlaniem RANK. Patrz debug wyżej po szczegóły, jak działa ustawianie kolorków */
#define println(FORMAT, ...) printf("%c[%d;%dm [tid %d ts %d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank,zegar, ##__VA_ARGS__, 27,0,37);

/* wysyłanie pakietu, skrót: wskaźnik do pakietu (0 oznacza stwórz pusty pakiet), do kogo, z jakim typem */
void sendPacket(packet_t *pkt, int destination, int tag);

void sendPacket2(packet_t *pkt, int destination, int tag);
void changeState( state_t );
void changeActiveTasks( int );
#endif
