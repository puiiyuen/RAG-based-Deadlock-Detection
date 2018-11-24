/*
 * manager.cpp
 * fork trains and detect deadlock
 */

//
// Created by peiyuan on 15/11/18.
//

#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define NORTH 0
#define WEST 1
#define SOUTH 2
#define EAST 3

#define INF 65535

using namespace std;

/*
 * Set of semaphore
 * dirLock: semaphore for direction
 * junction: semaphore for junction
 * rwMatrix: semaphore for read write matrix.txt
 */

struct semLock
{
    sem_t *dirLock[4];
    sem_t *junction;
    sem_t *rwMatrix;
};


/*
 * Function: readQueue
 * read train sequence from sequence.txt
 * return: vector<char> queue
 */

vector<char> readQueue()
{
    string content = "-1";

    fstream file("sequence.txt");

    if (file.is_open()) {
        getline(file, content);
        vector<char> queue(content.begin(), content.end());
        file.close();
        return queue;
    } else {
        vector<char> queue(content.begin(), content.end());
        return queue;
    }
}

/*
 * Function: convertQueue
 * convert char queue to int queue
 * param: preQueue: The queue needs to be convert
 * return: vector<int> queue
 */

vector<int> convertQueue(vector<char> preQueue)
{
    vector<int> newQueue;
    for (char i : preQueue) {
        if (i == 'N')
            newQueue.push_back(NORTH);
        else if (i == 'S')
            newQueue.push_back(SOUTH);
        else if (i == 'W')
            newQueue.push_back(WEST);
        else if (i == 'E')
            newQueue.push_back(EAST);
        else
            newQueue.push_back(-1);
    }
    return newQueue;
}

/*
 * Function: initMatrix
 * initialize matrix.txt base on the number of train
 * param: numOfTrain: number of train
 * return: true: Successfully initialize false: initialize failed
 */

bool initMatrix(int numOfTrain)
{
    fstream file("matrix.txt");
    if (file.is_open()) {
        for (int i = 0; i < numOfTrain; i++) {
            file << "0 0 0 0" << endl;
        }
        file.close();
        return true;
    }
    return false;
}

/*
 * Function: initialize named semaphore
 * param: sLock: set of semaphore
 * return: true: Successfully initialize false: initialize failed
 */

bool initSemaphore(semLock *&sLock)
{
    sLock->dirLock[NORTH] = sem_open("north", O_CREAT, 0777, 1);
    sLock->dirLock[SOUTH] = sem_open("south", O_CREAT, 0777, 1);
    sLock->dirLock[WEST] = sem_open("west", O_CREAT, 0777, 1);
    sLock->dirLock[EAST] = sem_open("east", O_CREAT, 0777, 1);
    sLock->rwMatrix = sem_open("rwMatrix", O_CREAT, 0777, 1);
    sLock->junction = sem_open("junction", O_CREAT, 0777, 1);

    return !(sLock->junction == SEM_FAILED ||
             sLock->rwMatrix == SEM_FAILED ||
             sLock->dirLock[NORTH] == SEM_FAILED ||
             sLock->dirLock[SOUTH] == SEM_FAILED ||
             sLock->dirLock[WEST] == SEM_FAILED ||
             sLock->dirLock[EAST] == SEM_FAILED);
}

/*
 * Function: close the connection of named semaphore and delete semaphores
 * param: sLock: set of semaphore
 * return: true: Successfully close and delete
 */

bool unlinkCloseSem(semLock *&sLock)
{
    sem_close(sLock->dirLock[NORTH]);
    sem_close(sLock->dirLock[SOUTH]);
    sem_close(sLock->dirLock[WEST]);
    sem_close(sLock->dirLock[EAST]);
    sem_close(sLock->rwMatrix);
    sem_close(sLock->junction);

    sem_unlink("north");
    sem_unlink("south");
    sem_unlink("west");
    sem_unlink("east");
    sem_unlink("rwMatrix");
    sem_unlink("junction");

    return true;

}

/*
 * function: getRandom
 * get random double number. range:(0.2-0.7)
 * return: double random number
 */

