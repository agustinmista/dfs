#ifndef __CLIENTHANDLER_H__
#define __CLIENTHANDLER_H__

// Check for a valid filename.
int is_valid(char *s);

// Handle the communication between a client and his assigned worker.
void *handle_client(void *session);

// Parse a string into a Request structure, return NULL if something went wrong.
Request *parseRequest(Session *s, char *string);

#endif
