#include "VirtualMachine.h"
#include "VirtualMachineUtils.c"
#include "Machine.h"

extern "C"{
//:{D
int volatile counter;

void AlarmCB(void *param){
        for (counter;counter > 0; counter--);   //need count down timers for each sleeping thread (?)      
}//AlarmCB

TVMStatus VMStart(int tickms, int machinetickms, int argc, char *argv[]){
	const char *arg = argv[1];
	TVMMainEntry ret = VMLoadModule(arg);		
	counter = tickms;
	
	MachineInitialize(1000);	
	MachineRequestAlarm(1000, AlarmCB, NULL);       //FYI could pass in a DataStructure instead of NULL
	MachineEnableSignals();
	
	TMachineFileCallback callback
	//VMMain(); 
}//VMStart

}//end of extern
