#include "VirtualMachine.h"
#include "Machine.h"

extern "C"{
int volatile counter;

TVMMainEntry VMLoadModule(const char *module);

void AlarmCB(void *param){
        for (counter;counter > 0; counter--);   //need count down timers for each sleeping thread (?)      
}//AlarmCB

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]){
	const char *arg = argv[0];

	TVMMainEntry ret = VMLoadModule(arg);		
	counter = tickms;
	
	//MachineInitialize(1000);	
	//MachineRequestAlarm(1000, AlarmCB, NULL);       //FYI could pass in a DataStructure instead of NULL
	//MachineEnableSignals();
	
	//TMachineFileCallback callback
	if(ret == NULL)
		return(VM_STATUS_FAILURE);
	else{
		ret(argc, argv);
		return(VM_STATUS_SUCCESS);
	}
}//VMStart


TVMStatus VMFileWrite(int filedescriptor, void *data, int *length){	//needs to work with machine something?
	if(data == NULL || length == NULL)
		return(VM_STATUS_ERROR_INVALID_PARAMETER);
	else{
		write(filedescriptor, data, *length);
		return(VM_STATUS_SUCCESS);
	}

}



}//end of extern