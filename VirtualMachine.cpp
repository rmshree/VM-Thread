#include "VirtualMachine.h"
#include "Machine.h"
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <queue>
#include <iostream>

extern "C"{

using namespace std;

int volatile counter;
int ticks;

typedef void (*TVMThreadEntry)(void *);

class TCB{
	public:

	TCB(){};
	TCB(TVMThreadPriority prior, size_t memsize, TVMThreadEntry entry1, void *param1); 
	TVMThreadID ID;
	TVMStatus state;	
	TVMThreadPriority priority;
	size_t st_size;
	unsigned int num_ticks;	
	uint8_t *StackBase;
	TVMThreadEntry entry;
	void *param;
	SMachineContext MachineContext; 	
	int filereturn;
	int acquired;
};

class MB{
	public:

	MB(){};
	vector <TCB *> acquire;
	vector <TCB *> acquire_l;
	vector <TCB *> acquire_m;
	vector <TCB *> acquire_h;
	TVMMutexID mutexID;
	TVMThreadID owner;
	int lock;
};

void idle(void *param){
//	MachineEnableSignals();
	while(1){
	}
}

TCB *running; //= new TCB((TVMThreadPriority)NULL, 0, (TVMThreadEntry)NULL, NULL);
//TCB *idle1 = new TCB((TVMThreadPriority)NULL, 0, idle, NULL);

vector <TCB *> v_tcb;

vector <TCB *> high;
vector <TCB *> medium;
vector <TCB *> low;

vector <TCB *> waiting;
vector <TCB *> sleeping;

vector <MB> v_mutex;

void Scheduler();

TCB:: TCB(TVMThreadPriority prior, size_t memsize, TVMThreadEntry entry1, void *param1){
	priority = prior;
	state = VM_THREAD_STATE_DEAD;
	//int st_size = memsize;
	st_size = memsize;
	StackBase = new uint8_t[memsize];
	entry = entry1;
	param = param1;
	num_ticks = ticks;
}

void Skeleton(void *param1){
    //get the entry function and param that you need to call 
	MachineEnableSignals();
	((TCB *)param1)->entry(((TCB *)param1)->param);
	VMThreadTerminate(((TCB *)param1)->ID);// This will allow you to gain control back if the ActualThreadEntry returns
}//SkeletonEntry


TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid){
	TMachineSignalState OldState;

	//MachineEnableSignals();
	if(entry == NULL || tid == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	else{
		MachineSuspendSignals(&OldState);
		TCB *block = new TCB(prio, memsize, entry, param);
		*tid = v_tcb.size();
		block->ID = v_tcb.size();
		//tid_tcb.push_back(block.ID);
		v_tcb.push_back(block); 	
		MachineResumeSignals(&OldState);
	}

	return VM_STATUS_SUCCESS;
}


TVMMainEntry VMLoadModule(const char *module);


void AlarmCB(void *param){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	for(unsigned int i = 0; i < sleeping.size(); i++){
		if(sleeping[i]->num_ticks == 0){
			sleeping[i]->state = VM_THREAD_STATE_READY;
			if(sleeping[i]->priority == VM_THREAD_PRIORITY_LOW)
				low.push_back(sleeping[i]);
			else if(sleeping[i]->priority == VM_THREAD_PRIORITY_NORMAL)
				medium.push_back(sleeping[i]);
			else // high priority
				high.push_back(sleeping[i]);
			for(unsigned int j = 0; j < waiting.size(); j++)
				if(waiting[j]->ID == sleeping[i]->ID){
					waiting.erase(waiting.begin() + j);
					break;
				}
			if(running == v_tcb[0]){
	                        sleeping.erase(sleeping.begin() + i);
				Scheduler();
			}
			else if(sleeping[i]->priority == VM_THREAD_PRIORITY_HIGH && running->priority != VM_THREAD_PRIORITY_HIGH){
			      	sleeping.erase(sleeping.begin() + i);
                                Scheduler();
			}
			else if(sleeping[i]->priority == VM_THREAD_PRIORITY_NORMAL && running->priority == VM_THREAD_PRIORITY_LOW){
                                sleeping.erase(sleeping.begin() + i);
                                Scheduler();
                        }
			else{
				sleeping.erase(sleeping.begin() + i);
			}
	 	//	MachineEnableSignals();
		}
		sleeping[i]->num_ticks--;
		//if(i == 0)
	}
	//call Scheduler (?)
	
	TCB *newowner;
	for(unsigned int i=0; i < v_mutex.size(); i++){
		for(unsigned int j=0; j < v_mutex[i].acquire.size(); j++){
			if(v_mutex[i].acquire[j]->num_ticks == 0){
				newowner = v_mutex[i].acquire[j];
				newowner->acquired = 0;
				v_mutex[i].acquire.erase(v_mutex[i].acquire.begin() + j);
			
				if(newowner->priority == VM_THREAD_PRIORITY_HIGH){
		               	        unsigned int k;
               		        	for(k=0; k < v_mutex[i].acquire_h.size(); k++)
                               			if(v_mutex[i].acquire_h[k]->ID == newowner->ID)
                                       			break;
			        	v_mutex[i].acquire_h.erase(v_mutex[i].acquire_h.begin() + k);
					high.push_back(newowner);
				}
				else if(newowner->priority == VM_THREAD_PRIORITY_NORMAL){
                                       	unsigned int k;
                                       	for(k=0; k < v_mutex[i].acquire_m.size(); k++)
                                       	        if(v_mutex[i].acquire_m[k]->ID == newowner->ID)
                                               	        break;
                                     	v_mutex[i].acquire_m.erase(v_mutex[i].acquire_m.begin() + k);
				
					medium.push_back(newowner);
				}
				else{
                                       	unsigned int k;
                                       	for(k=0; k < v_mutex[i].acquire_l.size(); k++)
                                               	if(v_mutex[i].acquire_l[k]->ID == newowner->ID)
                                                       	break;
                                    	v_mutex[i].acquire_l.erase(v_mutex[i].acquire_l.begin() + k);
	
					low.push_back(newowner);								
				}
                       		if(running == v_tcb[0])
        	                        Scheduler();
                        	else if(newowner->priority == VM_THREAD_PRIORITY_HIGH && running->priority != VM_THREAD_PRIORITY_HIGH)
                                		Scheduler();
                        	else if(newowner->priority == VM_THREAD_PRIORITY_NORMAL && running->priority == VM_THREAD_PRIORITY_LOW)
                                		Scheduler();
				else
					v_mutex[i].acquire[j]->num_ticks--;
			}
		}	
	}
			
	MachineResumeSignals(&OldState);
	Scheduler();	
	//counter--;   //need count down timers for each sleeping thread (?)      
}//AlarmCB


/*void MachineCB(void *param){
	if(param == NULL)	
		
		
}*/

TVMStatus VMThreadSleep(TVMTick tick){
        TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);	

	running->num_ticks = tick;

	if(tick == VM_TIMEOUT_INFINITE)	
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	else{
		//MachineSuspendSignals(&OldState);	
		running->state = VM_THREAD_STATE_WAITING;
		sleeping.push_back(running);
		waiting.push_back(running);
		//int i = sleeping.size() -1;
		//while(sleeping[i].num_ticks != 0);
		//sleeping.erase(sleeping.begin() + i);
		//MachineEnableSignals();
	}
	Scheduler();
	MachineResumeSignals(&OldState);
	

	return VM_STATUS_SUCCESS;
}//ThreadSleep


