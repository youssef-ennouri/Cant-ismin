#ifndef PSE_INCLUDE_H
#define PSE_INCLUDE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include "erreur.h"
#include "ligne.h"
#include "resolv.h"
#include "datathread.h"
#include "dataspec.h"

#define FAUX	0
#define VRAI	1

#endif