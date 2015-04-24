#include "VirtualMachine.h"
#include "Machine.h"
#include <fcntl.h>
#include <unistd.h>

extern "C"{
int volatile counter;

TVMMainEntry VMLoadModule(const char *module);

void AlarmCB(void *param){
	counter--;   //need count down timers for each sleeping thread (?)      
}//AlarmCB


/*void MachineCB(void *param){
	if(param == NULL)	
		
		
}*/

TVMStatus VMThreadSleep(TVMTick tick){
	if(tick == VM_TIMEOUT_INFINITE)	
		return VM_STATUS_ERROR_INVALID_PARAMETER;

	sleep(10*tick);
//	else if(tick == VM_TIMEOUT_IMMEDIATE)
	if(counter == 0)
		return VM_STATUS_SUCCESS;
}//ThreadSleep


TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]){
	const char *arg = argv[0];

	TVMMainEntry ret = VMLoadModule(arg);		
	counter = tickms;
	
	MachineInitialize(1000);	
	MachineRequestAlarm(1000, AlarmCB, NULL);       //FYI could pass in a DataStructure instead of NULL
	MachineEnableSignals();
	
	//TMachineFileCallback callback
	
	if(ret == NULL)
		return(VM_STATUS_FAILURE);
	else{
		ret(argc, argv);
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
