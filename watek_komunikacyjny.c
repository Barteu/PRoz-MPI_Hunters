#include "main.h"
#include "watek_komunikacyjny.h"
#include "obsluga_struktur.h"


void* startKomWatekGiver(void* ptr){
	MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
	/* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( stan!=InFinish ) {
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		// Aktualizujemy zegar Lamporta procesu Zleceniodawcy
		setMaxLamport(pakiet.ts);
		switch ( status.MPI_TAG ) {
		case END:
			changeState(InFinish);
			break;
		case FIN:
			tasksDoneGiver++;
			debugGiver("Dostałem FIN od (tid:%d), zrealizowano %d moich zadań", pakiet.src,tasksDoneGiver);
			changeActiveTasks(-1);
			if(activeTasks < lowerLimit && stan==InOverload){
				changeState(InActive);	
				pthread_cond_signal(&cond);
			}
			break;
		}

	}
}

void* startKomWatekHunter(void* ptr){
	MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
	int time2;
	/* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( stan!=InFinish ) {
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		// Aktualizujemy zegar Lamporta procesu Lowcy
		if(status.MPI_TAG==SHOP_ACK || status.MPI_TAG==SHOP_REQ)
		{
			time2 = setMaxLamport2(pakiet.ts);
		}
		else{
			setMaxLamport(pakiet.ts);
		}
		

		if(stan==InSearch){

			switch ( status.MPI_TAG ) {
			case END:
				changeState(InFinish);
				break;
			case BROADCAST:
				debugHunter("Dostałem BROADCAST: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data,pakiet.src,pakiet.src);
				addAckState(&ackStateTask, pakiet.data, pakiet.src);
				addRequestPriority(&requestPriorityTask, pakiet.data,pakiet.src);
				pakiet.priority = getRequestPriorityByHunter(&requestPriorityTask, pakiet.data, pakiet.src, rank);
				for(int i = 0; i < hunterTeamsNum; i++){
					if(i!=rank){
						if(getAckStateByHunter(&ackStateTask, pakiet.data, pakiet.data2, i) == REQUEST_NOT_SEND)
						{
							sendPacket(&pakiet, i, TASK_REQ);
							setAckStateByHunter(&ackStateTask, pakiet.data, pakiet.data2, i, REQUEST_SEND);
						}
					}
				}
				break;
			case TASK_REQ:
				debugHunter("Dostałem TASK_REQ: {taskId: %d, giverId: %d, priority: %d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.priority, pakiet.src);
				int myPriority = getRequestPriorityByHunter(&requestPriorityTask, pakiet.data, pakiet.data2, rank);
				if(myPriority != -1){
					if(myPriority < pakiet.priority || (myPriority==pakiet.priority && rank<pakiet.src) ){
						debugHunter("TASK_REQ uznaje jako ACK: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2,pakiet.src);
						setAckStateByHunter(&ackStateTask,pakiet.data,pakiet.data2, pakiet.src, ACK_RECEIVED);
						setRequestPriorityByHunter(&requestPriorityTask, pakiet.data, pakiet.data2, pakiet.src, pakiet.priority);
						//sprawdzamy czy nie mozemy przyjac tego zlecenia
						if(getAckStateTask(&ackStateTask,pakiet.data,pakiet.data2)){
							debugHunter("Zlecenie przyjete: {taskId:%d, giverId:%d}", pakiet.data, pakiet.data2);
							addTask(&taskQueue, pakiet.data, pakiet.data2);
							forwardAllAck(&requestPriorityTask, &ackStateTask,pakiet.data, pakiet.data2);
							changeState(InWait);
							pthread_cond_signal(&cond);
							waitQueueShop[rank] = time2;
							pakiet.priority = waitQueueShop[rank];
							for(int i = 0; i < hunterTeamsNum; i++){
								if(i != rank){
									sendPacket2(&pakiet, i, SHOP_REQ);
								}
							}
							ackNumShop = 0;
						}
					}
				}
				else
				{
					debugHunter("Wysylam ACK: {taskId:%d, giverId:%d} do (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
					sendPacket(&pakiet, pakiet.src, TASK_ACK);
				}
				break;
			case TASK_ACK:
				debugHunter("Dostałem TASK_ACK: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				setAckStateByHunter(&ackStateTask,pakiet.data,pakiet.data2, pakiet.src, ACK_RECEIVED);
				//sprawdzamy czy nie mozemy przyjac tego zlecenia
				if(getAckStateTask(&ackStateTask,pakiet.data,pakiet.data2)){
					addTask(&taskQueue, pakiet.data, pakiet.data2);
					forwardAllAck(&requestPriorityTask, &ackStateTask,pakiet.data, pakiet.data2);
					changeState(InWait);
					pthread_cond_signal(&cond);
					waitQueueShop[rank] = time2;
					pakiet.priority = waitQueueShop[rank];
					for(int i = 0; i < hunterTeamsNum; i++){
						if(i != rank){
							sendPacket2(&pakiet, i, SHOP_REQ);
						}
					}
					ackNumShop = 0;
			}
				break;
			case FIN:
				debugHunter("Dostałem FIN: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				deleteAckState(&ackStateTask, pakiet.data, pakiet.data2);
				deleteRequestPriority(&requestPriorityTask, pakiet.data, pakiet.data2);
				break;
			case SHOP_ACK:
				// Ignorujemy wiadomosc
				break;
			case SHOP_REQ:
				pakiet.priority = waitQueueShop[rank];
				sendPacket2(&pakiet, pakiet.src, SHOP_ACK);
				break;
			}

		}
		else if(stan==InWait){
			switch(status.MPI_TAG){
			case END:
				changeState(InFinish);
				break;
			case BROADCAST:
				debugHunter("Dostałem BROADCAST: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data,pakiet.src,pakiet.src);
				
				addAckState(&ackStateTask, pakiet.data, pakiet.src);
				addRequestPriority(&requestPriorityTask, pakiet.data,pakiet.src);
				break;
			
			case TASK_REQ:
				if(!isTaskInQueue(&taskQueue,pakiet.data, pakiet.data2))
				{
					sendPacket(&pakiet, pakiet.src, TASK_ACK);
					forwardAck(&requestPriorityTask, &ackStateTask, pakiet.data, pakiet.data2, REQUEST_NOT_SEND);
				}
				break;
			
			case TASK_ACK:
				debugHunter("Dostałem TASK_ACK: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				setAckStateByHunter(&ackStateTask,pakiet.data,pakiet.data2, pakiet.src, ACK_RECEIVED);
				//sprawdzamy czy nie mozemy przyjac tego zlecenia
				if(getAckStateTask(&ackStateTask,pakiet.data,pakiet.data2)){
					if(isTaskRequested(&requestPriorityTask, pakiet.data, pakiet.data2)){
						forwardAck(&requestPriorityTask, &ackStateTask, pakiet.data, pakiet.data2, REJECTED);

					}else{
						addTask(&taskQueue, pakiet.data, pakiet.data2);
					}
					
				}
				break;
			
			case FIN:
				debugHunter("Dostałem FIN: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				deleteAckState(&ackStateTask, pakiet.data, pakiet.data2);
				deleteRequestPriority(&requestPriorityTask, pakiet.data, pakiet.data2);
				break;
			case SHOP_ACK:
				ackNumShop += 1;
				if(ackNumShop == (hunterTeamsNum - 1) - (shopSize - 1)){
					changeState(InShop);
					pthread_cond_broadcast(&cond);
				}
				break;
			case SHOP_REQ:
				if(waitQueueShop[rank] == -1){
					sendPacket2(&pakiet, pakiet.src, SHOP_ACK);
				}
				else if(waitQueueShop[rank] < pakiet.priority || (waitQueueShop[rank] == pakiet.priority && rank < pakiet.src)){
					waitQueueShop[pakiet.src] = pakiet.priority;
					ackNumShop += 1; 
					if(ackNumShop == (hunterTeamsNum - 1) - (shopSize - 1)){
						changeState(InShop);
						pthread_cond_broadcast(&cond);
					}
				}
				
				break;
			}
		}
		else if(stan==InShop){
			switch(status.MPI_TAG){
			case END:
				changeState(InFinish);
				break;
			case BROADCAST:
				debugHunter("Dostałem BROADCAST {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data,pakiet.src,pakiet.src);
					//debugHunter("SHOP 1Koniec przetwarzania BROADCAST");
				addAckState(&ackStateTask, pakiet.data, pakiet.src);
					//debugHunter("SHOP 2Koniec przetwarzania BROADCAST");
				addRequestPriority(&requestPriorityTask, pakiet.data,pakiet.src);
					//debugHunter("SHOP 3Koniec przetwarzania BROADCAST");
				break;
			
			case TASK_REQ:
				if(!isTaskInQueue(&taskQueue,pakiet.data, pakiet.data2))
				{
					sendPacket(&pakiet, pakiet.src, TASK_ACK);
					forwardAck(&requestPriorityTask, &ackStateTask, pakiet.data, pakiet.data2, REQUEST_NOT_SEND);
				}
				break;
			
			case TASK_ACK:
				debugHunter("Dostałem TASK_ACK: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				setAckStateByHunter(&ackStateTask,pakiet.data,pakiet.data2, pakiet.src, ACK_RECEIVED);
				//sprawdzamy czy nie mozemy przyjac tego zlecenia
				if(getAckStateTask(&ackStateTask,pakiet.data,pakiet.data2)){
					if(isTaskRequested(&requestPriorityTask, pakiet.data, pakiet.data2)){
						forwardAck(&requestPriorityTask, &ackStateTask, pakiet.data, pakiet.data2, REJECTED);

					}else{
						addTask(&taskQueue, pakiet.data, pakiet.data2);
					}
					
				}
				break;
			
			case FIN:
				debugHunter("Dostałem FIN: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				deleteAckState(&ackStateTask, pakiet.data, pakiet.data2);
				deleteRequestPriority(&requestPriorityTask, pakiet.data, pakiet.data2);
				break;
			case SHOP_ACK:
				// Ignorujemy wiadomosc
				break;
			case SHOP_REQ:
				waitQueueShop[pakiet.src] = pakiet.priority;
				break;
			}
		}
		else if(stan==InTask){
			switch(status.MPI_TAG){
			case END:
				changeState(InFinish);
				break;
			case BROADCAST:
				debugHunter("Dostałem BROADCAST: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data,pakiet.src,pakiet.src);
				
				addAckState(&ackStateTask, pakiet.data, pakiet.src);
				addRequestPriority(&requestPriorityTask, pakiet.data,pakiet.src);
				break;
			
			case TASK_REQ:
				if(!isTaskInQueue(&taskQueue,pakiet.data, pakiet.data2))
				{
					sendPacket(&pakiet, pakiet.src, TASK_ACK);
					forwardAck(&requestPriorityTask, &ackStateTask, pakiet.data, pakiet.data2, REQUEST_NOT_SEND);
				}
				break;
			
			case TASK_ACK:
				debugHunter("Dostałem TASK_ACK: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				setAckStateByHunter(&ackStateTask,pakiet.data,pakiet.data2, pakiet.src, ACK_RECEIVED);
				//sprawdzamy czy nie mozemy przyjac tego zlecenia
				if(getAckStateTask(&ackStateTask,pakiet.data,pakiet.data2)){
					if(isTaskRequested(&requestPriorityTask, pakiet.data, pakiet.data2)){
						forwardAck(&requestPriorityTask, &ackStateTask, pakiet.data, pakiet.data2, REJECTED);

					}else{
						addTask(&taskQueue, pakiet.data, pakiet.data2);
					}
					
				}
				break;
			
			case FIN:
				debugHunter("Dostałem FIN: {taskId:%d, giverId:%d} od (tid:%d)", pakiet.data, pakiet.data2, pakiet.src);
				deleteAckState(&ackStateTask, pakiet.data, pakiet.data2);
				deleteRequestPriority(&requestPriorityTask, pakiet.data, pakiet.data2);
				break;
			case SHOP_ACK:
				// Ignorujemy wiadomosc
				break;
			case SHOP_REQ:
				sendPacket2(&pakiet, pakiet.src, SHOP_ACK);
				break;
			}
		}





	}
}
