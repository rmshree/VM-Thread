#include "VirtualMachine.h"
#include "Machine.h"
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <queue>

extern "C"{

using namespace std;

int volatile counter;
int ticks;

typedef void (*TVMThreadEntry)(void *);

class TCB{
	public:

	TCB(){};
	TCB(TVMThreadPriority prior, int memsize, TVMThreadEntry entry1, void *param1); 
	TVMThreadID ID;
	TVMStatus state;	
	TVMThreadPriority priority;
	int st_size;
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

TCB:: TCB(TVMThreadPriority prior, int memsize, TVMThreadEntry entry1, void *param1){
	priority = prior;
	state = VM_THREAD_STATE_DEAD;
	int st_size = memsize;
	StackBase = new uint8_t[memsize];
	entry = entry1;
	//*(void *)param = *(void *)param1;
	num_ticks = ticks;
}


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


TVMStatus VMThreadActivate(TVMThreadID thread){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);

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

	//handle scheduling

	return VM_STATUS_SUCCESS;
}//ThreadActivate

void Scheduler(TVMThreadID thread){
	
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
	
	void *param = NULL;

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