double getRandom()
{
    return (double) rand() / RAND_MAX;
}

/*
 * function: readMatrix
 * read the matrix[i][j] from matrix.txt
 * param: i: row index j: column index
 * return: the content of matrix[i][j]
 * if cannot read the matrix[i][j] return -1;
 */

int readMatrix(int i, int j)
{
    int m = 0, n = 0;
    string line;
    string temp;

    fstream file("matrix.txt");
    if (file.is_open()) {
        while (getline(file, line) && m <= i) {
            if (m == i) {
                istringstream myNum(line);
                while (getline(myNum, temp, ' ')) {
                    if (n == j) {
                        file.close();
                        return atoi(temp.c_str());
                    }
                    n++;
                }
                file.close();
                return -1;
            }
            m++;
        }
        file.close();
        return -1;
    }
    return -1;
}

/*
 * function: checkDeadlock
 * check the deadlock from train
 * param: sLock: set of semaphore
 *        matrix: new matrix for detect deadlock
 *        numOfTrain: number of train
 *        childPid: store pid of child process
 * return: true: if there is deadlock false: no deadlock
 */

bool checkDeadlock(semLock *&sLock, int **&matrix, int numOfTrain, pid_t childPid[])
{
    sem_wait(sLock->rwMatrix);

    int count[numOfTrain + 4];//count in-degree of each node
    int stack[numOfTrain + 4], top = -1;
    int numOfNoInDegree = 0;
    vector<int> deadLockTrain;


    //topological sort check if there is a circle
    for (int j = 0; j < numOfTrain + 4; j++) {
        count[j] = 0;
    }

    //convert matrix.txt file to deadlock detect matrix
    for (int i = 0; i < numOfTrain; i++) {
        for (int j = 0; j < 4; j++) {
            if (readMatrix(i, j) == 2) {
                matrix[numOfTrain + j][i] = 1;
                count[i]++;
            } else if (readMatrix(i, j) == 1) {
                matrix[i][numOfTrain + j] = 1;
                count[numOfTrain + j]++;
            } else
                continue;
        }
    }

    //push node with 0 in-degree
    for (int k = 0; k < numOfTrain + 4; k++) {
        if (count[k] == 0) {
            top++;
            stack[top] = k;
        }
    }


    //delete node with 0 in-degree and all edges started from this node
    while (top > -1) {
        int t = stack[top];
        top--;
        numOfNoInDegree++;
        for (int i = 0; i < numOfTrain + 4; i++) {
            if (matrix[t][i] == 1) {
                count[i]--;
                matrix[t][i] = 0;
                if (count[i] == 0) {
                    top++;
                    stack[top] = i;
                }
            }

        }
    }

    //check if there are nodes that they still stay in the deadlock detect matrix
    if (numOfNoInDegree == numOfTrain + 4) {
        sem_post(sLock->rwMatrix);
        return false;
    } else {

        //print the deadlock circle
        cout << "\n---There is deadlock---" << endl;
        int start = -1, end = -1, tempStart;

        for (int i = 0; i < numOfTrain + 4; i++) {
            if (count[i] > 0) {
                start = i;
                break;
            }
        }

        tempStart = start;

        while (start != end) {
            for (int j = 0; j < numOfTrain + 4; j++) {
                if (matrix[tempStart][j] == 1) {
                    if (tempStart < numOfTrain)
                        cout << "Train<" << tempStart << "> --> ";
                    else {
                        if (tempStart - numOfTrain == NORTH)
                            cout << "North-Lock --> ";
                        else if (tempStart - numOfTrain == SOUTH)
                            cout << "South-Lock --> ";
                        else if (tempStart - numOfTrain == WEST)
                            cout << "West-Lock -->";
                        else if (tempStart - numOfTrain == EAST)
                            cout << "East-Lock --> ";
                        else
                            cout << "Wrong item";
                    }
                    tempStart = j;
                    end = j;
                    break;
                }
            }
        }

        if (start < numOfTrain)
            cout << "Train <" << start << ">" << endl;
        else {
            if (start - numOfTrain == NORTH)
                cout << "North-Lock" << endl;
            else if (start - numOfTrain == SOUTH)
                cout << "South-Lock" << endl;
            else if (start - numOfTrain == WEST)
                cout << "West-Lock" << endl;
            else if (start - numOfTrain == EAST)
                cout << "East-Lock" << endl;
            else
                cout << "Wrong item" << endl;
        }

        cout << endl;

        //kill child processes when deadlock
        for (int i = 0; i < numOfTrain; i++) {
            if (childPid[i] != -999) {
                kill(childPid[i], SIGTERM);
                cout << "Train <" << i << "> is killed (pid: " << childPid[i] << ")" << endl;
            }
        }

        sem_post(sLock->rwMatrix);
        return true;
    }

}

