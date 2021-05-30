#include "main.h"
#include "obsluga_struktur.h"



void addTask(struct TaskQueue *taskQueue, int taskId, int giverId){
    pthread_mutex_lock(&taskQueueMut);
    struct TaskNode* newTask = (struct TaskNode*)malloc(sizeof(struct TaskNode));
    newTask->taskId = taskId;
    newTask->giverId = giverId;
    newTask->next = NULL;
    if(taskQueue->head == NULL){
        taskQueue->head = newTask;
        taskQueue->tail = newTask;
    }
    else{
        taskQueue->tail->next = newTask;
        taskQueue->tail = newTask;
    }
    pthread_mutex_unlock(&taskQueueMut);
}

void getTask(struct TaskQueue *taskQueue, int *ids){
    pthread_mutex_lock(&taskQueueMut);
    ids[0] = -1;
    ids[1] = -1;
 
    if(taskQueue->head!= NULL){
        struct TaskNode *tmp = taskQueue->head->next;
       
        ids[0] = taskQueue->head->taskId;
        ids[1] = taskQueue->head->giverId;

        free(taskQueue->head);
        taskQueue->head=tmp;
        if(tmp==NULL){
            taskQueue->tail = NULL;
        }
    }
    pthread_mutex_unlock(&taskQueueMut);
}

int isAnyTaskInQueue(struct TaskQueue *taskQueue){
    pthread_mutex_lock(&taskQueueMut);
    if(taskQueue->head != NULL){       
        pthread_mutex_unlock(&taskQueueMut);
        return 1;
    }
    pthread_mutex_unlock(&taskQueueMut);
    return 0;
}

int isTaskInQueue(struct TaskQueue *taskQueue, int taskId, int giverId){
    pthread_mutex_lock(&taskQueueMut);
    struct TaskNode *tmp = taskQueue->head;
    while(tmp!=NULL){
        if(tmp->taskId == taskId && tmp->giverId == giverId){
        pthread_mutex_unlock(&taskQueueMut);
        return 1;
        }
        tmp = tmp->next;
    }
    pthread_mutex_unlock(&taskQueueMut);
    return 0;
}


void addAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* newAckState = (struct AckStateNode*)malloc(sizeof(struct AckStateNode));
    newAckState->taskId = taskId;
    newAckState->giverId = giverId;
    newAckState->next = NULL;
    newAckState->prev = NULL;
    newAckState->states = (taskStateNames*)malloc(sizeof(taskStateNames) * hunterTeamsNum);
    for(int i = 0 ; i<hunterTeamsNum;i++){
        newAckState->states[i] = REQUEST_NOT_SEND;
    }
    if(ackStateTask->head == NULL){
        ackStateTask->head = newAckState;
        ackStateTask->tail = newAckState;
    }
    else{
     
        struct AckStateNode* lastNode = ackStateTask->tail;
        ackStateTask->tail->next = newAckState;
        ackStateTask->tail = newAckState;
        ackStateTask->tail->prev = lastNode;
      
    }
    pthread_mutex_unlock(&ackStateTaskMut);
}

void forwardAllAck(struct RequestPriorityTask* requestPriorityTask, struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);   
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *requestNode = requestPriorityTask->head;
    struct AckStateNode *ackNode;
    packet_t pakiet;

    while(requestNode != NULL){
        if(requestNode->taskId != taskId || requestNode->giverId != giverId){
            ackNode = ackStateTask->head;
            for(int i = 0; i < hunterTeamsNum; i++){
                if(requestNode->priorities[i] != -1 && i != rank){
                    
                    pakiet.data = requestNode->taskId;
                    pakiet.data2 = requestNode->giverId;
                    sendPacket(&pakiet, i, TASK_ACK);
                    requestNode->priorities[i] = -1;
                }
            }
            while(ackNode != NULL){
                if(ackNode->taskId == requestNode->taskId  && ackNode->giverId == requestNode->giverId){
                    for(int i = 0; i < hunterTeamsNum; i++){
                        ackNode->states[i] = REQUEST_NOT_SEND;
                    }
                }
                ackNode = ackNode->next;
            }
            
        }
        requestNode = requestNode->next;
    }
    
    pthread_mutex_unlock(&requestPriorityTaskMut);
    pthread_mutex_unlock(&ackStateTaskMut);     

}

