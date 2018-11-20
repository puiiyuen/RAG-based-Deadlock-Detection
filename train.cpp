//
// Created by peiyuan on 15/11/18.
//

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define NORTH 0
#define WEST 1
#define SOUTH 2
#define EAST 3

struct semLock
{
    sem_t *dirLock[4];
    sem_t *junction;
    sem_t *rwMatrix;
};

using namespace std;

void writeMatrix(int i,int j,int signal)
{
    int m=0;

    string line;
    string temp;

    char sig[1];
    sprintf(sig,"%d", signal);

    fstream file("matrix.txt");
    if(file.is_open())
    {
        while(getline(file,line)&&m<=i)
        {
            long linePos = file.tellp();
            if(m==i)
            {
                long pos=linePos-2*(3-j)-2;
                file.seekp(pos);
                file.write(sig,1);

                break;
            }
            else
                m++;
        }

        file.close();
    }
}

void train(int id,int dir,semLock *&lock)
{
    string direction,rightDir;
    if(dir==NORTH)
    {
        direction="North";
        rightDir="West";
    }
    else if(dir==SOUTH)
    {
        direction="South";
        rightDir="East";
    }
    else if(dir==WEST)
    {
        direction="West";
        rightDir="South";
    }
    else if(dir==EAST)
    {
        direction="East";
        rightDir="North";
    }
    else
    {
        direction="Wrong Direction";
        rightDir="Wrong Direction";
    }

    //Train started
    cout<<"---Train <"<<id<<"> "<<direction<<" train started---"<<endl;

    //Train requests for lock
    sem_wait(lock->rwMatrix);
    writeMatrix(id,dir,1);
    cout<<"---Train <"<<id<<"> Requests for "<<direction<<"-Lock---"<<endl;
    sem_post(lock->rwMatrix);

    //Train acquires lock
    sem_wait(lock->dirLock[dir]);
    sem_wait(lock->rwMatrix);
    writeMatrix(id,dir,2);
    cout<<"---Train <"<<id<<"> Acquires "<<direction<<"-Lock---"<<endl;
    sem_post(lock->rwMatrix);

    //Train request for right direction
    sem_wait(lock->rwMatrix);
    writeMatrix(id,(dir+1)%4,1);
    cout<<"---Train <"<<id<<"> Requests for "<<rightDir<<"-Lock---"<<endl;
    sem_post(lock->rwMatrix);

    //Train acquires right direction lock
    sem_wait(lock->dirLock[(dir+1)%4]);
    sem_wait(lock->rwMatrix);
    writeMatrix(id,(dir+1)%4,2);
    cout<<"---Train <"<<id<<"> Acquires "<<rightDir<<"-Lock---"<<endl;
    sem_post(lock->rwMatrix);

    //Train passing junction
    cout<<"---Train <"<<id<<"> Requests for Junction-Lock---"<<endl;
    sem_wait(lock->junction);
    cout<<"---Train <"<<id<<"> Acquires Junction-Lock---"<<endl;
    cout<<"---Train <"<<id<<"> is passing junction---"<<endl;
    sleep(2);
    sem_post(lock->junction);
    cout<<"---Train <"<<id<<"> Releases Junction-Lock---"<<endl;

    //Train releases locks
    sem_wait(lock->rwMatrix);
    sem_post(lock->dirLock[dir]);
    writeMatrix(id,dir,0);
    cout<<"---Train <"<<id<<"> Releases "<<direction<<"-Lock---"<<endl;
    sem_post(lock->dirLock[(dir+1)%4]);
    writeMatrix(id,(dir+1)%4,0);
    cout<<"---Train <"<<id<<"> Releases "<<rightDir<<"-Lock---"<<endl;
    sem_post(lock->rwMatrix);


}

bool initSemaphore(semLock *&sLock)
{
    sLock->dirLock[NORTH]=sem_open("north",O_CREAT,0777,1);
    sLock->dirLock[SOUTH]=sem_open("south",O_CREAT,0777,1);
    sLock->dirLock[WEST]=sem_open("west",O_CREAT,0777,1);
    sLock->dirLock[EAST]=sem_open("east",O_CREAT,0777,1);
    sLock->rwMatrix=sem_open("rwMatrix",O_CREAT,0777,1);
    sLock->junction=sem_open("junction",O_CREAT,0777,1);

    return !(sLock->junction == SEM_FAILED ||
    sLock->rwMatrix==SEM_FAILED ||
    sLock->dirLock[NORTH]==SEM_FAILED ||
    sLock->dirLock[SOUTH]==SEM_FAILED ||
    sLock->dirLock[WEST]==SEM_FAILED ||
    sLock->dirLock[EAST]==SEM_FAILED);
}

bool closeSem(semLock *&sLock)
{
    sem_close(sLock->junction);
    sem_close(sLock->rwMatrix);
    sem_close(sLock->dirLock[EAST]);
    sem_close(sLock->dirLock[WEST]);
    sem_close(sLock->dirLock[SOUTH]);
    sem_close(sLock->dirLock[NORTH]);

    return true;

}

int main(int argc, char **argv)
{
    semLock *lock=(semLock *)malloc(sizeof(semLock));
    
    int trainId=atoi(argv[1]);
    int direction=atoi(argv[2]);

    if(initSemaphore(lock))
    {
        train(trainId,direction,lock);
        closeSem(lock);
    }

    cout<<"---Train <"<<trainId<<"> Finished passing---"<<endl;

    return 0;
}