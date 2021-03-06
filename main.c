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

int tasksDoneHunter;
int tasksDoneGiver;


// taskGiver
int activeTasks;

int ackNumShop;
int* waitQueueShop;


pthread_mutex_t activeTasksMut = PTHREAD_MUTEX_INITIALIZER;

int zegar, zegar2;
pthread_mutex_t lampMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lampMut2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t senMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t stateMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t taskQueueMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ackStateTaskMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t requestPriorityTaskMut = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t sleepMut;// = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t sleepMut2;// = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;



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

    pthread_mutex_init(&sleepMut, NULL);
    pthread_mutex_init(&sleepMut2, NULL);

    pthread_mutex_lock(&sleepMut);
    pthread_mutex_lock(&sleepMut2);

    shopSize = atoi((*argv)[4]);
    if(rank< atoi((*argv)[1])){
        zegar = 1;
        zegar2 = 1;
        taskQueue.head = NULL;
        taskQueue.tail = NULL;
        ackStateTask.head = NULL;
        ackStateTask.tail = NULL;
        requestPriorityTask.head = NULL;
        requestPriorityTask.tail = NULL;
        stan = InSearch;
        waitQueueShop = (int*)malloc(sizeof(int)*size);
        ackNumShop = 0;
        for(int i=0; i<size;i++){
            waitQueueShop[i] = -1;
        }
        tasksDoneHunter = 0;
        pthread_create( &threadKom, NULL, startKomWatekHunter , 0);
        
    } 
    else
    {
        zegar = 1;
        stan = InActive;
        lowerLimit = atoi((*argv)[2]);
        upperLimit = atoi((*argv)[3]);
        activeTasks = 0;
        tasksDoneGiver= 0;
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
    debugGiver("Liczba moich aktualnie niewykonanych zadan: %d (LM: %d HM: %d)",activeTasks,lowerLimit,upperLimit);
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

const char * getStateName(){
    char *state;
  
    if(stan==InActive)
    {
        state = "Active";
    }
    else if(stan == InOverload){
        state = "Overload";
    }
    else if(stan == InSearch){
        state = "Search";
    }
    else if(stan == InWait){
        state = "Wait";
    }
    else if(stan == InShop){
        state = "Shop";
    }
     else if(stan == InTask){
        state = "Task";
    }
    else if(stan == InFinish){
        state = "Finish";
    }
    else{
        state = "unknown";
    }

    return state;
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

