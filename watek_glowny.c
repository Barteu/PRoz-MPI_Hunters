#include "main.h"
#include "watek_glowny.h"


// Main loop dla procesu Zleceniodawcy
void mainLoopGiver()
{
    srandom(rank);
	taskId = 0;
	activeTasksMut =  = PTHREAD_MUTEX_INITIALIZER;
    while (stan != InFinish) {
        int sleepTime = 1 + random()%5; 
        if (stan==InActive) {
			debug("Jestem w stanie Active");
			// Wysylam BROADCAST
			debug("Wysylam BROADCAST");
			packet_t *message = malloc(sizeof(packet_t));
			message->data = taskId; // ID zlecenia
			message->data2 = rank; // ID zleceniodawcy
			for(int i = 0; i < hunterTeamsNum; i++){
				sendPacket(message, i, BROADCAST);
			}
			debug("BROADCAST wyslany");
			taskId++;
			pthread_mutex_lock(&activeTasksMut);
			activeTasks++;
			pthread_mutex_unlock(&activeTasksMut);
			free(message);
			if(activeTasks > upperLimit){
				changeState(InOverload);
			}
			else {
				sleep(sleepTime);
			}
		}
		else if(stan==InOverload){
			debug("Jestem w stanie Overload");
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
	}
}