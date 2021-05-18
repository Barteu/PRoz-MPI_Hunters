#include "main.h"
#include "watek_komunikacyjny.h"


void* startKomWatekGiver(void* ptr){
	MPI_Status status;
    int is_message = FALSE;
    packet_t pakiet;
	/* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( stan!=InFinish ) {
		debugGiver("Czekam na recv");
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
	/* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( stan!=InFinish ) {
		debugHunter("Czekam na recv");
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		// Aktualizujemy zegar Lamporta procesu Lowcy
		setMaxLamport(pakiet.ts);
		
		switch ( status.MPI_TAG ) {
		case END:
			changeState(InFinish);
			break;
		case BROADCAST:
			debugHunter("Dostałem wiadomość od %d o ID zlecenia %d typu BROADCAST", pakiet.src, pakiet.data);
			break;
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