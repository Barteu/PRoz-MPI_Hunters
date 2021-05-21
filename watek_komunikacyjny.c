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
			debugGiver("Dostałem wiadomość od %d typu FIN", pakiet.src);
			changeActiveTasks(-1);
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d typu BROADCAST", pakiet.src, pakiet.data);
				
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d, ID zleceniodawcy %d i priorytecie %d typu TASK_REQ", pakiet.src, pakiet.data, pakiet.data2, pakiet.priority);
				int myPriority = getRequestPriorityByHunter(&requestPriorityTask, pakiet.data, pakiet.data2, rank);
				if(myPriority != -1){
					if(myPriority < pakiet.priority || (myPriority==pakiet.priority && rank<pakiet.src) ){
						debugHunter("TASK_REQ staje sie ACK dla ID zlecenia %d, ID zleceniodawcy %d", pakiet.data, pakiet.data2);
						setAckStateByHunter(&ackStateTask,pakiet.data,pakiet.data2, pakiet.src, ACK_RECEIVED);
						setRequestPriorityByHunter(&requestPriorityTask, pakiet.data, pakiet.data2, pakiet.src, pakiet.priority);
						//sprawdzamy czy nie mozemy przyjac tego zlecenia
						if(getAckStateTask(&ackStateTask,pakiet.data,pakiet.data2)){
							debugHunter("Zlecenie id %d od %d przyjete", pakiet.data, pakiet.data2);
							addTask(&taskQueue, pakiet.data, pakiet.data2);
							forwardAllAck(&requestPriorityTask, &ackStateTask,pakiet.data, pakiet.data2);
							changeState(InWait);
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
					sendPacket(&pakiet, pakiet.src, TASK_ACK);
					debugHunter("Wyslalaem ACK do %d dla ID zlecenia %d , ID zleceniodawcy %d",pakiet.src,pakiet.data,pakiet.data2);
				}
				break;
			case TASK_ACK:
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d, ID zleceniodawcy %d typu TASK_ACK", pakiet.src, pakiet.data, pakiet.data2);
				setAckStateByHunter(&ackStateTask,pakiet.data,pakiet.data2, pakiet.src, ACK_RECEIVED);
				//sprawdzamy czy nie mozemy przyjac tego zlecenia
				if(getAckStateTask(&ackStateTask,pakiet.data,pakiet.data2)){
					addTask(&taskQueue, pakiet.data, pakiet.data2);
					forwardAllAck(&requestPriorityTask, &ackStateTask,pakiet.data, pakiet.data2);
					changeState(InWait);
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
				debugHunter("Dostałem wiadomość od %d o zakonczeniu zlecenia o id %d, ID zleceniodawcy %d typu FIN", pakiet.src, pakiet.data, pakiet.data2);
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d typu BROADCAST", pakiet.src, pakiet.data);
				
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d, ID zleceniodawcy %d typu TASK_ACK", pakiet.src, pakiet.data, pakiet.data2);
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
				debugHunter("Dostałem wiadomość od %d o zakonczeniu zlecenia o id %d, ID zleceniodawcy %d typu FIN", pakiet.src, pakiet.data, pakiet.data2);
				deleteAckState(&ackStateTask, pakiet.data, pakiet.data2);
				deleteRequestPriority(&requestPriorityTask, pakiet.data, pakiet.data2);
				break;
			case SHOP_ACK:
				ackNumShop += 1;
				if(ackNumShop == (hunterTeamsNum - 1) - (shopSize - 1)){
					changeState(InShop);
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d typu BROADCAST", pakiet.src, pakiet.data);
				
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d, ID zleceniodawcy %d typu TASK_ACK", pakiet.src, pakiet.data, pakiet.data2);
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
				debugHunter("Dostałem wiadomość od %d o zakonczeniu zlecenia o id %d, ID zleceniodawcy %d typu FIN", pakiet.src, pakiet.data, pakiet.data2);
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d typu BROADCAST", pakiet.src, pakiet.data);
				
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
				debugHunter("Dostałem wiadomość od %d o ID zlecenia %d, ID zleceniodawcy %d typu TASK_ACK", pakiet.src, pakiet.data, pakiet.data2);
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
				debugHunter("Dostałem wiadomość od %d o zakonczeniu zlecenia o id %d, ID zleceniodawcy %d typu FIN", pakiet.src, pakiet.data, pakiet.data2);
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



// /* wątek komunikacyjny; zajmuje się odbiorem i reakcją na komunikaty */
// void *startKomWatek(void *ptr)
// {
//     MPI_Status status;
//     int is_message = FALSE;
//     packet_t pakiet;
//     /* Obrazuje pętlę odbierającą pakiety o różnych typach */
//     while ( stan!=InFinish ) {
// 	debug("czekam na recv");
//         MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

// 		setMaxLamport(pakiet.ts);

//         switch ( status.MPI_TAG ) {
// 	    case FINISH: 
//                 changeState(InFinish);
// 	    break;
// 	    case TALLOWTRANSPORT: 
//                 changeTallow( pakiet.data);
//                 debug("Dostałem wiadomość od %d z danymi %d",pakiet.src, pakiet.data);
// 	    break;
// 		case PREPSTATE:
// 				 debug("Zmieniam stan na InTallows");
// 				 changeState( InTallows );
				 
// 		break;
// 		case TALLOWPREPSTATE:
// 				numberReceivedP++;
// 					 int i;
// 					 if (numberReceivedP > size-1) {
// 						     for (i=0;i<size;i++)
// 							sendPacket(0,i,GIVEMESTATE);
// 					 }
// 		break;
		
// 	    case GIVEMESTATE: 
//                 pakiet.data = tallow;
//                 sendPacket(&pakiet, ROOT, STATE);
//                 debug("Wysyłam mój stan do monitora: %d funtów łoju na składzie!", tallow);
// 				changeState(InRun);
// 				changePrepared(FALSE);
				
// 	    break;
//             case STATE:
//                 numberReceived++;
				
//                 globalState += pakiet.data;
//                 if (numberReceived > size-1) {
//                     debug("W magazynach mamy %d funtów łoju.", globalState);
// 					numberReceivedP=0;
//                 } 
//             break;
// 	    case INMONITOR: 
//                 changeState( InMonitor );
//                 debug("Od tej chwili czekam na polecenia od monitora");
// 	    break;
// 	    case INRUN: 
//                 changeState( InRun );
//                 debug("Od tej chwili decyzję podejmuję autonomicznie i losowo");
// 	    break;
// 	    default:
// 	    break;
//         }
//     }
// }