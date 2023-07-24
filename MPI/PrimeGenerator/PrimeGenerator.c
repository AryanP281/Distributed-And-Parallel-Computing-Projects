
#include<stdlib.h>
#include<stdio.h>
#include<mpi.h>
#include<time.h>

#define K 5
#define false 0
#define true 1
#define PRIME_COUNT 1000
#define BATCH_SIZE 100

void runMaster();
void runSlave();
char isPrime(int n);
int gcd(int a, int b);
int power(int a, unsigned int n, int p);
void generateBatch(int* buffer, char* seen);

int pRank; //The rank of the process

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    //Getting process's rank
    MPI_Comm_rank(MPI_COMM_WORLD, &pRank);

    //Executing correct process
    if(!pRank)
        runMaster();
    else
        runSlave();

    MPI_Finalize();

    return 0;
}

void runMaster()
{
    //Getting size of communicator
    int commSize;
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);

    //Providing the initial set of inputs
    srand(time(NULL));
    char* seen = (char*)calloc(INT32_MAX, sizeof(char)); //Keeps track of numbers that have already been processed
    int buffer[commSize-1][BATCH_SIZE]; //The random numbers generated as potential prime candidates for each slave
    int primesReturned[commSize-1] = {0}; //The prime numbers returned by each process from the last batch
    MPI_Request requests[commSize];
    for(int i = 1; i < commSize; ++i)
    {
        generateBatch(buffer[i-1], seen);
        MPI_Send(buffer[i-1], BATCH_SIZE, MPI_INT, i, 0, MPI_COMM_WORLD); //Sending the candidate numbers to slave for processing
        MPI_Irecv(primesReturned+i-1, 1, MPI_INT, i, 1, MPI_COMM_WORLD, requests+i-1); //Getting number of primes to be returned by the slave 
    }
    
    //Executing the master loop
    int primes[PRIME_COUNT] = {0};
    int primesFound = 0;
    MPI_Request dataSendRequests[commSize-1];
    while(primesFound != PRIME_COUNT)
    {
        for(int i = 1; i < commSize; ++i)
        {
            //Checking if any slave has returned any prime numbers
            int testFlag = 0;
            MPI_Status status;
            MPI_Test(requests+i-1, &testFlag, &status);

            if(testFlag)
            {
                //Getting the prime numbers
                int slaveOutput[primesReturned[i-1]];
                MPI_Recv(slaveOutput, primesReturned[i-1], MPI_INT, i, 2, MPI_COMM_WORLD, &status);

                //Saving the received prime numbers
                for(int j = 0; j < primesReturned[i-1] && primesFound < PRIME_COUNT; ++j)
                {
                    primes[primesFound++] = slaveOutput[j];
                    printf("%d\n", primes[primesFound-1]);
                }

                //Providing new batch of inputs to the slave
                if(primesFound < PRIME_COUNT)
                {
                    //Sending new data
                    generateBatch(buffer[i-1], seen);
                    MPI_Isend(buffer[i-1], BATCH_SIZE, MPI_INT, i, 0, MPI_COMM_WORLD, dataSendRequests+i-1);

                    //Listeneing for output
                     MPI_Irecv(primesReturned+i-1, 1, MPI_INT, i, 1, MPI_COMM_WORLD, requests+i-1); //Getting number of primes to be returned by the slave 
                }

                printf("Primes found: %d\n\n", primesFound);
                fflush(NULL);
            }
        }
    }

    //Killing the slaves
    char killStatus[commSize-1];
    for(int j = 0; j < commSize-1; ++j)
    {
        killStatus[j] = true;
    }
    MPI_Request killRequests[commSize-1];
    for(int i = 0; i < commSize-1; ++i)
    {   
        MPI_Isend(killStatus+i, 1, MPI_CHAR, i+1,3,MPI_COMM_WORLD,killRequests+i);
    }

    //Displaying the generated primes
    printf("\nThe generated primes are: ");
    for(int i = 0; i < PRIME_COUNT; ++i)
    {
        printf("%d, ", primes[i]);
    }
    fflush(NULL);

    free(seen);
}