TVMStatus VMThreadDelete(TVMThreadID thread){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	unsigned int i;
	for(i = 0; i < v_tcb.size(); i++)
		if(v_tcb[i]->ID == thread)
			break;
	
	if(i == v_tcb.size())
		return VM_STATUS_ERROR_INVALID_ID;	

	if(v_tcb[i]->state != VM_THREAD_STATE_DEAD)
		return VM_STATUS_ERROR_INVALID_STATE;
	else{
		delete v_tcb[i];
		v_tcb.erase(v_tcb.begin() + i);	
	}

	MachineResumeSignals(&OldState);	
	return VM_STATUS_SUCCESS;
}//ThreadDelete


//SMachineContext global_new_context;

TVMStatus VMThreadActivate(TVMThreadID thread){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	//SMachineContext curr_context;
		
	unsigned int i;

	for(i = 0; i < v_tcb.size(); i++)
                if(v_tcb[i]->ID == thread)
                        break;

	if(i == v_tcb.size())
                return VM_STATUS_ERROR_INVALID_ID;

	if(v_tcb[i]->state != VM_THREAD_STATE_DEAD)
                return VM_STATUS_ERROR_INVALID_STATE;
	
	v_tcb[i]->state = VM_THREAD_STATE_READY;

	if(v_tcb[i]->priority == VM_THREAD_PRIORITY_LOW)
		low.push_back(v_tcb[i]);
	else if(v_tcb[i]->priority == VM_THREAD_PRIORITY_NORMAL){
                medium.push_back(v_tcb[i]);
	}
	else
                high.push_back(v_tcb[i]);

	//create context
	MachineContextCreate(&v_tcb[i]->MachineContext, Skeleton, v_tcb[i], v_tcb[i]->StackBase, v_tcb[i]->st_size);
	//MachineEnableSignals();
	//handle scheduling
	//if(v_tcb[i].state == VM_THREAD_STATE_WAITING)
	if(running == v_tcb[0]){
       		//sleeping.erase(sleeping.begin() + i);
           	Scheduler();
     	}
   	else if(v_tcb[i]->priority == VM_THREAD_PRIORITY_HIGH && running->priority != VM_THREAD_PRIORITY_HIGH){
            	//sleeping.erase(sleeping.begin() + i);
               	Scheduler();
   	}
       	else if(v_tcb[i]->priority == VM_THREAD_PRIORITY_NORMAL && running->priority == VM_THREAD_PRIORITY_LOW){
     		//sleeping.erase(sleeping.begin() + i);
              	Scheduler();
      	}
       	//else
       		//sleeping.erase(sleeping.begin() + i);
	
	//Scheduler();
	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//ThreadActivate

void Scheduler(){
	TVMThreadID temp;
	//TMachineSignalState OldState;
		if(!(high.empty())){
			temp = high[0]->ID;
			high.erase(high.begin());		
		}
		else if(!(medium.empty())){
			temp = medium[0]->ID;
                        medium.erase(medium.begin());	
		}
		else if (!(low.empty())){
			temp = low[0]->ID;
                        low.erase(low.begin());
		}
		else{
			//MachineSuspendSignals(&OldState);
			if(running->state != VM_THREAD_STATE_RUNNING){ //run idle thread now
				v_tcb[0]->state = VM_THREAD_STATE_RUNNING;
				TCB *oldstate = running;
				running = v_tcb[0];
				//running->ID = v_tcb[0].ID;
				MachineContextSwitch(&oldstate->MachineContext, &running->MachineContext);
				//MachineEnableSignals();
				return;
			}	
			else
				return; 
		}	 

	//MachineSuspendSignals(&OldState);
	unsigned int i;
	for(i=0; i<v_tcb.size();i++)
		if(v_tcb[i]->ID == temp)
			break;

	v_tcb[i]->state =  VM_THREAD_STATE_RUNNING;
	
	if(running != v_tcb[0] && running->state == VM_THREAD_STATE_RUNNING){
		if(running->priority == VM_THREAD_PRIORITY_HIGH)
			high.push_back(running);
		else if(running->priority == VM_THREAD_PRIORITY_NORMAL)
			medium.push_back(running);
		else if(running->priority == VM_THREAD_PRIORITY_LOW)
			low.push_back(running);
	}
//	else if(running->state == VM_THREAD_STATE_WAITING)
//		waiting.push_back(*running);

	
	TCB *temp1 = running;
        running = v_tcb[i];
	//MachineEnableSignals();
	MachineContextSwitch(&temp1->MachineContext, &running->MachineContext);
	//if(v_tcb[temp].state == VM_THREAD_STATE_WAITING)
	//	waiting.push(v_tcb[temp]);		
}//Scheduler

void pop_queue(TVMThreadID thread, int i){
	unsigned int h, m, l;
	
	if(v_tcb[i]->ID == 0)
		return;
	else if(running->ID == thread)
		return;
	else if(v_tcb[i]->priority == VM_THREAD_PRIORITY_LOW){
		for(h = 0; h < low.size(); h++){	
			if(low[h]->ID == thread)
				break;
		}
		low.erase(low.begin() + h);
	}
	else if(v_tcb[i]->priority == VM_THREAD_PRIORITY_NORMAL){
		for(m = 0; m < medium.size(); m++){
                        if(medium[m]->ID == thread)
                                break;
                }
                medium.erase(medium.begin() + m);
	}
        else{ //if(v_tcb[i].priority == VM_THREAD_PRIORITY_HIGH){
		for(l = 0; l < high.size(); l++){
                        if(high[l]->ID == thread)
                                break;
                }
                high.erase(high.begin() + l);
        }
}//popqueue

TVMStatus VMThreadTerminate(TVMThreadID thread){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	unsigned int i;
        for(i = 0; i < v_tcb.size(); i++)
                if(v_tcb[i]->ID == thread)
                        break;
        if(i == v_tcb.size())
                return VM_STATUS_ERROR_INVALID_ID;
        if(v_tcb[i]->state == VM_THREAD_STATE_DEAD)
                return VM_STATUS_ERROR_INVALID_STATE;
	pop_queue(thread, i);
	v_tcb[i]->state = VM_THREAD_STATE_DEAD;

	Scheduler();
	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//ThreadTerminate


TVMStatus VMThreadID(TVMThreadIDRef threadref){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	if(threadref == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	//*threadref = running.ID;
	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//ThreadID


TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef state){
	
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	unsigned int i;
	if(state == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	for(i = 0; i < v_tcb.size(); i++)
                if(v_tcb[i]->ID == thread)
                        break;
	
        if(i == v_tcb.size())
                return VM_STATUS_ERROR_INVALID_ID;

	*state = v_tcb[i]->state;
	
	MachineResumeSignals(&OldState);

	return VM_STATUS_SUCCESS;
}//ThreadState


TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]){
	const char *arg = argv[0];
	//TMachineSignalState OldState;
	TVMThreadID idleID;

	//MachineSuspendSignals(&OldState);
	VMThreadCreate(idle, NULL, 0x100000, (TVMThreadPriority)NULL, &idleID);
	//idle1 = &v_tcb[0];
	
	TVMMainEntry main = VMLoadModule(arg);		
	ticks = tickms;
	
	MachineInitialize(tickms);
	MachineRequestAlarm(tickms*1000, AlarmCB, NULL);       //FYI could pass in a DataStructure instead of NULL

	//Create a special TCB for the main thread -> assign it to be the running(global) -> pass the running into MachineSwitchContext as 1st param
	TCB *mainThread = new TCB(VM_THREAD_PRIORITY_NORMAL, 0, NULL, NULL);
	v_tcb.push_back(mainThread);
	v_tcb[1]->ID = 1;
	running = v_tcb[1];	

	//create context for idle	
	MachineContextCreate(&v_tcb[0]->MachineContext, Skeleton, v_tcb[0], v_tcb[0]->StackBase, v_tcb[0]->st_size);


	MachineEnableSignals();

	if(main == NULL)
		return(VM_STATUS_FAILURE);
	else{
		main(argc, argv);
		return(VM_STATUS_SUCCESS);
	}
}//VMStart

//VMAllThreads
//VMSchedule

void OpenCB(void* calldata, int result){
	((TCB*)calldata)->filereturn = result;
	((TCB*)calldata)->state = VM_THREAD_STATE_READY;

	
	unsigned int i;
	for(i=0; i < waiting.size(); i++)
		if(waiting[i]->ID == ((TCB*)calldata)->ID)
	   		break;
       
	if(waiting[i]->priority == VM_THREAD_PRIORITY_LOW)
                low.push_back(waiting[i]);
	else if(waiting[i]->priority == VM_THREAD_PRIORITY_NORMAL){
               	medium.push_back(waiting[i]);
	}
     	else // high priority
      		high.push_back(waiting[i]);
	
	//Scheduler if higher priority has woken
      	if(running == v_tcb[0]){
               	waiting.erase(waiting.begin() + i);
              	Scheduler();
     	}
      	else if(waiting[i]->priority == VM_THREAD_PRIORITY_HIGH && running->priority != VM_THREAD_PRIORITY_HIGH){
            	waiting.erase(waiting.begin() + i);
              	Scheduler();
      	}
      	else if(waiting[i]->priority == VM_THREAD_PRIORITY_NORMAL && running->priority == VM_THREAD_PRIORITY_LOW){
              	waiting.erase(waiting.begin() + i);
            	Scheduler();
      	}
   	else{
           	waiting.erase(sleeping.begin() + i);
    	}

}

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor){
	if(filename == NULL || filedescriptor == NULL)
                return VM_STATUS_ERROR_INVALID_PARAMETER;

	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	running->state = VM_THREAD_STATE_WAITING;
       	waiting.push_back(running);
	void *calldata = (void*)running;

	MachineFileOpen(filename, flags, mode, OpenCB, calldata);
	Scheduler();

	if(running->filereturn < 0)
		return VM_STATUS_FAILURE; 	
	else
		*filedescriptor = (running->filereturn);

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//FileOpen

TVMStatus VMFileClose(int filedescriptor){
	TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        running->state = VM_THREAD_STATE_WAITING;
	waiting.push_back(running);
        void *calldata = (void*)running;

        MachineFileClose(filedescriptor, OpenCB, calldata);
        Scheduler();

        if(running->filereturn < 0)
                return VM_STATUS_FAILURE;

        MachineResumeSignals(&OldState);
        return VM_STATUS_SUCCESS;

}//FileCLose


TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){	//needs to work with machine something?
	if(data == NULL || length == NULL)
		return(VM_STATUS_ERROR_INVALID_PARAMETER);
	else{
		TMachineSignalState OldState;
		MachineSuspendSignals(&OldState);
		
		running->state = VM_THREAD_STATE_WAITING;
		waiting.push_back(running);
		void *calldata = (void*)running;

		MachineFileWrite(filedescriptor, data, *length, OpenCB, calldata);
		Scheduler();		

		if(running->filereturn  < 0)
			return(VM_STATUS_FAILURE);
		else
			*length = running->filereturn;
				
		MachineResumeSignals(&OldState);
		return(VM_STATUS_SUCCESS);
	}

}//FileWrite

TVMStatus VMFileRead(int filedescriptor, void *data, int *length){

	if(data == NULL || length == NULL)
                return(VM_STATUS_ERROR_INVALID_PARAMETER);
        
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

	running->state = VM_THREAD_STATE_WAITING;
	waiting.push_back(running);
	void *calldata = (void*)running;

	MachineFileRead(filedescriptor, data, *length, OpenCB, calldata);
	Scheduler();

	if(running->filereturn < 0)
		return(VM_STATUS_FAILURE);
	else
		*length = running->filereturn;

	MachineResumeSignals(&OldState);
	return(VM_STATUS_SUCCESS);
}//FileRead

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset){
        TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

        running->state = VM_THREAD_STATE_WAITING;
        waiting.push_back(running);
        void *calldata = (void*)running;

        MachineFileSeek(filedescriptor, offset, whence, OpenCB, calldata);
	Scheduler();

        if(running->filereturn < 0)
                return(VM_STATUS_FAILURE);
        else
                *newoffset = running->filereturn;

        MachineResumeSignals(&OldState);
        return(VM_STATUS_SUCCESS);

}//FileSeek