/*
 * function: manager
 * manage trains, semaphore, initialize values
 *
 */

void manager()
{

    //read the sequence and convert
    vector<int> trainQueue = convertQueue(readQueue());

    int numOfTrain = trainQueue.size();
    int **deadLockMatrix;
    pid_t childPid[numOfTrain];

    //initialize semaphores
    semLock *lock = (semLock *) malloc(sizeof(semLock));

    //clear semaphores
    unlinkCloseSem(lock);

    //initial deadlock matrix
    deadLockMatrix = (int **) malloc((4 + numOfTrain) * sizeof(int *));
    for (int i = 0; i < numOfTrain + 4; i++) {
        deadLockMatrix[i] = (int *) malloc((4 + numOfTrain) * sizeof(int *));
        for (int j = 0; j < numOfTrain + 4; j++) {
            if (i == j)
                deadLockMatrix[i][j] = 0;
            else
                deadLockMatrix[i][j] = INF;
        }
    }

    //initial array to save pid of child process
    for (int c = 0; c < numOfTrain; c++) {
        childPid[numOfTrain] = -999;
    }

    //initialize matrix.txt and semaphore
    if (!trainQueue.empty() && initMatrix(numOfTrain) && initSemaphore(lock)) {
        double p = 0;
        while (p < 0.2 || p > 0.7) {
            cout << "Input a number (0.2-0.7) for check deadlock or create new train process:";
            cin >> p;
        }

        int intTrainNo = 0;
        char *trainNO = (char *) malloc(sizeof(int));
        char *trainDir = (char *) malloc(sizeof(int));

        while (true) {
            if (getRandom() < p) {

                //check deadlock
                if (checkDeadlock(lock, deadLockMatrix, numOfTrain, childPid)) {
                    cout << "\nChild processes terminated" << endl;
                    cout << "Exiting Manager..." << endl;
                    unlinkCloseSem(lock);
                    exit(0);
                } else {
                    //no deadlock check if all trains passed
                    if (trainQueue.empty()) {
                        sem_wait(lock->rwMatrix);
                        bool free = true;
                        for (int i = 0; i < numOfTrain; ++i) {
                            for (int j = 0; j < 4; ++j) {
                                if (readMatrix(i, j) != 0) {
                                    free = false;
                                    break;
                                }
                            }
                        }
                        if (free) {
                            cout << "\n---All trains have passed junction---" << endl;
                            sem_post(lock->rwMatrix);
                            unlinkCloseSem(lock);
                            exit(0);
                        } else {
                            sem_post(lock->rwMatrix);
                        }
                    }
                }
            } else {

                //fork child process execute trains
                if (!trainQueue.empty()) {
                    sprintf(trainNO, "%d", intTrainNo);
                    sprintf(trainDir, "%d", trainQueue[0]);
                    childPid[intTrainNo] = fork();
                    if (childPid[intTrainNo] == 0) {
                        execlp("./train", trainNO, trainDir, NULL);
                        exit(0);
                    } else {
                        trainQueue.erase(trainQueue.begin());
                        intTrainNo++;
                    }
                }
            }

        }
    } else {
        //initialize failed
        unlinkCloseSem(lock);
        cout << "Initialize Failed..." << endl;
        cout << "Program terminated..." << endl;
        exit(0);
    }
}


/*
 * main function
 */

int main()
{

    //random seed
    srand((unsigned) time(nullptr));

    manager();

    return 0;
}
