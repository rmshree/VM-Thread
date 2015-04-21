#include "VirtualMachine.h"
#include "VirtualMachineUtils.c"
#include "Machine.h"

extern "C"{


TVMStatus VMStart(int tickms, int machinetickms, int argc,
char *argv[]){
	const char *arg = argv[1];
	TVMMainEntry ret;
	ret = VMLoadModule(arg);		
	
	MachineInitialize(1000);	
	MachineRequestAlarm(1000);	//need to write callback func
	MachineEnableSignals();
	
	TMachineFileCallback callback
	//VMMain(); 
}

}//end of extern
