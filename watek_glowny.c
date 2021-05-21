#include "main.h"
#include "watek_glowny.h"
#include "obsluga_struktur.h"


// Main loop dla procesu Zleceniodawcy
void mainLoopGiver()
{
    srandom(rank);
	taskId = 0;
    while (stan != InFinish) {
        int sleepTime = 1 + random()%5; 
        if (stan==InActive) {
			debugGiver("Jestem w stanie Active");
			// Wysylam BROADCAST
			packet_t message;
			message.data = taskId; // ID zlecenia
			message.data2 = rank; // ID zleceniodawcy
			for(int i = 0; i < hunterTeamsNum; i++){
				sendPacket(&message, i, BROADCAST);
			}
			debugGiver("BROADCAST [tid:%d, taskId:%d] wyslany" , rank,taskId);
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
			debugGiver("Jestem w stanie Overload");
			pthread_mutex_lock(&activeTasksMut);
			if(activeTasks < lowerLimit){
				pthread_mutex_unlock(&activeTasksMut);
				changeState(InActive);
			}
			else{
				pthread_mutex_unlock(&activeTasksMut);
			}
			sleep(1);
			// Tutaj chyba aktywne czekanie jest, nie wiem czy moze byc :/ ?
		}
		/*debug("Zmieniam stan na wysyłanie");
		changeState( InSend );
		packet_t *pkt = malloc(sizeof(packet_t));
		pkt->data = perc;
                changeTallow( -perc);
                sleep( SEC_IN_STATE); // to nam zasymuluje, że wiadomość trochę leci w kanale
                                      // bez tego algorytm formalnie błędny za każdym razem dawałby poprawny wynik
		sendPacket( pkt, (rank+1)%size,TALLOWTRANSPORT);
		changeState( InRun );
		debug("Skończyłem wysyłać");
            } 
			else if(stan==InTallows)
			{
				if(tallowPrepared==0)
				{
				changePrepared(TRUE);
					
				packet_t *pkt = malloc(sizeof(packet_t));
				pkt->data = 1;
                sleep( SEC_IN_STATE); // to nam zasymuluje, że wiadomość trochę leci w kanale
                                      // bez tego algorytm formalnie błędny za każdym razem dawałby poprawny wynik
				sendPacket( pkt,0,TALLOWPREPSTATE);

				debug("Wysyłam info że juz skończyłem wysyłać");
				
					
				}
				
				
            }
        }
        sleep(SEC_IN_STATE);*/
    }
}

void mainLoopHunter(){
	while(stan!=InFinish){
		// cos tam robi
		if(stan==InSearch){
			debugHunter("Jestem w stanie SEARCH");
			sleep(5);
		}
		else if(stan==InWait){
			debugHunter("Jestem w stanie WAIT");
			sleep(5);
		}
		else if(stan==InShop){
			debugHunter("Jestem w stanie INSHOP");
			srandom(rank);
        	int sleepTime = 3 + random()%5;
			
			debugHunter("Wychodze ze stanu INSHOP");
			changeState(InTask);
			
		}
		else if(stan==InTask){
			debugHunter("Jestem w stanie INTASK");
			srandom(rank);
			packet_t message;
			for(int i = 0; i < hunterTeamsNum; i++){
				if(i != rank && waitQueueShop[i]!=-1){
					sendPacket2(&message, i, SHOP_ACK);
					waitQueueShop[i] = -1;
				}
			}
			ackNumShop = 0;
			int sleepTime = 5 + random()%5;

			debugHunter("Wychodze ze stanu INTASK");
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
				
				debugHunter("Wychodze do WAIT");
				changeState(InWait);
			}
			else{
				
				debugHunter("Wychodze do INSEARCH");
				changeState(InSearch);	
				sendOldRequests(&requestPriorityTask, &ackStateTask);
			}
		
		}
	}
}