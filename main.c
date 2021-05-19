#include "main.h"
#include "watek_komunikacyjny.h"
#include "watek_glowny.h"
#include "monitor.h"
/* wątki */
#include <pthread.h>

/* sem_init sem_destroy sem_post sem_wait */
//#include <semaphore.h>
/* flagi dla open */
//#include <fcntl.h>

state_t stan;
volatile char end = FALSE;
int size,rank; /* nie trzeba zerować, bo zmienna globalna statyczna */
MPI_Datatype MPI_PAKIET_T;
pthread_t threadKom, threadMon;

int upperLimit, lowerLimit, hunterTeamsNum, shopSize;

struct TaskQueue taskQueue;
struct AckStateTask ackStateTask;
struct RequestPriorityTask requestPriorityTask;



// taskGiver
int activeTasks;


pthread_mutex_t activeTasksMut = PTHREAD_MUTEX_INITIALIZER;

int zegar, zegar2;
pthread_mutex_t lampMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lampMut2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t senMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t taskQueueMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ackStateTaskMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t requestPriorityTaskMut = PTHREAD_MUTEX_INITIALIZER;

void addTask(struct TaskQueue *taskQueue, int taskId, int giverId){
    pthread_mutex_lock(&taskQueueMut);
    struct TaskNode* newTask = (struct TaskNode*)malloc(sizeof(struct TaskNode));
    newTask->taskId = taskId;
    newTask->giverId = giverId;
    newTask->next = NULL;
    if(taskQueue->head == NULL){
        taskQueue->head = newTask;
        taskQueue->tail = newTask;
    }
    else{
        taskQueue->tail->next = newTask;
        taskQueue->tail = newTask;
    }
    pthread_mutex_unlock(&taskQueueMut);
}

void getTask(struct TaskQueue *taskQueue, int *ids){
    pthread_mutex_lock(&taskQueueMut);
    ids[0] = -1;
    ids[1] = -1;
    if(taskQueue->head){
        struct TaskNode *tmp = taskQueue->head->next;
       
        ids[0] = taskQueue->head->taskId;
        ids[1] = taskQueue->head->giverId;

        free(taskQueue->head);
        taskQueue->head=tmp;
        if(tmp==NULL){
            taskQueue->tail = NULL;
        }
    }
    pthread_mutex_unlock(&taskQueueMut);
}

int isTaskInQueue(struct TaskQueue *taskQueue, int taskId, int giverId){
    pthread_mutex_lock(&taskQueueMut);
    struct TaskNode *tmp = taskQueue->head;
    while(tmp!=NULL){
        if(tmp->taskId == taskId && tmp->giverId == giverId){
        pthread_mutex_unlock(&taskQueueMut);
        return 1;
        }
        tmp = tmp->next;
    }
    pthread_mutex_unlock(&taskQueueMut);
    return 0;
}


void addAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* newAckState = (struct AckStateNode*)malloc(sizeof(struct AckStateNode));
    newAckState->taskId = taskId;
    newAckState->giverId = giverId;
    newAckState->next = NULL;
    newAckState->prev = NULL;
    newAckState->states = (taskStateNames*)malloc(sizeof(taskStateNames) * hunterTeamsNum);
    for(int i = 0 ; i<hunterTeamsNum;i++){
        newAckState->states[i] = REQUEST_NOT_SEND;
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
    pthread_mutex_unlock(&ackStateTaskMut);
}

void forwardAllAck(struct RequestPriorityTask* requestPriorityTask, struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);   
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *requestNode = requestPriorityTask->head;
    struct AckStateNode *ackNode;
    packet_t pakiet;

    while(requestNode != NULL){
        if(requestNode->taskId != taskId || requestNode->giverId != giverId){
            ackNode = ackStateTask->head;
            for(int i = 0; i < hunterTeamsNum; i++){
                if(requestNode->priorities[i] != -1 && i != rank){
                    
                    pakiet.data = requestNode->taskId;
                    pakiet.data2 = requestNode->giverId;
                    sendPacket(&pakiet, i, TASK_ACK);
                    requestNode->priorities[i] = -1;
                }
            }
            while(ackNode != NULL){
                if(ackNode->taskId == requestNode->taskId  && ackNode->giverId == requestNode->giverId){
                    for(int i = 0; i < hunterTeamsNum; i++){
                        ackNode->states[i] = REQUEST_NOT_SEND;
                    }
                }
                ackNode = ackNode->next;
            }
            
        }
        requestNode = requestNode->next;
    }
    
    pthread_mutex_unlock(&requestPriorityTaskMut);
    pthread_mutex_unlock(&ackStateTaskMut);     

}

