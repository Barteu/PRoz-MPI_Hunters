#include "main.h"
#include "watek_glowny.h"
#include "obsluga_struktur.h"



// Main loop dla procesu Zleceniodawcy
void mainLoopGiver()
{
    srandom(time(NULL));
	taskId = 0;
    while (stan != InFinish) {
        int sleepTime = 4 + random()%5; 
        if (stan==InActive) {
			printlnGiver("Jestem w stanie Active");
			// Wysylam BROADCAST
			packet_t message;
			message.data = taskId; // ID zlecenia
			message.data2 = rank; // ID zleceniodawcy
			for(int i = 0; i < hunterTeamsNum; i++){
				sendPacket(&message, i, BROADCAST);
			}
			printlnGiver("BROADCAST {taskId:%d, giverId:%d} wyslany" , taskId,rank);
			taskId++;
			changeActiveTasks(1);
			
			if(activeTasks > upperLimit){
				changeState(InOverload);
			}
			else {
				sleep(sleepTime);
			}
		}
		else if(stan==InOverload){
			printlnGiver("Jestem w stanie Overload");
			while(stan==InOverload){
				sleep(1);
			}
			//pthread_mutex_lock(&sleepMut);
			// pthread_cond_wait(&cond, &sleepMut);
			// pthread_mutex_unlock(&sleepMut);
		}
	
    }
}

void mainLoopHunter(){
	srandom(time(NULL));
	while(stan!=InFinish){
		// cos tam robi
		if(stan==InSearch){
			printlnHunter("Jestem w stanie SEARCH");
			while(stan==InSearch){
				sleep(1);
			}
			//pthread_mutex_lock(&sleepMut);
			// pthread_cond_wait(&cond, &sleepMut);
			// pthread_mutex_unlock(&sleepMut);
		}
		else if(stan==InWait){
			printlnHunter("Jestem w stanie WAIT");
			//pthread_mutex_lock(&sleepMut2);
			while(stan==InWait){
				sleep(1);
			}
			// pthread_mutex_unlock(&sleepMut2);
			debugHunter("Wychodze ze stanu WAIT");
		}
		else if(stan==InShop){
			printlnHunter("Jestem w stanie SHOP");
        	int sleepTime = 3 + random()%5;
			sleep(sleepTime);
			printlnHunter("Wychodze ze stanu SHOP");
			changeState(InTask);
			
		}
		else if(stan==InTask){
			printlnHunter("Jestem w stanie TASK");
			packet_t message;
			for(int i = 0; i < hunterTeamsNum; i++){
				if(i != rank && waitQueueShop[i]!=-1){
					sendPacket2(&message, i, SHOP_ACK);
					waitQueueShop[i] = -1;
				}
			}
			ackNumShop = 0;
			int sleepTime = 5 + random()%5;
			sleep(sleepTime);

			tasksDoneHunter ++;
			printlnHunter("Wychodze ze stanu TASK, zrealizowalem %d zadan",tasksDoneHunter);
			

			packet_t message2;
			int ids[2];
			getTask(&taskQueue, ids);

			message2.data = ids[0];
			message2.data2 = ids[1];
			for(int i = 0; i < hunterTeamsNum; i++){
				if(i != rank)
					sendPacket(&message2, i, FIN);
			}
			sendPacket(&message2, ids[1], FIN);
			
			deleteAckState(&ackStateTask, ids[0],ids[1]);
			deleteRequestPriority(&requestPriorityTask, ids[0], ids[1]);
			
			if(isAnyTaskInQueue(&taskQueue)){
				packet_t pakiet;
				pthread_mutex_lock(&lampMut2);
				waitQueueShop[rank] = zegar2;
				pthread_mutex_unlock(&lampMut2);
				pakiet.priority = waitQueueShop[rank];
				for(int i = 0; i < hunterTeamsNum; i++){
					if(i != rank){
						sendPacket2(&pakiet, i, SHOP_REQ);
					}
				}
				ackNumShop = 0;
				
				printlnHunter("Mam kolejne zadanie w kolejce, Wchodze do WAIT");
				changeState(InWait);
			}
			else{
				
				debugHunter("Bede ubiegal sie o kolejne zadanie, Wychodze do SEARCH");
				changeState(InSearch);	
				sendOldRequests(&requestPriorityTask, &ackStateTask);
			}
		
		}
	}
}