TVMStatus VMMutexCreate(TVMMutexIDRef mutexref){
	if(mutexref == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	MB mblock;
	*mutexref = v_mutex.size();
	mblock.mutexID = v_mutex.size();
	mblock.lock = 0;
	v_mutex.push_back(mblock);	
	
	MachineResumeSignals(&OldState);

	return VM_STATUS_SUCCESS;
}//MutexCreate

TVMStatus VMMutexDelete(TVMMutexID mutex){
	TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

	unsigned int i;
	for(i = 0; i < v_mutex.size(); i++)
		if(v_mutex[i].mutexID == mutex)
			break;

	if(i == v_mutex.size())
		return VM_STATUS_ERROR_INVALID_ID;
	else if(v_mutex[i].lock == 1)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	else
		v_mutex.erase(v_mutex.begin() + i);

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//MutexDelete

TVMStatus VMMutexQuery(TVMMutexID mutex, TVMThreadIDRef ownerref){
	if(ownerref == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

	unsigned int i;
        for(i = 0; i < v_mutex.size(); i++)
                if(v_mutex[i].mutexID == mutex)
                        break;

	if(i == v_mutex.size())
		return VM_STATUS_ERROR_INVALID_ID;
	else if(v_mutex[i].lock == 1)
		*ownerref = v_mutex[i].owner;
	else
		return VM_THREAD_ID_INVALID;
	
	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//MutexQuery

TVMStatus VMMutexAcquire(TVMMutexID mutex, TVMTick timeout){

	TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

	unsigned int i;
        for(i = 0; i < v_mutex.size(); i++)
                if(v_mutex[i].mutexID == mutex)
                        break;

        if(i == v_mutex.size())
                return VM_STATUS_ERROR_INVALID_ID;

	if(timeout == VM_TIMEOUT_IMMEDIATE)
		if(v_mutex[i].lock == 0){
			v_mutex[i].owner = running->ID;
			return VM_STATUS_SUCCESS;
		}
		else
			return VM_STATUS_FAILURE;
	else{
		if(timeout == VM_TIMEOUT_INFINITE)
                	running->num_ticks = -1;
		else
			running->num_ticks = timeout;

		running->state = VM_THREAD_STATE_WAITING;

		if(v_mutex[i].lock == 0){
                        v_mutex[i].owner = running->ID;
                        return VM_STATUS_SUCCESS;
                }
		else{
     			v_mutex[i].acquire.push_back(running);		

			//push into right priority queue in mutex
			if(running->priority == VM_THREAD_PRIORITY_HIGH){
				v_mutex[i].acquire_h.push_back(running);
			}
			else if(running->priority == VM_THREAD_PRIORITY_NORMAL){
				v_mutex[i].acquire_m.push_back(running);
			}
			else{ // low priority for mutex lock
				v_mutex[i].acquire_l.push_back(running);
			}
			Scheduler();
		}
	}
	MachineResumeSignals(&OldState);
	if(running->acquired == 0)
		return VM_STATUS_FAILURE;
	else 
		return VM_STATUS_SUCCESS;
}//MutexAcquire

TVMStatus VMMutexRelease(TVMMutexID mutex){
	TMachineSignalState OldState;
        MachineSuspendSignals(&OldState);

	unsigned int i;
	for(i = 0; i < v_mutex.size(); i++)
                if(v_mutex[i].mutexID == mutex)
                        break;

	if(v_mutex.size() == i)
		return VM_STATUS_ERROR_INVALID_ID;
	else if(v_mutex[i].owner != running->ID)
		return VM_STATUS_ERROR_INVALID_STATE;
	else{
		running->acquired = 0;
		v_mutex[i].lock = 0;
	}
	TCB *newowner;
                if(!v_mutex[i].acquire.empty()){
                        if(!v_mutex[i].acquire_h.empty()){
                                v_mutex[i].lock = 1;
                                newowner = v_mutex[i].acquire_h[0];
                                newowner->acquired = 1;
                                newowner->state = VM_THREAD_STATE_READY;
                                high.push_back(newowner);
                                v_mutex[i].acquire_h.erase(v_mutex[i].acquire_h.begin());
                                v_mutex[i].owner = newowner->ID;
                        }
                        else if(!v_mutex[i].acquire_m.empty()){
                                v_mutex[i].lock = 1;
                                newowner = v_mutex[i].acquire_m[0];
                                newowner->acquired = 1;
                                newowner->state = VM_THREAD_STATE_READY;
                                medium.push_back(newowner);
                                v_mutex[i].acquire_m.erase(v_mutex[i].acquire_m.begin());
                                v_mutex[i].owner = newowner->ID;
                        }
                        else{ //if(!acquire_l.empty()){
                                v_mutex[i].lock = 1;
                                newowner = v_mutex[i].acquire_l[0];
                                newowner->acquired = 1;
                                newowner->state = VM_THREAD_STATE_READY;
                                low.push_back(newowner);
                                v_mutex[i].acquire_l.erase(v_mutex[i].acquire_l.begin());
                                v_mutex[i].owner = newowner->ID;
                        }
                        unsigned int j;
                        for(j=0; j < v_mutex[i].acquire.size(); j++)
                                if(v_mutex[i].acquire[j]->ID == newowner->ID)
                                        break;
                        v_mutex[i].acquire.erase(v_mutex[i].acquire.begin() + j);


                        if(running == v_tcb[0]){
                                Scheduler();
                        }
                        else if(newowner->priority == VM_THREAD_PRIORITY_HIGH && running->priority != VM_THREAD_PRIORITY_HIGH)
                                Scheduler();
                        else if(newowner->priority == VM_THREAD_PRIORITY_NORMAL && running->priority == VM_THREAD_PRIORITY_LOW)
        			Scheduler();
		}		               

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//MutexRelease


}//end of extern
