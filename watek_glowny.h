#ifndef WATEK_GLOWNY_H
#define WATEK_GLOWNY_H

/* pętla główna aplikacji: zmiany stanów itd */
int taskId;

int activeTasks;

pthread_mutex_t activeTasksMut;

void mainLoopHunter();
void mainLoopGiver();

#endif