void forwardAck(struct RequestPriorityTask* requestPriorityTask, struct AckStateTask *ackStateTask, int taskId, int giverId, taskStateNames ackState){
    pthread_mutex_lock(&ackStateTaskMut);   
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *requestNode = requestPriorityTask->head;
    struct AckStateNode *ackNode;

    while(requestNode != NULL){
        if(requestNode->taskId == taskId && requestNode->giverId == giverId){
            ackNode = ackStateTask->head;
            for(int i = 0; i < hunterTeamsNum; i++){
                if(requestNode->priorities[i] != -1){

                    if(ackState == REJECTED && i!=rank){
                        packet_t pakiet;
                        pakiet.data = taskId;
                        pakiet.data2 = giverId;
                        sendPacket(&pakiet ,i,TASK_ACK);
                    }
                    requestNode->priorities[i] = -1;
                }
            }
            while(ackNode != NULL){
                if(ackNode->taskId == requestNode->taskId  && ackNode->giverId == requestNode->giverId){
                    for(int i = 0; i < hunterTeamsNum; i++){
                        ackNode->states[i] = ackState;
                    }
                    break;
                }
                ackNode = ackNode->next;
            }
            break;
        }
        requestNode = requestNode->next;
    }
    
    pthread_mutex_unlock(&requestPriorityTaskMut);
    pthread_mutex_unlock(&ackStateTaskMut);     

}


taskStateNames getAckStateByHunter(struct AckStateTask *ackStateTask, int taskId, int giverId, int hunterId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){  
            taskStateNames tmpState = node->states[hunterId];
            pthread_mutex_unlock(&ackStateTaskMut);
            return tmpState;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&ackStateTaskMut);
    return -1;
}

int getAckStateTask(struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    int counter = 0;
    while(node != NULL){
        if(node->taskId == taskId && node->giverId == giverId){  
            for(int i = 0; i < hunterTeamsNum; i++){
                if(node->states[i] == ACK_RECEIVED){
                    counter++;
                }
            }
           
        }
        node = node->next;
    }  
    pthread_mutex_unlock(&ackStateTaskMut);
    if(counter==(hunterTeamsNum-1)){
        return 1;
    }
    else{
        return 0;
    }
    
}

void setAckStateByHunter(struct AckStateTask *ackStateTask, int taskId, int giverId, int hunterId, taskStateNames state){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){  
            node->states[hunterId] = state;
            pthread_mutex_unlock(&ackStateTaskMut);
            return;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&ackStateTaskMut);
}



// 1 - udalo sie usunac, 0 - nie udalo sie odnalezc wskazanego wezla
int deleteAckState(struct AckStateTask *ackStateTask, int taskId, int giverId){
    pthread_mutex_lock(&ackStateTaskMut);
    struct AckStateNode* node;
    node = ackStateTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            struct AckStateNode* prevNode = node->prev;
            struct AckStateNode* nextNode = node->next;

            free(node);
            if(prevNode != NULL)
                prevNode->next = nextNode;
            else{
                ackStateTask->head = nextNode;
            }
            if(nextNode != NULL)
                nextNode->prev = prevNode;
            else{
                // IDK
                ackStateTask->tail = prevNode;
            }
            pthread_mutex_unlock(&ackStateTaskMut);
            return 1;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&ackStateTaskMut);
    return 0;
}

void addRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId){
     pthread_mutex_lock(&lampMut);
     pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode* newRequestPriority = (struct RequestPriorityNode*)malloc(sizeof(struct RequestPriorityNode));
    newRequestPriority->taskId = taskId;
    newRequestPriority->giverId = giverId;
    newRequestPriority->next = NULL;
    newRequestPriority->prev = NULL;
    newRequestPriority->priorities = (int*)malloc(sizeof(int) * hunterTeamsNum);
    for(int i = 0 ; i<hunterTeamsNum;i++){
         newRequestPriority->priorities[i] = -1;
    }
    newRequestPriority->priorities[rank] = zegar;
    if(requestPriorityTask->head == NULL){
        requestPriorityTask->head = newRequestPriority;
        requestPriorityTask->tail = newRequestPriority;
    }
    else{
        struct RequestPriorityNode* lastNode = requestPriorityTask->tail;
        requestPriorityTask->tail->next =  newRequestPriority;
        requestPriorityTask->tail =  newRequestPriority;
        requestPriorityTask->tail->prev = lastNode;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    pthread_mutex_unlock(&lampMut);
}


 int getRequestPriorityByHunter(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId, int hunterId){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            int tmpPriority = node->priorities[hunterId];
            pthread_mutex_unlock(&requestPriorityTaskMut);
            return tmpPriority;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    return -1;
}


 int isTaskRequested(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            for(int i = 0 ; i < hunterTeamsNum; i++){
                if(i != rank && node->priorities[i] != -1){
                    pthread_mutex_unlock(&requestPriorityTaskMut);
                    return 1;
                }
            }
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    return 0;
}


 void setRequestPriorityByHunter(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId, int hunterId, int priority){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode *node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            node->priorities[hunterId] = priority;
            break;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
}

// 1 - udalo sie usunac, 0 - nie udalo sie odnalezc wskazanego wezla
int deleteRequestPriority(struct RequestPriorityTask *requestPriorityTask, int taskId, int giverId){
    pthread_mutex_lock(&requestPriorityTaskMut);
    struct RequestPriorityNode* node;
    node = requestPriorityTask->head;
    while(node){
        if(node->taskId == taskId && node->giverId == giverId){
            struct RequestPriorityNode* prevNode = node->prev;
            struct RequestPriorityNode* nextNode = node->next;
            free(node);
            if(prevNode != NULL)
                prevNode->next = nextNode;
            else{
                requestPriorityTask->head = nextNode;
            }
            if(nextNode != NULL)
                nextNode->prev = prevNode;
            else{
                requestPriorityTask->tail = prevNode;
            }
            pthread_mutex_unlock(&requestPriorityTaskMut);
            return 1;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&requestPriorityTaskMut);
    return 0;
}

void sendOldRequests(struct RequestPriorityTask *requestPriorityTask, struct AckStateTask *ackStateTask){
    pthread_mutex_lock(&requestPriorityTaskMut);
    pthread_mutex_lock(&ackStateTaskMut);
    struct RequestPriorityNode* nodePriority;
    nodePriority = requestPriorityTask->head;
    while(nodePriority){
        struct AckStateNode* nodeState;
        nodeState = ackStateTask->head;
        while(nodeState){
        if(nodeState->taskId == nodePriority->taskId && nodeState->giverId == nodePriority->giverId){  
            for(int i = 0; i < hunterTeamsNum; i++){
                if(i!=rank){
                    if(nodeState->states[i] == REQUEST_NOT_SEND){
                        pthread_mutex_lock(&lampMut);
                        nodePriority->priorities[rank] = zegar;
                        pthread_mutex_unlock(&lampMut);
                        packet_t pakiet;
                        pakiet.data = nodeState->taskId;
                        pakiet.data2 = nodeState->giverId;
                        pakiet.priority = nodePriority->priorities[rank];
                        for(int j = 0; j < hunterTeamsNum; j++){
                            if(j!=rank){
                                sendPacket(&pakiet, j, TASK_REQ);
                                nodeState->states[j] = REQUEST_SEND;
                            }
                               
                        }
                        break;
                    }
                }
            }
            break;
        }
        nodeState = nodeState->next;
    }
        nodePriority = nodePriority->next;
    }

    pthread_mutex_unlock(&ackStateTaskMut);
    pthread_mutex_unlock(&requestPriorityTaskMut);
}