void forwardAck(struct RequestPriorityTask* requestPriorityTask, struct AckStateTask *ackStateTask, int taskId, int giverId, taskStateNames ackState){
    pthread_mutex_lock(&ackStateTaskMut);   
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *requestNode = requestPriorityTask->head;
    struct AckStateNode *ackNode;

    while(requestNode != NULL){
        if(requestNode->taskId == taskId && requestNode->giverId == giverId){
            ackNode = ackStateTask->head;
            for(int i = 0; i < hunterTeamsNum; i++){
                if(requestNode->priorities[i] != -1){

                    if(ackState == REJECTED && i!=rank){
                        packet_t pakiet;
                        pakiet.data = taskId;
                        pakiet.data2 = giverId;
                        sendPacket(&pakiet ,i,TASK_ACK);
                    }
                    requestNode->priorities[i] = -1;
                }
            }
            while(ackNode != NULL){
                if(ackNode->taskId == requestNode->taskId  && ackNode->giverId == requestNode->giverId){
                    for(int i = 0; i < hunterTeamsNum; i++){
                        ackNode->states[i] = ackState;
                    }
                    break;
                }
                ackNode = ackNode->next;
            }
            break;
        }
        requestNode = requestNode->next;
    }
    
    pthread_mutex_unlock(&requestPriorityTaskMut);
    pthread_mutex_unlock(&ackStateTaskMut);     

}


// struct AckStateNode getAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
//     pthread_mutex_lock(&ackStateTaskMut);
//     struct AckStateNode* node;
//     node = ackStateTask->head;
//     while(node){
//         if(node->taskId == taskId && node->giverId == giverId){
//             struct AckStateNode tmp;
//             tmp.taskId = node->taskId;
//             tmp.giverId = node->giverId;
//             tmp.states = (taskStateNames)malloc(sizeof(taskStateNames) * hunterTeamsNum);
//             for(int i = 0; i < hunterTeamsNum; i++){
//                 tmp.states[i] = node->states[i];
//             }
//             pthread_mutex_unlock(&ackStateTaskMut);
//             return tmp;
//         }
//         node = node->next;
//     }
//     pthread_mutex_unlock(&ackStateTaskMut);
//     return;
// }

taskStateNames getAckStateByHunter(struct AckStateTask *ackStateTask, int taskId, int giverId, int hunterId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){  
            taskStateNames tmpState = node->states[hunterId];
            pthread_mutex_unlock(&ackStateTaskMut);
            return tmpState;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&ackStateTaskMut);
    return -1;
}

int getAckStateTask(struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    int counter = 0;
    while(node != NULL){
        if(node->taskId == taskId && node->giverId == giverId){  
            for(int i = 0; i < hunterTeamsNum; i++){
                if(node->states[i] == ACK_RECEIVED){
                    counter++;
                }
            }
           
        }
        node = node->next;
    }  
    pthread_mutex_unlock(&ackStateTaskMut);
    if(counter==(hunterTeamsNum-1)){
        return 1;
    }
    else{
        return 0;
    }
    
}

void setAckStateByHunter(struct AckStateTask *ackStateTask, int taskId, int giverId, int hunterId, taskStateNames state){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){  
            node->states[hunterId] = state;
            pthread_mutex_unlock(&ackStateTaskMut);
            return;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&ackStateTaskMut);
}