void runSlave()
{
    int numsToTest[BATCH_SIZE] = {0};
    int primes[BATCH_SIZE] = {0};
    int primeCount;
    MPI_Status recvStatus;
    MPI_Request recvRequest;

    //Getting the initial set of numbers
    MPI_Irecv(numsToTest, BATCH_SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, &recvRequest);

    //Setting listener to check if slave needs to be killed
    char kill = 0;
    MPI_Request killRequest;
    MPI_Irecv(&kill, 1, MPI_CHAR, 0, 3, MPI_COMM_WORLD,&killRequest);

    int bc = 1;

    char primesFound = 0;
    do
    {
        //Checking if data has been received
        int dataReqTestFlag;
        MPI_Test(&recvRequest, &dataReqTestFlag, &recvStatus);
        if(dataReqTestFlag)
        {            
            //Checking the inputs for primality
            primeCount = 0;
            for(int i = 0; i < BATCH_SIZE; ++i)
            {
                if(isPrime(numsToTest[i]))
                    primes[primeCount++] = numsToTest[i];
            }

            //Returning primes to master
            MPI_Send(&primeCount, 1, MPI_INT, 0, 1, MPI_COMM_WORLD); //Providing number of prime numbers returned to master
            MPI_Send(primes, primeCount, MPI_INT, 0, 2, MPI_COMM_WORLD); //Sending prime numbers to master

            //Getting new data from master
            MPI_Irecv(numsToTest, BATCH_SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, &recvRequest);
        }

        //Checking if the slave needs to be killed
        int killTest;
        MPI_Status killStatus;
        MPI_Test(&killRequest, &killTest, &killStatus);
        if(killTest)
            primesFound = true;

    }while(!primesFound);

    printf("Slave %d killed\n",pRank);
    fflush(NULL);
}

void generateBatch(int* buffer, char* seen)
{
    int rndNum;
    int generated = 0;
    while(generated != BATCH_SIZE)
    {
        rndNum = rand();
        if(seen[rndNum])
            continue;
        buffer[generated++] = rndNum;
        seen[rndNum] = true;
    }
}

char isPrime(int n)
{
    // Corner cases
   if (n <= 1 || n == 4)  return false;
   if (n <= 3) return true;
 
   // Try k times
   int k = K;
   while (k > 0)
   {
       // Pick a random number in [2..n-2]       
       // Above corner cases make sure that n > 4
       int a = 2 + rand()%(n-4); 
 
       // Checking if a and n are co-prime
       if (gcd(n, a) != 1)
          return false;
  
       // Fermat's little theorem
       if (power(a, n-1, n) != 1)
          return false;
 
       k--;
    }
 
    return true;
}

int gcd(int a, int b)
{
    if(a < b)
        return gcd(b, a);
    else if(a%b == 0)
        return b;
    else return gcd(b, a%b); 
}

int power(int a, unsigned int n, int p)
{
    int res = 1;      // Initialize result
    a = a % p;  // Update 'a' if 'a' >= p
 
    while (n > 0)
    {
        // If n is odd, multiply 'a' with result
        if (n & 1)
            res = (res*a) % p;
 
        // n must be even now
        n = n>>1; // n = n/2
        a = (a*a) % p;
    }
    return res;
}

/*
//Receiving the input batch from the master
        MPI_Recv(numsToTest, BATCH_SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD,&recvStatus);

        //Checking the inputs for primality
        primeCount = 0;
        for(int i = 0; i < BATCH_SIZE; ++i)
        {
            if(isPrime(numsToTest[i]))
                primes[primeCount++] = numsToTest[i];
        }

        //Returning primes to master
        if(primeCount)
        {
            //Providing number of prime numbers returned to master
            MPI_Send(&primeCount, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);

            //Sending prime numbers to master
            MPI_Send(primes, primeCount, MPI_INT, 0, 2, MPI_COMM_WORLD);
            printf("Test");
            fflush(NULL);
        }
*/