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
	int num_ticks;	
	uint8_t *StackBase;
	TVMThreadEntry entry;
	void *param;
	SMachineContext MachineContext; 	
};


TCB running;
//vector <TVMThreadID> tid_tcb;
vector <TCB> v_tcb;

queue <TCB> high;
queue <TCB> medium;
queue <TCB> low;

queue <TCB> waiting;
queue <TCB> sleeping;

TCB:: TCB(TVMThreadPriority prior, size_t memsize, TVMThreadEntry entry1, void *param1){
	priority = prior;
	state = VM_THREAD_STATE_DEAD;
	//int st_size = memsize;
	st_size = memsize;
	StackBase = new uint8_t[memsize];
	entry = entry1;
	//*(void *)param = *(void *)param1;
	num_ticks = ticks;
}

void Skeleton(void *param){
    //get the entry function and param that you need to call 
//	ActualThreadEntry(ActualThreadParam);
//	VMTerminate(ThisThreadID);// This will allow you to gain control back if the ActualThreadEntry returns
	cout << "In Skeleton\n";	
}//SkeletonEntry*/

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid){

	TMachineSignalState OldState;

	if(entry == NULL || tid == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	else{
		MachineSuspendSignals(&OldState);
		TCB block(prio, memsize, entry, param);
		*tid = v_tcb.size();
		block.ID = v_tcb.size();
		//tid_tcb.push_back(block.ID);
		v_tcb.push_back(block); 	
	}

	return VM_STATUS_SUCCESS;
}


TVMMainEntry VMLoadModule(const char *module);

void AlarmCB(void *param){
	counter--;   //need count down timers for each sleeping thread (?)      
}//AlarmCB


/*void MachineCB(void *param){
	if(param == NULL)	
		
		
}*/

TVMStatus VMThreadSleep(TVMTick tick){
	counter = tick;

	if(tick == VM_TIMEOUT_INFINITE)	
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	else{
//	sleep(10*tick);
//	else if(tick == VM_TIMEOUT_IMMEDIATE)
		while(counter != 0);
	}

	return VM_STATUS_SUCCESS;
}//ThreadSleep


TVMStatus VMThreadDelete(TVMThreadID thread){
	unsigned int i;
	for(i = 0; i < v_tcb.size(); i++)
		if(v_tcb[i].ID == thread)
			break;
	
	if(i == v_tcb.size())
		return VM_STATUS_ERROR_INVALID_ID;	

	if(v_tcb[i].state != VM_THREAD_STATE_DEAD)
		return VM_STATUS_ERROR_INVALID_STATE;
	else
		v_tcb.erase(v_tcb.begin() + i);	
	
	return VM_STATUS_SUCCESS;
}//ThreadDelete

void Scheduler(TVMThreadID thread);

SMachineContext global_new_context;

TVMStatus VMThreadActivate(TVMThreadID thread){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	SMachineContext curr_context;
	
	unsigned int i;

	for(i = 0; i < v_tcb.size(); i++)
                if(v_tcb[i].ID == thread)
                        break;

	if(i == v_tcb.size())
                return VM_STATUS_ERROR_INVALID_ID;

	if(v_tcb[i].state != VM_THREAD_STATE_DEAD)
                return VM_STATUS_ERROR_INVALID_STATE;
	
	v_tcb[i].state = VM_THREAD_STATE_READY;

	if(v_tcb[i].priority == VM_THREAD_PRIORITY_LOW)
		low.push(v_tcb[i]);
	if(v_tcb[i].priority == VM_THREAD_PRIORITY_NORMAL)
                medium.push(v_tcb[i]);
	else
                high.push(v_tcb[i]);

	//create context
	MachineContextCreate(&curr_context, Skeleton, v_tcb[i].param, v_tcb[i].StackBase, v_tcb[i].st_size);
	curr_context = global_new_context;	

	//handle scheduling
	if(v_tcb[i].state == VM_THREAD_STATE_WAITING)
		Scheduler(v_tcb[i].ID);
	
	return VM_STATUS_SUCCESS;
}//ThreadActivate

void Scheduler(TVMThreadID thread){
	TVMThreadID temp;

	if(thread){
		if(!(high.empty())){
			high.pop();
			temp = high.front().ID;
		}
		else if(!(medium.empty())){
			medium.pop();
			temp = medium.front().ID;
		}
		else{
			low.pop();
			temp = low.front().ID;	
		}
	}
	MachineContextSwitch(&v_tcb[temp].MachineContext, &global_new_context);
}//Scheduler

void pop_queue(TVMThreadID thread, int i){
	if(v_tcb[i].priority == VM_THREAD_PRIORITY_LOW){
		queue <TCB> temp;
		TCB temp1;
		while(low.size() > 0){
			temp1 = low.front();
			low.pop();
			if(temp1.ID == thread)
				break;
			temp.push(temp1);
		}
		while(low.size() > 0){
			temp1 = low.front();
			low.pop();
			temp.push(temp1);
		}	
		low = temp;
	}
	else if(v_tcb[i].priority == VM_THREAD_PRIORITY_NORMAL){
            	queue <TCB> temp;
                TCB temp1;
                while(medium.size() > 0){
                        temp1 = medium.front();
                        medium.pop();
                        if(temp1.ID == thread)
                                break;
                        temp.push(temp1);
                }
                while(medium.size() > 0){
                        temp1 = medium.front();
                        medium.pop();
                        temp.push(temp1);
                }
                medium = temp;
	}
        else{ //if(v_tcb[i].priority == VM_THREAD_PRIORITY_HIGH){
                queue <TCB> temp;
                TCB temp1;
                while(high.size() > 0){
                        temp1 = high.front();
                        high.pop();
                        if(temp1.ID == thread)
                                break;
                        temp.push(temp1);
                }
                while(high.size() > 0){
                        temp1 = high.front();
                        high.pop();
                        temp.push(temp1);
                }
                high = temp;
        }
}//popqueue

TVMStatus VMThreadTerminate(TVMThreadID thread){
	unsigned int i;

        for(i = 0; i < v_tcb.size(); i++)
                if(v_tcb[i].ID == thread)
                        break;

        if(i == v_tcb.size())
                return VM_STATUS_ERROR_INVALID_ID;

        if(v_tcb[i].state == VM_THREAD_STATE_DEAD)
                return VM_STATUS_ERROR_INVALID_STATE;

	pop_queue(thread, i);
	v_tcb[i].state = VM_THREAD_STATE_DEAD;

	return VM_STATUS_SUCCESS;
}//ThreadTerminate


TVMStatus VMThreadID(TVMThreadIDRef threadref){
	if(threadref == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	//*threadref = running.ID;

	return VM_STATUS_SUCCESS;
}//ThreadID


TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef state){
	if(state == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	unsigned int i;
	
	for(i = 0; i < v_tcb.size(); i++)
                if(v_tcb[i].ID == thread)
                        break;
	
        if(i == v_tcb.size())
                return VM_STATUS_ERROR_INVALID_ID;

	*state = v_tcb[i].state;

	return VM_STATUS_SUCCESS;
}//ThreadState


TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]){
	const char *arg = argv[0];

	TVMMainEntry main = VMLoadModule(arg);		
	ticks = tickms;
	
	MachineInitialize(tickms);	
	MachineRequestAlarm(tickms*1000, AlarmCB, NULL);       //FYI could pass in a DataStructure instead of NULL
	MachineEnableSignals();

	//Create a special TCB for the main thread -> assign it to the running(global) -> pass the running into MachineSwitchContext as 1st param
	
	//TMachineFileCallback callback
	
	if(main == NULL)
		return(VM_STATUS_FAILURE);
	else{
		main(argc, argv);
		return(VM_STATUS_SUCCESS);
	}
}//VMStart

//VMAllThreads
//VMSchedule


TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor){
	
//	void *param = NULL;

	if(filename == NULL || filedescriptor == NULL)	
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	else if((*filedescriptor = open(filename, flags, mode))>0)
		return VM_STATUS_SUCCESS;
	else
		return VM_STATUS_FAILURE;	
}//FileOpen


TVMStatus VMFileClose(int filedescriptor){
	if((close(filedescriptor)) < 0)
		return VM_STATUS_FAILURE;

	else
		return VM_STATUS_SUCCESS; 
}//FileCLose

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
