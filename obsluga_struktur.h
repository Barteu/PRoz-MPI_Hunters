#ifndef OBSLUGA_STRUKTUR_H
#define OBSLUGA_STRUKTUR_H


void addRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId);
//struct RequestPriorityNode getRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId);
int getRequestPriorityByHunter(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId, int hunterId);
void setRequestPriorityByHunter(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId, int hunterId, int priority);

int deleteRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId);
void addAckState(struct AckStateTask *ackStateTask, int taskId, int giverId);

//struct AckStateNode* getAckState(struct AckStateTask *ackStateTask, int taskId, int giverId);
taskStateNames getAckStateByHunter(struct AckStateTask *ackStateTask, int taskId, int giverId, int hunterId);
int getAckStateTask(struct AckStateTask *ackStateTask, int taskId, int giverId);
void setAckStateByHunter(struct AckStateTask *ackStateTask, int taskId, int giverId, int hunterId, taskStateNames state);
int deleteAckState(struct AckStateTask *ackStateTask, int taskId, int giverId);

void addTask(struct TaskQueue *taskQueue, int taskId, int giverId);
int isTaskInQueue(struct TaskQueue *taskQueue, int taskId, int giverId);
void getTask(struct TaskQueue *taskQueue, int* ids);
int isTaskRequested(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId);



void forwardAllAck(struct RequestPriorityTask* requestPriorityTask, struct AckStateTask *ackStateTask, int taskId, int giverId);
void forwardAck(struct RequestPriorityTask* requestPriorityTask, struct AckStateTask *ackStateTask, int taskId, int giverId, taskStateNames ackState);

int isAnyTaskInQueue(struct TaskQueue *taskQueue);

void sendOldRequests(struct RequestPriorityTask *requestPriorityTask, struct AckStateTask *ackStateTask);
#endif