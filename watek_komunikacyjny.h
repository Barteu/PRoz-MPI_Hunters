#ifndef WATEK_KOMUNIKACYJNY_H
#define WATEK_KOMUNIKACYJNY_H

/* wątek komunikacyjny: odbieranie wiadomości i reagowanie na nie poprzez zmiany stanu */
void *startKomWatek(void *ptr);
void *startKomWatekGiver(void *ptr);
void *startKomWatekHunter(void *ptr);
#endif
