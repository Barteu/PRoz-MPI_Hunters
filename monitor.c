#include "main.h"
#include "monitor.h"


/* monitor to osobny wątek istniejący tylko u ROOTa, komunikujący się z użytkownikiem */
/* Ponieważ wprowadza on dodatkowe komplikacje w debugowaniu, można go wyciąć przy implementacji
   tematów projektów */
void *startMonitor(void *ptr)
{
    /* Obrazuje pętlę odbierającą komendy od użytkownika */
    char *instring, *token, *saveptr;
    instring=malloc(100);
    int newline;
    char *res;
    while ( stan!=InFinish ) {
	debug("sobie monitoruję");
        res= fgets( instring, 99, stdin);
        if (res == 0) continue; 
        newline = strcspn(instring, "\n");
        if (newline<2) continue;
        instring[newline]=0; 
	debug("string %s\n",instring);
        token = strtok_r(instring, " ", &saveptr);
        if ((strcmp(token,"exit")==0) ||
	    (strcmp(token,"quit")==0)) {
            int i;
            for (i=0;i<size;i++)
		sendPacket(0,i,FINISH);
        }
        else if ((strcmp(token,"stop")==0) ||
	    (strcmp(token,"wait")==0)) {
            int i;
            for (i=0;i<size;i++)
		sendPacket(0,i,INMONITOR);
        } else if ((strcmp(token,"resume")==0) ||
	    (strcmp(token,"run")==0)) {
            int i;
            for (i=0;i<size;i++)
		sendPacket(0,i,INRUN);
        }  
 //       free(instring);
    }
    free(instring);
}
