#include "Common.h"
#include "ClientHandler.h"

void *handle_client(void *s){
    
    Session *session = (Session *) s;
    
    printf("Hola! soy el ClientHandler del cliente id: %d\n", session->client_id);

    while(1){
        //---
        //--- Where the magic happens!
        //---
    }   
    
    return NULL;
}