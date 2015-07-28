#include "Common.h"
#include "Worker.h"

/*struct mq_attr attr;
attr.mq_maxmsg = 300;
attr.mq_msgsize = 10000;
attr.mq_flags = 0;*/

/*
 * Está lejos de estar terminada la función, sobre todo habría que ver
 * bien el uso y la inicialización de los punteros y si las cadenas
 * deberían tener longitud fija o no.
 * 
 * También hay que terminar de ver el paso de mensajes entre los workers
 * y con el handler.
 * 
 * Como todavía no se como recibe las cosas desde afuera el handler, por las dudas
 * el mensaje al handler tiene un R_ al inicio del comando, hay que verlo bien.
 * 
 * Por ahora, la idea es que cualquier worker que reciba un msj LSD...
 * va a pasarselo al worker0, él va a mandar al sig I_LSD dst lista, 
 * donde I_LSD se va a usar para los msjs internos, dst debería ser el 
 * nombre de la queue que mandó el pedido original y lista la lista de
 * los nombres de archivo.. y así... hasta el último worker que le responde
 * al handler con R_LSD y la lista. El handler debería agregarle el null
 * y esas cosas supongo.
 * 
 *  Capas después directamente hago una función aparte que liste los
 * nombres de fichero o algo así.
 * 
 * El parseo de la parte que tiene los nombres de las respuestas anteriores
 * no me convence, capas después veo si no lo puedo tratar como un bloque
 * o directamente lo envío desde cada worker por separado al handler,
 * total el orden no importa.
 * 
 * Bueno, en fin, es una idea nada más. Hago el commit para que la vayan
 * viendo. Más tarde sigo, lo arreglo un poco y lo reviso bien.
 */

void *worker(void *w_info){
	File *files_init, *files;
    
    int n_files = 0;
    int i;
    
    char message[100];
    char *parsing, *saveptr, *cmd, *arg0, *arg1, *dst, *ans;
	
    // Parse worker args
    int wid      = ((Worker_Info *)w_info)->id;
    mqd_t wqueue = ((Worker_Info *)w_info)->queue;
    free(w_info);	
    
    
    while(1){
        
        memset(message, 0, 100);
        files = files_init;
              
        if(mq_receive(wqueue, message, 100, NULL) >= 0){
			
			parsing = strtok_r(message, " ", &saveptr);
			if (strcmp (parsing,"LSD") == 0){
				
				if(wid == 0){
					cmd = parsing;
					parsing = strtok_r(NULL, " ", &saveptr);
				
					if (parsing != NULL){
					
						dst = parsing;
						parsing = strtok_r(NULL, " ", &saveptr);
						
						if(parsing == NULL){
						
							ans = "I_LSD ";
							
							strcat(ans, dst);
							
							for(i=0; i < n_files; i++){
								strcat(ans, files->name);
								strcat(ans, " ");
								files = files->next;
							}				
							//mq_send(sig Worker,  ans, sizeof(ans), MQ_PRIO_MAX);
						}			
					}
				}
				//else
					//mq_send(dir_worker_0, message, 100, MQ_PRIO_MAX)
			}	
			else if (strcmp (parsing, "I_LSD") == 0){
					
				cmd = parsing;
				
				parsing = strtok_r(NULL, " ", &saveptr); //dst
				
				if (parsing != NULL){
					
					dst = parsing;
					parsing = strtok_r(NULL, " ", &saveptr); //primer archivo si no es nulo
					
					if(wid < N_WORKERS - 1){
						ans = cmd;
						strcat(ans, " ");
						strcat(ans, dst);
						
						while(parsing != NULL){
							strcat(ans, parsing);
							parsing = strtok_r(NULL, " ", &saveptr);
						}
												
						for(i=0; i < n_files; i++){
								strcat(ans, files->name);
								strcat(ans, " ");
								files = files->next;
						}
						//mq_send(dir_wid+1, ans, sizeof(ans), MQ_PRIO_MAX);
					}
					else if(wid == N_WORKERS - 1){
						
						ans = "R_LSD "; 	
						
						while(parsing != NULL){
							strcat(ans, parsing);
							parsing = strtok_r(NULL, " ", &saveptr);
						}
						
						for(i=0; i < n_files; i++){
								strcat(ans, files->name);
								strcat(ans, " ");
								files = files->next;
						}					
						
						//mq_send(dst, ans, sizeof(ans), MQ_PRIO_MAX - 1);
					}
			}
			else if (strcmp (parsing,"DEL") == 0){
				//Crear una función que analice si el archivo
				//se encuentra en este worker
				//Si no está enviar mensaje indicando el comando y el nro
				//de worker
				
				//Borrar el archivo si está en alguno
				
				parsing = strtok_r(NULL, " ", &saveptr);
				if(parsing != NULL){
					arg0 = parsing;
					parsing = strtok_r(NULL, " ", &saveptr);
					if(parsing == NULL)
						printf ("DEL\n");
					else
						printf("Error de comando\n");
				}	
			}
			else if (strcmp (parsing,"CRE") == 0)
				printf ("CRE\n");
			else if (strcmp (parsing,"OPN") == 0)
				printf ("OPN\n");
			else if (strcmp (parsing,"WRT") == 0)
				printf ("WRT\n");
			else if (strcmp (parsing,"REA") == 0)
				printf ("REA\n");
			else if (strcmp (parsing,"CLO") == 0)
				printf ("CLO\n");
			else if (strcmp (parsing,"BYE") == 0)
				printf ("BYE\n");
			else
				printf("error de comando");
		}
		}
    }
    mq_close(wqueue);
    return 0;

}

int init_workers(){
    
    for(int i = 0; i<N_WORKERS; i++){
        
        // Instance the worker message queue
        char *worker_name;
        asprintf(&worker_name, "/w%d", i);
        
        if((worker_queues[i] = mq_open(worker_name, O_RDWR | O_CREAT, 0666, NULL)) == (mqd_t) -1)
            ERROR("\nDFS_SERVER: Error opening message queue for workers\n");
        
        Worker_Info *newWorker = malloc(sizeof (Worker_Info));
        newWorker->id = i;
        newWorker->queue = worker_queues[i];
        
        // Spawn a new worker
        pthread_create(&workers[i], NULL, worker, newWorker);
    }
    return 0;
}
