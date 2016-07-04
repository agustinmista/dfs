#ifndef __DISPATCHER_H__
#define __DISPATCHER_H__

// Returns a random worker id.
int getWorkerId();

// Dispatch new connections, creating a new session 
// and setting a random worker for each client.
void *init_dispatcher(void *s);

#endif
