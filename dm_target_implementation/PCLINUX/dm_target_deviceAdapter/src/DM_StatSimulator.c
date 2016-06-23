/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C) 2014 Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'. 
 *
 *---------------------------------------------------------------------------
 * File        : DM_StatSimulator.c
 *
 * Created     : 27/05/2010
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "DM_SharedData.h"

DM_SharedData* data;

static void _updateData()
{
   if (rand()%100+1 <= 5) { data->continuity = 0; }
   if (rand()%100+1 >= 90)
   data->packetsReceived += rand() % 100 + 1; // on ajoute un nb compris entre 1 et 100
   if (rand()%100+1 <= 10) { data->packetsLost += rand() % 20 + 1; } // dans 10 % des cas, on ajoute un nb compris entre 1 et 20
   else if (rand()%100+1 <= 20) { data->packetsLost += rand() % 10 + 1; } // dans 20 % des autres cas, on ajoute un nb compris entre 1 et 10
   data->timestamp = time(NULL);
}

static void _pushData()
{
   // pose d'un verrou, avec attente si nécessaire
   LIGHT_LOCK_1(data->flag);

   // accès à la mémoire partagée
   _updateData();

   // suppression du verrou
   LIGHT_UNLOCK_1(data->flag);
}

int main()
{
    int shmid;
    key_t key;

    /*
     * We'll name our shared memory segment
     * "5678".
     */
    key = 5678;

    /*
     * Create the segment.
     */
    if ((shmid = shmget(key, sizeof(DM_SharedData), IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    if ((data = shmat(shmid, NULL, 0)) == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    /*
     * Now put some things into the memory for the
     * other process to read.
     */
    LIGHT_LOCK_INIT(data->flag);
    data->continuity = 0;
    data->packetsReceived = 0;
    data->packetsLost = 0;
    data->timestamp = 0;

    srand(time(NULL));

    int i = 0;
    for (i=0; i<300000; i++)
    {
      _pushData();
      sleep(1);
    }

    shmdt(data);

    return 0;
}