// 1 - udalo sie usunac, 0 - nie udalo sie odnalezc wskazanego wezla
int deleteAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            struct AckStateNode* prevNode = node->prev;
            struct AckStateNode* nextNode = node->next;

            free(node);
            if(prevNode != NULL)
                prevNode->next = nextNode;
            else{
                ackStateTask->head = nextNode;
            }
            if(nextNode != NULL)
                nextNode->prev = prevNode;
            else{
                ackStateTask->tail = nextNode;
            }
            pthread_mutex_unlock(&ackStateTaskMut);
            return 1;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&ackStateTaskMut);
    return 0;
}

void addRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId){
     pthread_mutex_lock(&lampMut);
     pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode* newRequestPriority = (struct RequestPriorityNode*)malloc(sizeof(struct RequestPriorityNode));
    newRequestPriority->taskId = taskId;
    newRequestPriority->giverId = giverId;
    newRequestPriority->next = NULL;
    newRequestPriority->prev = NULL;
    newRequestPriority->priorities = (int*)malloc(sizeof(int) * hunterTeamsNum);
    for(int i = 0 ; i<hunterTeamsNum;i++){
         newRequestPriority->priorities[i] = -1;
    }
    newRequestPriority->priorities[rank] = zegar;
    if(requestPriorityTask->head == NULL){
        requestPriorityTask->head = newRequestPriority;
        requestPriorityTask->tail = newRequestPriority;
    }
    else{
        struct RequestPriorityNode* lastNode = requestPriorityTask->tail;
        requestPriorityTask->tail->next =  newRequestPriority;
        requestPriorityTask->tail =  newRequestPriority;
        requestPriorityTask->tail->prev = lastNode;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    pthread_mutex_unlock(&lampMut);
}

// struct RequestPriorityNode* getRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId){
//     pthread_mutex_lock(&requestPriorityTaskMut);
//     struct RequestPriorityNode* node;
//     node = requestPriorityTask->head;
//     while(node){
//         if(node->taskId == taskId && node->giverId == giverId){
//             return node;
//         }
//         node = node->next;
//     }
//     pthread_mutex_unlock(&requestPriorityTaskMut);
//     return;
// }


 int getRequestPriorityByHunter(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId, int hunterId){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            int tmpPriority = node->priorities[hunterId];
            pthread_mutex_unlock(&requestPriorityTaskMut);
            return tmpPriority;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    return -1;
}


 int isTaskRequested(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            for(int i = 0 ; i < hunterTeamsNum; i++){
                if(i != rank && node->priorities[i] != -1){
                    pthread_mutex_unlock(&requestPriorityTaskMut);
                    return 1;
                }
            }
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    return 0;
}


 void setRequestPriorityByHunter(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId, int hunterId, int priority){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            node->priorities[hunterId] = priority;
            break;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
}

// 1 - udalo sie usunac, 0 - nie udalo sie odnalezc wskazanego wezla
int deleteRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode* node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            struct RequestPriorityNode* prevNode = node->prev;
            struct RequestPriorityNode* nextNode = node->next;
            free(node);
            if(prevNode != NULL)
                prevNode->next = nextNode;
            else{
                requestPriorityTask->head = nextNode;
            }
            if(nextNode != NULL)
                nextNode->prev = prevNode;
            else{
                requestPriorityTask->tail = nextNode;
            }
            pthread_mutex_unlock(&requestPriorityTaskMut);
            return 1;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    return 0;
}


void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: chcemy %d. Co otrzymamy?\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	    break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n"); /* tego chcemy. Wszystkie inne powodują problemy */
	    break;
        default: printf("Nikt nic nie wie\n");
    }
}

