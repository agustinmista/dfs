#include "Common.h"
#include "ClientHandler.h"

void *handle_client(void *s){
    
    Session *session = (Session *) s;
	
	//formato de mensaje a worker: CMD args(dependiendo de CMD) client_queue
	//mq_receive con direccion client_name que corresponda no?
	
    while(1){
        //---
        //--- Where the magic happens!
        //---
    }   
    
    return NULL;
}
