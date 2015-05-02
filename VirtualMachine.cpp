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
	//TMachineFileCallback callback

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
	((TCB*)calldata)->state = VM_THREAD_STATE_RUNNING;		
}

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor){
	 if(filename == NULL || filedescriptor == NULL)
                return VM_STATUS_ERROR_INVALID_PARAMETER;

	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	
	void *calldata = (void*)running;

	MachineFileOpen(filename, flags, mode, OpenCB, calldata);

	VMThreadSleep(5);

	*filedescriptor = (running->filereturn);

	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//FileOpen

void CloseCB(void* calldata, int result){

}

TVMStatus VMFileClose(int filedescriptor){
	if((close(filedescriptor)) < 0)
		return VM_STATUS_FAILURE;

	else
		return VM_STATUS_SUCCESS; 
}//FileCLose

void WriteCB(){

}

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){	//needs to work with machine something?
	if(data == NULL || length == NULL)
		return(VM_STATUS_ERROR_INVALID_PARAMETER);
	else{
		if((write(filedescriptor, data, *length)) < 0)
			return(VM_STATUS_FAILURE);
		
		else
			return(VM_STATUS_SUCCESS);
	}

}//FileWrite

TVMStatus VMFileRead(int filedescriptor, void *data, int *length){

	if(data == NULL || length == NULL)
                return(VM_STATUS_ERROR_INVALID_PARAMETER);
        
	*length = read(filedescriptor, data, *length);
	
	if(*length == 0)
		return(VM_STATUS_FAILURE);
	else
		return(VM_STATUS_SUCCESS);
}//FileRead

TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset){
	*newoffset = offset + whence;

return VM_STATUS_SUCCESS;
}//FileSeek


}//end of extern
#include "VirtualMachine.h"