/* srprawdza, czy są wątki, tworzy typ MPI_PAKIET_T
*/
void inicjuj(int *argc, char ***argv)
{
    int provided;
    MPI_Init_thread(argc, argv,MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    const int nitems=5; /* bo packet_t ma trzy pola */
    int       blocklengths[5] = {1,1,1,1,1};
    MPI_Datatype typy[5] = {MPI_INT, MPI_INT, MPI_INT,MPI_INT, MPI_INT};

    MPI_Aint     offsets[5]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
	offsets[3] = offsetof(packet_t, data2);
    offsets[4] = offsetof(packet_t, priority);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    shopSize = atoi((*argv)[4]);
    if(rank< atoi((*argv)[1])){
        taskQueue.head = NULL;
        taskQueue.tail = NULL;
        ackStateTask.head = NULL;
        ackStateTask.tail = NULL;
        requestPriorityTask.head = NULL;
        requestPriorityTask.tail = NULL;
        stan = InSearch;
        pthread_create( &threadKom, NULL, startKomWatekHunter , 0);
    } 
    else
    {
        stan = InActive;
        lowerLimit = atoi((*argv)[2]);
        upperLimit = atoi((*argv)[3]);
        activeTasks = 0;
        pthread_create( &threadKom, NULL, startKomWatekGiver , 0);
    }
   
    if (rank==0) {
	pthread_create( &threadMon, NULL, startMonitor, 0);
    }

}

/* usunięcie zamkków, czeka, aż zakończy się drugi wątek, zwalnia przydzielony typ MPI_PAKIET_T
   wywoływane w funkcji main przed końcem
*/
void finalizuj()
{
    pthread_mutex_destroy( &stateMut);
    /* Czekamy, aż wątek potomny się zakończy */
    println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(threadKom,NULL);
    if (rank==0) pthread_join(threadMon,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}


/* opis patrz main.h */
void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    pkt->src = rank;
	pkt->ts = incLamport();
    MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    if (freepkt) free(pkt);
}

int incLamport(){
	
	pthread_mutex_lock(&lampMut);
	if(stan==InFinish)
	{
		pthread_mutex_unlock(&lampMut);
		return zegar;
	}
	zegar++; int tmp = zegar;
	pthread_mutex_unlock(&lampMut);
	return tmp;
}

int setMaxLamport(int new){
	
	pthread_mutex_lock(&lampMut);
	if(stan==InFinish)
	{
		pthread_mutex_unlock(&lampMut);
		return zegar;
	}
	zegar = (new>zegar)?new:zegar;
	zegar++;
	int tmp = zegar;
	pthread_mutex_unlock(&lampMut);
	return tmp;
}

/* opis patrz main.h */
void sendPacket2(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    pkt->src = rank;
	pkt->ts = incLamport2();
    MPI_Send( pkt, 1, MPI_PAKIET_T, destination, tag, MPI_COMM_WORLD);
    if (freepkt) free(pkt);
}

int incLamport2(){
	
	pthread_mutex_lock(&lampMut2);
	if(stan==InFinish)
	{
		pthread_mutex_unlock(&lampMut2);
		return zegar2;
	}
	zegar2++; int tmp = zegar2;
	pthread_mutex_unlock(&lampMut2);
	return tmp;
}

int setMaxLamport2(int new){
	
	pthread_mutex_lock(&lampMut2);
	if(stan==InFinish)
	{
		pthread_mutex_unlock(&lampMut2);
		return zegar2;
	}
	zegar2 = (new>zegar2)?new:zegar2;
	zegar2++;
	int tmp = zegar2;
	pthread_mutex_unlock(&lampMut2);
	return tmp;
}


void changeActiveTasks( int newActiveTasks )
{
    pthread_mutex_lock( &activeTasksMut );
    if (stan==InFinish) { 
	pthread_mutex_unlock( &activeTasksMut );
        return;
    }
    activeTasks += newActiveTasks;
    pthread_mutex_unlock( &activeTasksMut );
}

void changeState( state_t newState )
{
    pthread_mutex_lock( &stateMut );
    if (stan==InFinish) { 
	pthread_mutex_unlock( &stateMut );
        return;
    }
    stan = newState;
    pthread_mutex_unlock( &stateMut );
}

int main(int argc, char **argv)
{
    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj(&argc,&argv); // tworzy wątek komunikacyjny w "watek_komunikacyjny.c"
  
    hunterTeamsNum = atoi(argv[1]);
    if(rank < hunterTeamsNum){
        // ID procesu mniejsze niz numer druzyn lowcow -> proces reprezentuje druzyne lowcow 
        mainLoopHunter();  // w pliku "watek_glowny.c"
    }
    else{
        // ID procesu wieksze niz numer druzyn lowcow -> proces reprezentuje zleceniodawce
        mainLoopGiver(); // w pliku "watek_glowny.c"
    }
    finalizuj();
    return 0;
}

