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
};

void idle(void *param){
//	MachineEnableSignals();
	cout << "In Idle loop \n";
	while(1){
	}
}

TCB *running = new TCB((TVMThreadPriority)NULL, 0, NULL, NULL);
TCB *idle1 = new TCB((TVMThreadPriority)NULL, 0, idle, NULL);

vector <TCB> v_tcb;

vector <TCB> high;
vector <TCB> medium;
vector <TCB> low;

vector <TCB> waiting;
vector <TCB> sleeping;

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
	cout << "In Skeleton\n";	
	MachineEnableSignals();
	((TCB *)param1)->entry(((TCB *)param1)->param);
	cout<<"Problem is now VMTerminate\n";
	VMThreadTerminate(running->ID);// This will allow you to gain control back if the ActualThreadEntry returns
}//SkeletonEntry


TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid){
	//TMachineSignalState OldState;

	MachineEnableSignals();
	if(entry == NULL || tid == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
	else{
		//MachineSuspendSignals(&OldState);
		TCB block(prio, memsize, entry, param);
		*tid = v_tcb.size();
		block.ID = v_tcb.size();
		//tid_tcb.push_back(block.ID);
		v_tcb.push_back(block); 	
		//MachineEnableSignals();
	}

	return VM_STATUS_SUCCESS;
}


TVMMainEntry VMLoadModule(const char *module);

void AlarmCB(void *param){
	//TMachineSignalState OldState;
//	cout << "In ALARM \n";
	for(unsigned int i = 0; i < sleeping.size(); i++){
		if(sleeping[i].num_ticks == 0){
			cout<< "ONE STATE IS AWAKE\n";
			//MachineSuspendSignals(&OldState);
			sleeping[i].state = VM_THREAD_STATE_READY;
			if(sleeping[i].priority == VM_THREAD_PRIORITY_LOW)
				low.push_back(sleeping[i]);
			else if(sleeping[i].priority == VM_THREAD_PRIORITY_NORMAL)
				medium.push_back(sleeping[i]);
			else // high priority
				high.push_back(sleeping[i]);
			for(unsigned int j = 0; j < waiting.size(); j++)
				if(waiting[j].ID == sleeping[i].ID){
					waiting.erase(waiting.begin() + j);
					break;
				}
			if(running == idle1){
				cout<<"SHOULD BE IN HERE: IDLE1\n";
	                        sleeping.erase(sleeping.begin() + i);
				Scheduler();
			}
			else if(sleeping[i].priority == VM_THREAD_PRIORITY_HIGH && running->priority != VM_THREAD_PRIORITY_HIGH){
			      	sleeping.erase(sleeping.begin() + i);
                                Scheduler();
			}
			else if(sleeping[i].priority == VM_THREAD_PRIORITY_NORMAL && running->priority == VM_THREAD_PRIORITY_LOW){
                                sleeping.erase(sleeping.begin() + i);
                                Scheduler();
                        }
			else
				sleeping.erase(sleeping.begin() + i);
	 	//	MachineEnableSignals();
		}
		sleeping[i].num_ticks--;
		//cout<<sleeping[i].ID<<" "<<sleeping[i].num_ticks<<endl;
	}
	//call Scheduler (?)
	Scheduler();	
	//counter--;   //need count down timers for each sleeping thread (?)      
}//AlarmCB


/*void MachineCB(void *param){
	if(param == NULL)	
		
		
}*/

TVMStatus VMThreadSleep(TVMTick tick){
	running->num_ticks = tick;

	//TMachineSignalState OldState;
//	cout << "In Sleep \n";
	if(tick == VM_TIMEOUT_INFINITE)	
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	else{
		//MachineSuspendSignals(&OldState);	
		running->state = VM_THREAD_STATE_WAITING;
		sleeping.push_back(*running);
		//int i = sleeping.size() -1;
		//while(sleeping[i].num_ticks != 0);
		//sleeping.erase(sleeping.begin() + i);
		//MachineEnableSignals();
	}
	Scheduler();
	
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


//SMachineContext global_new_context;

TVMStatus VMThreadActivate(TVMThreadID thread){
	TMachineSignalState OldState;
	MachineSuspendSignals(&OldState);
	//SMachineContext curr_context;
	cout << "In Activate \n";
		
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
		low.push_back(v_tcb[i]);
	if(v_tcb[i].priority == VM_THREAD_PRIORITY_NORMAL)
                medium.push_back(v_tcb[i]);
	else
                high.push_back(v_tcb[i]);

	//create context
	MachineContextCreate(&v_tcb[i].MachineContext, Skeleton, &v_tcb[i], v_tcb[i].StackBase, v_tcb[i].st_size);
	//MachineEnableSignals();
	//handle scheduling
	//if(v_tcb[i].state == VM_THREAD_STATE_WAITING)
	Scheduler();
	MachineResumeSignals(&OldState);
	return VM_STATUS_SUCCESS;
}//ThreadActivate

void Scheduler(){
	TVMThreadID temp;
	//TMachineSignalState OldState;
//	cout << "In Scheduler \n";
		if(!(high.empty())){
			temp = high[0].ID;
			high.erase(high.begin());		
		}
		else if(!(medium.empty())){
			cout<<"POP OFF MAIN\n";
			temp = medium[0].ID;
                        medium.erase(medium.begin());	
		}
		else if (!(low.empty())){
			temp = low[0].ID;
                        low.erase(low.begin());
		}
		else{
			//MachineSuspendSignals(&OldState);
			if(running->state != VM_THREAD_STATE_RUNNING){ //run idle thread now
				idle1->state = VM_THREAD_STATE_RUNNING;
				cout << "Idle state \n";
				TCB *oldstate = running;
				cout<< running <<"address of MAIN" << oldstate->ID << endl;
				cout << idle1 << " " << idle1->ID << "IDLE BEFOrE PTR CHANGE" << endl;
				running = idle1;
				cout << running <<" "<< idle1->ID << endl;
				cout<< oldstate->ID << " "<< running->ID<<endl;
				MachineContextSwitch(&oldstate->MachineContext, &running->MachineContext);
				cout<<"Problem after conctext switch\n";
				//MachineEnableSignals();
				return;
			}	
			else
				return; 
		}	 

	//MachineSuspendSignals(&OldState);
	unsigned int i;
	for(i=0; i<v_tcb.size();i++)
		if(v_tcb[i].ID == temp)
			break;

	v_tcb[i].state =  VM_THREAD_STATE_RUNNING;
	
	if(running->state == VM_THREAD_STATE_RUNNING && running != idle1){
		if(running->priority == VM_THREAD_PRIORITY_HIGH)
			high.push_back(*running);
		else if(running->priority == VM_THREAD_PRIORITY_NORMAL)
			medium.push_back(*running);
		else if(running->priority == VM_THREAD_PRIORITY_LOW)
			low.push_back(*running);
	}
	else if(running->state == VM_THREAD_STATE_WAITING)
		waiting.push_back(*running);

	
	TCB *temp1 = running;
        running = &v_tcb[i];
	//MachineEnableSignals();
	MachineContextSwitch(&temp1->MachineContext, &running->MachineContext);
	//if(v_tcb[temp].state == VM_THREAD_STATE_WAITING)
	//	waiting.push(v_tcb[temp]);		
}//Scheduler

void pop_queue(TVMThreadID thread, int i){
	unsigned int h, m, l;

	if(v_tcb[i].ID == 0)
		return;
	else if(v_tcb[i].priority == VM_THREAD_PRIORITY_LOW){
		for(h = 0; h < low.size(); h++){	
			if(low[h].ID == thread)
				break;
		}
		low.erase(low.begin() + h);
	}
	else if(v_tcb[i].priority == VM_THREAD_PRIORITY_NORMAL){
		for(m = 0; m < low.size(); m++){
                        if(medium[m].ID == thread)
                                break;
                }
                medium.erase(medium.begin() + m);
	}
        else{ //if(v_tcb[i].priority == VM_THREAD_PRIORITY_HIGH){
		for(l = 0; l < low.size(); l++){
                        if(high[l].ID == thread)
                                break;
                }
                high.erase(high.begin() + l);
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
	unsigned int i;
	if(state == NULL)
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	
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
	//TMachineSignalState OldState;
	TVMThreadID idleID;

	//MachineSuspendSignals(&OldState);
	VMThreadCreate(idle, NULL, 0, (TVMThreadPriority)NULL, &idleID);
	idle1 = &v_tcb[0];
	//cout<< idle1->ID << " "<< idle1 << "IDLE ID" << endl;
	
	TVMMainEntry main = VMLoadModule(arg);		
	ticks = tickms;
	
	MachineInitialize(tickms);
	MachineRequestAlarm(tickms*1000, AlarmCB, NULL);       //FYI could pass in a DataStructure instead of NULL

	//Create a special TCB for the main thread -> assign it to be the running(global) -> pass the running into MachineSwitchContext as 1st param
	TCB *mainThread = new TCB(VM_THREAD_PRIORITY_NORMAL, 0, NULL, NULL);
	v_tcb.push_back(*mainThread);
	v_tcb[1].ID = 1;
	running = &v_tcb[1];	
	cout << running->ID << "MAIN ID" << v_tcb[0].ID << "IDLE ID" << endl;

	//create context for idle	
	MachineContextCreate(&idle1->MachineContext, Skeleton, idle1->param, idle1->StackBase, idle1->st_size);
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
#include "VirtualMachine.h"
