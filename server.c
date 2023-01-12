#include <stdio.h> 
#include <fcntl.h>      
#include <unistd.h>     
#include <sys/socket.h>
#include <string.h>  
#include <stdlib.h> 
#include <sys/types.h> 
#include <stdbool.h>  
#include <string.h>
#include <stdlib.h> 
#include <sys/ipc.h>
#include <sys/stat.h>  
#include <errno.h> 
#include <netinet/ip.h> 
#include <time.h>
#include <sys/sem.h>
#include "./constants.h"

//=======================================================================================================================================================================================


#define MAX_TRANSACTIONS 10
#define ADMIN_LOGIN_ID "Deepak"
#define ADMIN_PASSWORD "deepak" 
#define ACCOUNT_RECORD
#define CUSTOMER_RECORD
#define TRANSACTIONS

struct Account
{
    int accountNumber;     
    int owners[2];         
    bool isRegularAccount;                                                  // 1 -> Regular account, 0 -> Joint account
    bool active;                                                            // 1 -> Active, 0 -> Deactivated (Deleted)
    long int balance;     
    int transactions[MAX_TRANSACTIONS];                                     // A list of transaction IDs. Used to look up the transactions. // -1 indicates unused space in array
};

struct Customer
{
    int id;                                                                  // 0, 1, 2 ....
    char name[25];
    char gender;                                                             // M -> Male, F -> Female, O -> Other
    int age;
    
    char login[30];                                                          // Format : name-id (name will the first word in the structure member `name`)
    char password[30];
  
    int account;                                                             // Account number of the account the customer owns
};

struct Transaction
{
    int transactionID;                                                       // 0, 1, 2, 3 ...
    int accountNumber;
    bool operation;                                                         // 0 -> Withdraw, 1 -> Deposit
    long int oldBalance;
    long int newBalance;
    time_t transactionTime;
};

struct Customer loggedInCustomer;
int sem_iden;



//=========================================================================================================================================================================================


void connection_handler(int connFD); 
bool admin_handler(int connFD);
bool add_account(int connFD);
int add_customer(int connFD, bool isPrimary, int newAccountNumber);
bool delete_account(int connFD);
bool modify_customer_info(int connFD);
bool login_handler(bool isAdmin, int connect_FD, struct Customer *ptrToCustomer);
bool get_account_details(int connect_FD, struct Account *customerAccount);
bool get_customer_details(int connect_FD, int customer_ID);
bool customer_operation_handler(int connFD);
bool deposit(int connFD);
bool withdraw(int connFD);
bool get_balance(int connFD);
bool change_password(int connFD);
void write_transaction_to_array(int *transactionArray, int ID);
int write_transaction_to_file(int accountNumber, long int oldBalance, long int newBalance, bool operation);
bool lock_critical_section(struct sembuf *semOp);
bool unlock_critical_section(struct sembuf *sem_op);
bool get_transaction_details(int connFD, int accountNumber);



//==============================================================================================================================================================================================

void main()
{
    int socketFD, bindStatus, listenStatus, connectFD;
    struct sockaddr_in serverAddress, clientAddress;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD == -1)
    {
        perror("Error while creating server socket!");
        _exit(0);
    }

    serverAddress.sin_family = AF_INET;                // IPv4
    serverAddress.sin_port = htons(8081);              // Server will listen to port 8080
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // Binds the socket to all interfaces

    bindStatus = bind(socketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if (bindStatus == -1)
    {
        perror("Error while binding to server socket!");
        _exit(0);
    }

    listenStatus = listen(socketFD, 10);
    if (listenStatus == -1)
    {
        perror("Error while listening for connections on the server socket!");
        close(socketFD);
        _exit(0);
    }

    int clientSize;
    while (1)
    {
        clientSize = (int)sizeof(clientAddress);
        connectFD = accept(socketFD, (struct sockaddr *)&clientAddress, &clientSize);
        if (connectFD == -1)
        {
            perror("Error while connecting to client!");
            close(socketFD);
        }
        else
        {
            if (!fork())
            {
                // Child will enter this branch
                connection_handler(connectFD);
                close(connectFD);
                _exit(0);
            }
        }
    }

    close(socketFD);
}

void connection_handler(int connectFD)
{
    printf("Client has connected to the server!\n");

    char readBuffer[1000], writeBuffer[1000];
    ssize_t readBytes, writeBytes;
    

    writeBytes = write(connectFD, INITIAL_PROMPT, strlen(INITIAL_PROMPT));      //sends initial prompt to client and wait for the client input 
    if (writeBytes == -1)
        perror("Error while sending first prompt to the user!");
    else
    {
        bzero(readBuffer, sizeof(readBuffer));
        readBytes = read(connectFD, readBuffer, sizeof(readBuffer));            //connectionFileDes stores the client input- (admin or customer)
        if (readBytes == -1)
            perror("Error while reading from client");
        else if (readBytes == 0)
            printf("No data was sent by the client");
        else
        {
            int userChoice;
            userChoice = atoi(readBuffer);
            switch (userChoice)
            {
            case 1:
                // Admin
                admin_handler(connectFD);
                break;
            case 2:
                // Customer
                customer_operation_handler(connectFD);
                break;
            default:
                // Exit
                break;
            }
        }
    }
    printf("Terminating connection to client!\n");
}


//==================================================================================   ADMIN  =======================================================================================================

bool admin_handler(int connFD)
{

    if (login_handler(true, connFD, NULL))
    {
        ssize_t writeBytes, readBytes;            // Number of bytes read from / written to the client
        char readBuffer[1000], writeBuffer[1000]; // A buffer used for reading & writing to the client
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ADMIN_LOGIN_SUCCESS);
        while (1)
        {
            strcat(writeBuffer, "\n");
            strcat(writeBuffer, ADMIN_MENU);
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));            // client sees ADMIN_LOGIN_SUCCESS ---\n---ADMIN_MENU
            if (writeBytes == -1)
            {
                perror("Error while writing ADMIN_MENU to client!");
                return false;
            }
            bzero(writeBuffer, sizeof(writeBuffer));

            readBytes = read(connFD, readBuffer, sizeof(readBuffer));               // client enter the choice
            if (readBytes == -1)
            {
                perror("Error while reading client's choice for ADMIN_MENU");
                return false;
            }

            int choice = atoi(readBuffer);
            switch (choice)
            {
            case 1:
                get_customer_details(connFD, -1);
                break;
            case 2:
                get_account_details(connFD, NULL);
                break;
            case 3: 
                get_transaction_details(connFD, -1);
                break;
            case 4:
                add_account(connFD);
                break;
            case 5:
                delete_account(connFD);
                break;
            case 6:
                modify_customer_info(connFD);
                break;
            default:
                writeBytes = write(connFD, ADMIN_LOGOUT, strlen(ADMIN_LOGOUT));
                return false;
            }
        }
    }
    else
    {
        // ADMIN LOGIN FAILED
        return false;
    }
    return true;
}


// =============================================================================== GET CUSTOMER DETAIL =============================================================================================


bool get_customer_details(int connect_FD, int customer_ID)
{
    ssize_t r_bytes, w_bytes;                                                                                          // Number of bytes read from / written to the socket
    char r_buff[1000], w_buff[10000];                                                                              // A buffer for reading from / writing to the socket
    char temp_buff[1000];

    struct Customer customer;
    int customer_FD;
    struct flock lock = {F_RDLCK, SEEK_SET, 0, sizeof(struct Account), getpid()};                                           //

    if (customer_ID == -1)                                                                                                   // for admin
    {
        w_bytes = write(connect_FD, GET_CUSTOMER_ID, strlen(GET_CUSTOMER_ID));                                               //server asking for customer id
        if (w_bytes == -1)
        {
            perror("Error while writing GET_CUSTOMER_ID message to client!");
            return false;
        }

        bzero(r_buff, sizeof(r_buff));
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff));                                                           // server read the customer id
        if (r_bytes == -1)
        {
            perror("Error getting customer ID from client!");
            ;
            return false;
        }

        customer_ID = atoi(r_buff);
    }

    customer_FD = open(CUSTOMER_FILE, O_RDONLY);                                                                 // open the customer file
    if (customer_FD == -1)
    {
                                                                                                                           // Customer File doesn't exist
        bzero(w_buff, sizeof(w_buff));
        strcpy(w_buff, CUSTOMER_ID_DOESNT_EXIT);
        strcat(w_buff, "^");
        w_bytes = write(connect_FD, w_buff, strlen(w_buff));
        if (w_bytes == -1)
        {
            perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
        return false;
    }
    int offset = lseek(customer_FD, customer_ID * sizeof(struct Customer), SEEK_SET);                            // move to the particular record
    if (errno == EINVAL)
    {
                                                                                                                           // Customer record doesn't exist
        bzero(w_buff, sizeof(w_buff));
        strcpy(w_buff, CUSTOMER_ID_DOESNT_EXIT);
        strcat(w_buff, "^");
        w_bytes = write(connect_FD, w_buff, strlen(w_buff));
        if (w_bytes == -1)
        {
            perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required customer record!");
        return false;
    }
    lock.l_start = offset;

    int lock_status = fcntl(customer_FD, F_SETLKW, &lock);                                                    // lock the record 
    if (lock_status == -1)
    {
        perror("Error while obtaining read lock on the Customer file!");
        return false;
    }

    r_bytes = read(customer_FD, &customer, sizeof(struct Customer));                                           // read the particular record
    if (r_bytes == -1)
    {
        perror("Error reading customer record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;
    fcntl(customer_FD, F_SETLK, &lock);                                                                          // unlock the reccord

    bzero(w_buff, sizeof(w_buff));
    sprintf(w_buff, "Customer Details - \n\tID : %d\n\tName : %s\n\tGender : %c\n\tAge: %d\n\tAccount Number : %d\n\tLoginID : %s", customer.id, customer.name, customer.gender, customer.age, customer.account, customer.login);

    strcat(w_buff, "\n\nYou'll now be redirected to the main menu...^");

    w_bytes = write(connect_FD, w_buff, strlen(w_buff));                                                           // display the customer detail
    if (w_bytes == -1)
    {
        perror("Error writing customer info to client!");
        return false;
    }

    r_bytes = read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
    return true;
}


//================================================================================ GET  ACCOUNT DETAIL  ==============================================================================================


bool get_account_details(int connect_FD, struct Account *customerAccount)
{
    ssize_t r_bytes, w_bytes;                                                                                       // Number of bytes read from / written to the socket
    char r_buff[1000], w_buff[1000];                                                                            // A buffer for reading from / writing to the socket
    char temp_buff[1000];

    int accountNumber;
    struct Account account;
    int account_FD;

    if (customerAccount == NULL)                                                                                         // in case of admin login
    {

        w_bytes = write(connect_FD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));                                      // server asking for account number 
        if (w_bytes == -1)
        {
            perror("Error writing GET_ACCOUNT_NUMBER message to client!");
            return false;
        }

        bzero(r_buff, sizeof(r_buff));
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff));                                                        // server read the clients response
        if (r_bytes == -1)
        {
            perror("Error reading account number response from client!");
            return false;
        }

        accountNumber = atoi(r_buff);                                                                                // argument string to integer
    }
    else
        accountNumber = customerAccount->accountNumber;                                                                  // in case of customer login

    account_FD = open(ACCOUNT_FILE, O_RDONLY);                                                                // open the account file  
    if (account_FD == -1) 
    {
                                                                                                                         // Account record doesn't exist
        bzero(w_buff, sizeof(w_buff));
        strcpy(w_buff, ACCOUNT_ID_DOESNT_EXIT);
        strcat(w_buff, "^");
        perror("Error opening account file in get_account_details!");
        w_bytes = write(connect_FD, w_buff, strlen(w_buff));
        if (w_bytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
        return false;
    }

    int offset = lseek(account_FD, accountNumber * sizeof(struct Account), SEEK_SET);                           // move to the particular record 
    if (offset == -1 && errno == EINVAL)
    {
                                                                                                                           // Account record doesn't exist
        bzero(w_buff, sizeof(w_buff));
        strcpy(w_buff, ACCOUNT_ID_DOESNT_EXIT);
        strcat(w_buff, "^");
        perror("Error seeking to account record in get_account_details!");
        w_bytes = write(connect_FD, w_buff, strlen(w_buff));
        if (w_bytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required account record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};                                      // read lock on record 

    int lock_status = fcntl(account_FD, F_SETLKW, &lock);                                                      
    if (lock_status == -1)
    {
        perror("Error obtaining read lock on account record!");
        return false;
    }

    r_bytes = read(account_FD, &account, sizeof(struct Account));                                              // read the account detail or record
    if (r_bytes == -1)
    {
        perror("Error reading account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;       
    fcntl(account_FD, F_SETLK, &lock);                                                                           // unlock the record

    if (customerAccount != NULL)
    {
        *customerAccount = account;
        return true;
    }
      															    // store the entire detail
    bzero(w_buff, sizeof(w_buff));
    sprintf(w_buff, "Account Details - \n\tAccount Number : %d\n\tAccount Type : %s\n\tAccount Status : %s", account.accountNumber, (account.isRegularAccount ? "Regular" : "Joint"), (account.active) ? "Active" : "Deactived");
    if (account.active)
    {
        sprintf(temp_buff, "\n\tAccount Balance:₹ %ld", account.balance);
        strcat(w_buff, temp_buff);
    }

    sprintf(temp_buff, "\n\tPrimary Owner ID: %d", account.owners[0]);
    strcat(w_buff, temp_buff);
    if (account.owners[1] != -1)
    {
        sprintf(temp_buff, "\n\tSecondary Owner ID: %d", account.owners[1]);
        strcat(w_buff, temp_buff);
    }

    strcat(w_buff, "\n^");

    w_bytes = write(connect_FD, w_buff, strlen(w_buff));                                                          // display the detail to client
    r_bytes = read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read

    return true;
}


//====================================================================        GET TRANSACTION DETAIL        ==========================================================================================



bool get_transaction_details(int connect_FD, int accountNumber)
{

    ssize_t r_bytes, w_bytes;                                                                                               // Number of bytes read from / written to the socket
    char r_buff[1000], w_buff[10000], temp_buff[1000];                                                                // A buffer for reading from / writing to the socket

    struct Account account;

    if (accountNumber == -1)                                                                                                    // for admin
    {
        // Get the accountNumber
        w_bytes = write(connect_FD, GET_ACCOUNT_NUMBER, strlen(GET_ACCOUNT_NUMBER));                                              // server asking for account number 
        if (w_bytes == -1)
        {
            perror("Error writing GET_ACCOUNT_NUMBER message to client!");
            return false;
        }

        bzero(r_buff, sizeof(r_buff));
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff));                                                                // read the account number
        if (r_bytes == -1)
        {
            perror("Error reading account number response from client!");
            return false;
        }

        account.accountNumber = atoi(r_buff);                                                                                //  store the account number
    }
    else
        account.accountNumber = accountNumber;

    if (get_account_details(connect_FD, &account))                                                                                   // display account detail 
    {
        int iter;

        struct Transaction transaction;
        struct tm transactionTime;

        bzero(w_buff, sizeof(r_buff));

        int transactionFileDescriptor = open(TRANSACTION_FILE, O_RDONLY);                                                       // open the transaction file in read only mode 
        if (transactionFileDescriptor == -1)
        {
            perror("Error while opening transaction file!");
            write(connect_FD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
            return false;
        }

        for (iter = 0; iter < MAX_TRANSACTIONS && account.transactions[iter] != -1; iter++)                                    // for all the transaction
        {

            int offset = lseek(transactionFileDescriptor, account.transactions[iter] * sizeof(struct Transaction), SEEK_SET);   // move to the required record
            if (offset == -1)
            {
                perror("Error while seeking to required transaction record!");
                return false;
            }

            struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Transaction), getpid()};                               // apply read lock on record

            int lock_status = fcntl(transactionFileDescriptor, F_SETLKW, &lock);
            if (lock_status == -1)
            {
                perror("Error obtaining read lock on transaction record!");
                return false;
            }

            r_bytes = read(transactionFileDescriptor, &transaction, sizeof(struct Transaction));                               // read the transaction
            if (r_bytes == -1)
            {
                perror("Error reading transaction record from file!");
                return false;
            }

            lock.l_type = F_UNLCK;                                              
            fcntl(transactionFileDescriptor, F_SETLK, &lock);                                                                    // unlock the record

            transactionTime = *localtime(&(transaction.transactionTime));

            bzero(temp_buff, sizeof(temp_buff));                                                                               // print the transaction detail
            sprintf(temp_buff, "Details of transaction %d - \n\t Date : %d:%d %d/%d/%d \n\t Operation : %s \n\t Balance - \n\t\t Before : %ld \n\t\t After : %ld \n\t\t Difference : %ld\n", (iter + 1), transactionTime.tm_hour, transactionTime.tm_min, transactionTime.tm_mday, transactionTime.tm_mon, transactionTime.tm_year, (transaction.operation ? "Deposit" : "Withdraw"), transaction.oldBalance, transaction.newBalance, (transaction.newBalance - transaction.oldBalance));

            if (strlen(w_buff) == 0)
                strcpy(w_buff, temp_buff);
            else
                strcat(w_buff, temp_buff);
        }

        close(transactionFileDescriptor);                                                                                         // close the transaction file

        if (strlen(w_buff) == 0)
        {
            write(connect_FD, TRANSACTIONS_NOT_FOUND, strlen(TRANSACTIONS_NOT_FOUND));
            read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
            return false;
        }
        else
        {
            strcat(w_buff, "^");
            w_bytes = write(connect_FD, w_buff, strlen(w_buff));
            read(connect_FD, r_buff, sizeof(r_buff)); // Dummy read
        }
    }
}

//-----------------------------------------------------------------------------add account-----------------------------------------------------------------------------------------

bool add_account(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Account newAccount, prevAccount;

    int accountFileDescriptor = open(ACCOUNT_FILE, O_RDONLY);
    if (accountFileDescriptor == -1 && errno == ENOENT)
    {
        // Account file was never created
        newAccount.accountNumber = 0;
    }
    else if (accountFileDescriptor == -1)
    {
        perror("Error while opening account file");
        return false;
    }
    else
    {
        int offset = lseek(accountFileDescriptor, -sizeof(struct Account), SEEK_END);            // fd point to last record       
        if (offset == -1)
        {
            perror("Error seeking to last Account record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};        //lock the account file
        int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1)
        {
            perror("Error obtaining read lock on Account record!");
            return false;
        }

        readBytes = read(accountFileDescriptor, &prevAccount, sizeof(struct Account));            // store the prev_account (last account) in  prevAccount
        if (readBytes == -1)
        {
            perror("Error while reading Account record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(accountFileDescriptor, F_SETLK, &lock);

        close(accountFileDescriptor);

        newAccount.accountNumber = prevAccount.accountNumber + 1;                                  // new account number = last account no. +1 
    }
    writeBytes = write(connFD, ADMIN_ADD_ACCOUNT_TYPE, strlen(ADMIN_ADD_ACCOUNT_TYPE));            // server asking for the regular or common account and wait till client enter the choice
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_ACCOUNT_TYPE message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, &readBuffer, sizeof(readBuffer));                                     // read the client choice in readBuffer
    if (readBytes == -1)
    {
        perror("Error reading account type response from client!");
        return false;
    }

    newAccount.isRegularAccount = atoi(readBuffer) == 1 ? true : false;                           // 1--regular account  ,2--common account

    newAccount.owners[0] = add_customer(connFD, true, newAccount.accountNumber);                  // add customer detail such  as name ,gender,age

    if (newAccount.isRegularAccount)
        newAccount.owners[1] = -1;
    else
        newAccount.owners[1] = add_customer(connFD, false, newAccount.accountNumber);             // if the account is common then add the detail for other customer

    newAccount.active = true;                                                                     // update account as active and balance =0
    newAccount.balance = 0;

    memset(newAccount.transactions, -1, MAX_TRANSACTIONS * sizeof(int));                           // create space for transaction field

    accountFileDescriptor = open(ACCOUNT_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);            // open the account file
    if (accountFileDescriptor == -1)
    {
        perror("Error while creating / opening account file!");
        return false;
    }

    writeBytes = write(accountFileDescriptor, &newAccount, sizeof(struct Account));                // update the new account entery into account file
    if (writeBytes == -1)
    {
        perror("Error while writing Account record to file!");
        return false;
    }

    close(accountFileDescriptor);                                                                  // clode the account file

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%d", ADMIN_ADD_ACCOUNT_NUMBER, newAccount.accountNumber);
    strcat(writeBuffer, "\nRedirecting you to the main menu ...^");
    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));
    readBytes = read(connFD, readBuffer, sizeof(read)); // Dummy read
    return true;
}

//=========================================== ADD CUSTOMER ======================================================================


int add_customer(int connFD, bool isPrimary, int newAccountNumber)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Customer newCustomer, previousCustomer;

    int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);                    // open customer file
    if (customerFileDescriptor == -1 && errno == ENOENT)
    {
        // Customer file was never created
        newCustomer.id = 0;
    }
    else if (customerFileDescriptor == -1)
    {
        perror("Error while opening customer file");
        return -1;
    }
    else
    {
        int offset = lseek(customerFileDescriptor, -sizeof(struct Customer), SEEK_END);          // fd point to last customer
        if (offset == -1)
        {
            perror("Error seeking to last Customer record!");
            return false;
        }

        struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};      //lock
        int lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
        if (lockingStatus == -1)
        {
            perror("Error obtaining read lock on Customer record!");
            return false;
        }

        readBytes = read(customerFileDescriptor, &previousCustomer, sizeof(struct Customer));     // store the last customer detail in previousCustomer
        if (readBytes == -1)
        {
            perror("Error while reading Customer record from file!");
            return false;
        }

        lock.l_type = F_UNLCK;
        fcntl(customerFileDescriptor, F_SETLK, &lock);

        close(customerFileDescriptor);

        newCustomer.id = previousCustomer.id + 1;                                                 // customer id = (last customer id number) +1
    }

    if (isPrimary)
        sprintf(writeBuffer, "%s%s", ADMIN_ADD_CUSTOMER_PRIMARY, ADMIN_ADD_CUSTOMER_NAME);
    else
        sprintf(writeBuffer, "%s%s", ADMIN_ADD_CUSTOMER_SECONDARY, ADMIN_ADD_CUSTOMER_NAME);

    writeBytes = write(connFD, writeBuffer, sizeof(writeBuffer));                                //server asking for name and wait for client to input
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_CUSTOMER_NAME message to client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer));                                    // store the lient input to readBuffer
    if (readBytes == -1)
    {
        perror("Error reading customer name response from client!");
        ;
        return false;
    }

    strcpy(newCustomer.name, readBuffer);                                                        // update custormer name

    writeBytes = write(connFD, ADMIN_ADD_CUSTOMER_GENDER, strlen(ADMIN_ADD_CUSTOMER_GENDER));    //server asking for gender and wait for client to input the gender
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_CUSTOMER_GENDER message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));                                     // read the input from the client
    if (readBytes == -1)
    {
        perror("Error reading customer gender response from client!");
        return false;
    }

    if (readBuffer[0] == 'M' || readBuffer[0] == 'F' || readBuffer[0] == 'O')                     // update customer gender
        newCustomer.gender = readBuffer[0];  
    else
    {
        writeBytes = write(connFD, ADMIN_ADD_CUSTOMER_WRONG_GENDER, strlen(ADMIN_ADD_CUSTOMER_WRONG_GENDER));      // otherwise  server send ADMIN_ADD_CUSTOMER_WRONG_GENDER and returns
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    bzero(writeBuffer, sizeof(writeBuffer));
    strcpy(writeBuffer, ADMIN_ADD_CUSTOMER_AGE);                                                  
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));                                //server asking for age
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_ADD_CUSTOMER_AGE message to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));
    if (readBytes == -1)
    {
        perror("Error reading customer age response from client!");
        return false;
    }

    int customerAge = atoi(readBuffer);
    if (customerAge == 0)
    {                                                                                        // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ERRON_INPUT_FOR_NUMBER);
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ERRON_INPUT_FOR_NUMBER message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    newCustomer.age = customerAge;                                                            //update the age

    newCustomer.account = newAccountNumber;                                                   // update the account number

    strcpy(newCustomer.login, newCustomer.name);                                              // for customer id------------
    strcat(newCustomer.login, "-");
    sprintf(writeBuffer, "%d", newCustomer.id);                                               // format for id -  "name-cus_id"
    strcat(newCustomer.login, writeBuffer);                                                   // store the id in customer id fieled

    char hashedPassword[1000];                                                                // for  customer passward
    strcpy(hashedPassword, AUTOGEN_PASSWORD);
    strcpy(newCustomer.password, hashedPassword);

    customerFileDescriptor = open(CUSTOMER_FILE, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);     // now open the customer file and append the newcustomer entery
    if (customerFileDescriptor == -1)
    {
        perror("Error while creating / opening customer file!");
        return false;
    }
    writeBytes = write(customerFileDescriptor, &newCustomer, sizeof(newCustomer));             // write the new customer entery
    if (writeBytes == -1)
    {
        perror("Error while writing Customer record to file!");
        return false;
    }

    close(customerFileDescriptor);                                                               //close the customer fill

    bzero(writeBuffer, sizeof(writeBuffer));
    sprintf(writeBuffer, "%s%s-%d\n%s%s", ADMIN_ADD_CUSTOMER_AUTOGEN_LOGIN, newCustomer.name, newCustomer.id, ADMIN_ADD_CUSTOMER_AUTOGEN_PASSWORD, AUTOGEN_PASSWORD);
    strcat(writeBuffer, "^");
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
    if (writeBytes == -1)
    {
        perror("Error sending customer loginID and password to the client!");
        return false;
    }

    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return newCustomer.id;
}

//-----------------------------------------------------------------------------delete account----------------------------------------------------------------------------------------

bool delete_account(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Account account;

    writeBytes = write(connFD, ADMIN_DEL_ACCOUNT_NO, strlen(ADMIN_DEL_ACCOUNT_NO));         //server sending promting to enter the "account number"  
    if (writeBytes == -1)
    {
        perror("Error writing ADMIN_DEL_ACCOUNT_NO to client!");
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));                                                  
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));                               // server read  the client response
    if (readBytes == -1)
    {
        perror("Error reading account number response from the client!");
        return false;
    }

    int accountNumber = atoi(readBuffer);                                                   //

    int accountFileDescriptor = open(ACCOUNT_FILE, O_RDONLY);                               // opening the account file
    if (accountFileDescriptor == -1)
    {
                                                                                            // if account record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }


    int offset = lseek(accountFileDescriptor, accountNumber * sizeof(struct Account), SEEK_SET);        // search for the account number  as  accountFileDescriptor is pointing to the particular recod
    if (errno == EINVAL)
    {
                                                                                                           // Customer record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ACCOUNT_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ACCOUNT_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required account record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};                      // read lock on account file
    int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Error obtaining read lock on Account record!");
        return false;
    }

    readBytes = read(accountFileDescriptor, &account, sizeof(struct Account));                               // read the account detail and store in account 
    if (readBytes == -1)
    {
        perror("Error while reading Account record from file!");
        return false;
    }

    lock.l_type = F_UNLCK;                                                                                    // unlock the file
    fcntl(accountFileDescriptor, F_SETLK, &lock);

    close(accountFileDescriptor);                                                                             //closing the account file 

    bzero(writeBuffer, sizeof(writeBuffer));
    if (account.balance == 0)                                                                                  // No money, hence can close account
    {
                                                                                                             
        account.active = false;                                                                                // setting status of account is inactive
        accountFileDescriptor = open(ACCOUNT_FILE, O_WRONLY);
        if (accountFileDescriptor == -1)
        {
            perror("Error opening Account file in write mode!");
            return false;
        }

        offset = lseek(accountFileDescriptor, accountNumber * sizeof(struct Account), SEEK_SET);                // find the particular record 
        if (offset == -1)
        {
            perror("Error seeking to the Account!");
            return false;
        }

        lock.l_type = F_WRLCK;                                                                                       
        lock.l_start = offset;

        int lockingStatus = fcntl(accountFileDescriptor, F_SETLKW, &lock);                                       // write lock on account file
        if (lockingStatus == -1)
        {
            perror("Error obtaining write lock on the Account file!");
            return false;
        }

        writeBytes = write(accountFileDescriptor, &account, sizeof(struct Account));                             // update the record or account as inactive
        if (writeBytes == -1)
        {
            perror("Error deleting account record!");
            return false;
        }

        lock.l_type = F_UNLCK;                                                                                  // release the write lock
        fcntl(accountFileDescriptor, F_SETLK, &lock);

        strcpy(writeBuffer, ADMIN_DEL_ACCOUNT_SUCCESS);                                                         // display "ADMIN_DEL_ACCOUNT_SUCCESS" to client
    }
    else
                                                                                                                // Account has some money ask customer to withdraw it
        strcpy(writeBuffer, ADMIN_DEL_ACCOUNT_FAILURE);
    writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));                                               // if account balance is not zero then display "ADMIN_DEL_ACCOUNT_FAILURE" to client
    if (writeBytes == -1)
    {
        perror("Error while writing final DEL message to client!");
        return false;
    }
    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return true;
}


//-----------------------------------------------------------------------------modify customer info-----------------------------------------------------------------------------------------

bool modify_customer_info(int connFD)
{
    ssize_t readBytes, writeBytes;
    char readBuffer[1000], writeBuffer[1000];

    struct Customer customer;

    int customerID;

    off_t offset;
    int lockingStatus;

    writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_ID, strlen(ADMIN_MOD_CUSTOMER_ID));          //server asking to enter the customer id 
    if (writeBytes == -1)
    {
        perror("Error while writing ADMIN_MOD_CUSTOMER_ID message to client!");
        return false;
    }
    bzero(readBuffer, sizeof(readBuffer));
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));                                 // server read the customer id
    if (readBytes == -1)
    {
        perror("Error while reading customer ID from client!");
        return false;
    }

    customerID = atoi(readBuffer);

    int customerFileDescriptor = open(CUSTOMER_FILE, O_RDONLY);                               // open the customer file
    if (customerFileDescriptor == -1)
    {
                                                                                              // Customer File doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, CUSTOMER_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    
    offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);    // fd pointing to particular record
    if (errno == EINVAL)
    {
        // Customer record doesn't exist
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, CUSTOMER_ID_DOESNT_EXIT);
        strcat(writeBuffer, "^");
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing CUSTOMER_ID_DOESNT_EXIT message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }
    else if (offset == -1)
    {
        perror("Error while seeking to required customer record!");
        return false;
    }

    struct flock lock = {F_RDLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};       //lock the customer file

    lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);
    if (lockingStatus == -1)
    {
        perror("Couldn't obtain lock on customer record!");
        return false;
    }

    readBytes = read(customerFileDescriptor, &customer, sizeof(struct Customer));              //read the customer file
    if (readBytes == -1)
    {
        perror("Error while reading customer record from the file!");
        return false;
    }

                                                                                               // Unlock the record
    lock.l_type = F_UNLCK;
    fcntl(customerFileDescriptor, F_SETLK, &lock);

    close(customerFileDescriptor);

    writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_MENU, strlen(ADMIN_MOD_CUSTOMER_MENU));       // server send "Which information would you like to modify" to client and for client response
    if (writeBytes == -1)
    {
        perror("Error while writing ADMIN_MOD_CUSTOMER_MENU message to client!");
        return false;
    }
    readBytes = read(connFD, readBuffer, sizeof(readBuffer));                                  //client response store in readBuffer tell which field to modify
    if (readBytes == -1)
    {
        perror("Error while getting customer modification menu choice from client!");
        return false;
    }

    int choice = atoi(readBuffer);
    if (choice == 0)
    {                                                                                          // A non-numeric string was passed to atoi
        bzero(writeBuffer, sizeof(writeBuffer));
        strcpy(writeBuffer, ERRON_INPUT_FOR_NUMBER);
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing ERRON_INPUT_FOR_NUMBER message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    bzero(readBuffer, sizeof(readBuffer));
    switch (choice)                                                                                                 //moving to particular field
    {
    case 1:
        writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_NEW_NAME, strlen(ADMIN_MOD_CUSTOMER_NEW_NAME));               // enter the new name
        if (writeBytes == -1)
        {
            perror("Error while writing ADMIN_MOD_CUSTOMER_NEW_NAME message to client!");
            return false;
        }
        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));                                                  //read the client response into buffer          
        if (readBytes == -1)
        {
            perror("Error while getting response for customer's new name from client!");
            return false;
        }
        strcpy(customer.name, readBuffer);                                                                          //update the name
        break;
    case 2:
        writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_NEW_AGE, strlen(ADMIN_MOD_CUSTOMER_NEW_AGE));                // enter the new age
        if (writeBytes == -1)
        {
            perror("Error while writing ADMIN_MOD_CUSTOMER_NEW_AGE message to client!");
            return false;
        }
        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));                                                  //read the client response into buffer  
        if (readBytes == -1)
        {
            perror("Error while getting response for customer's new age from client!");
            return false;
        }
        int updatedAge = atoi(readBuffer);
        if (updatedAge == 0)
        {
                                                                                                             // Either client has sent age as 0 (which is invalid) or has entered a non-numeric string
            bzero(writeBuffer, sizeof(writeBuffer));
            strcpy(writeBuffer, ERRON_INPUT_FOR_NUMBER);
            writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
            if (writeBytes == -1)
            {
                perror("Error while writing ERRON_INPUT_FOR_NUMBER message to client!");
                return false;
            }
            readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
            return false;
        }
        customer.age = updatedAge;                                                                                  //update the age
        break;
    case 3:
        writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_NEW_GENDER, strlen(ADMIN_MOD_CUSTOMER_NEW_GENDER));          // enter the new gender
        if (writeBytes == -1)
        {
            perror("Error while writing ADMIN_MOD_CUSTOMER_NEW_GENDER message to client!");
            return false;
        }
        readBytes = read(connFD, &readBuffer, sizeof(readBuffer));                                                 //read the client response into buffer
        if (readBytes == -1)
        {
            perror("Error while getting response for customer's new gender from client!");
            return false;
        }
        customer.gender = readBuffer[0];                                                                           //update the gender
        break;
    default:                                                         
        bzero(writeBuffer, sizeof(writeBuffer));                                                                   //for invalid choice
        strcpy(writeBuffer, INVALID_MENU_CHOICE);
        writeBytes = write(connFD, writeBuffer, strlen(writeBuffer));
        if (writeBytes == -1)
        {
            perror("Error while writing INVALID_MENU_CHOICE message to client!");
            return false;
        }
        readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read
        return false;
    }

    customerFileDescriptor = open(CUSTOMER_FILE, O_WRONLY);                                                         //open the customer file
    if (customerFileDescriptor == -1)
    {
        perror("Error while opening customer file");
        return false;
    }
    offset = lseek(customerFileDescriptor, customerID * sizeof(struct Customer), SEEK_SET);                         //move to particular record
    if (offset == -1)
    {
        perror("Error while seeking to required customer record!");
        return false;
    }

    lock.l_type = F_WRLCK;
    lock.l_start = offset;
    lockingStatus = fcntl(customerFileDescriptor, F_SETLKW, &lock);                                                 //wite lock
    if (lockingStatus == -1)
    {
        perror("Error while obtaining write lock on customer record!");
        return false;
    }

    writeBytes = write(customerFileDescriptor, &customer, sizeof(struct Customer));                                 //write the updated customer info
    if (writeBytes == -1)
    {
        perror("Error while writing update customer info into file");
    }

    lock.l_type = F_UNLCK;                                                                                          //unlock
    fcntl(customerFileDescriptor, F_SETLKW, &lock);

    close(customerFileDescriptor);                                                                                  //close file

    writeBytes = write(connFD, ADMIN_MOD_CUSTOMER_SUCCESS, strlen(ADMIN_MOD_CUSTOMER_SUCCESS));                     //sending "modification was successfully made" to client
    if (writeBytes == -1)
    {
        perror("Error while writing ADMIN_MOD_CUSTOMER_SUCCESS message to client!");
        return false;
    }
    readBytes = read(connFD, readBuffer, sizeof(readBuffer)); // Dummy read

    return true;
}


//====================================================  login  =================================================================

bool login_handler(bool isAdmin, int connect_FD, struct Customer *ptrToCustomer_ID)
{
    ssize_t r_bytes, w_bytes;                                                                                           // Number of bytes written to / read from the socket
    char r_buff[1000], w_buff[1000];                                                                               // Buffer for reading from / writing to the client
    char temp_buff[1000];
    struct Customer customer;

    int ID;

    bzero(r_buff, sizeof(r_buff));
    bzero(w_buff, sizeof(w_buff));

                                                                                                                         // Get login message for respective user type
    if (isAdmin)
        strcpy(w_buff, ADMIN_LOGIN_WELCOME);
    else
        strcpy(w_buff, CUSTOMER_LOGIN_WELCOME);

                                                                                                                         // Append the request for LOGIN ID message
    strcat(w_buff, "\n");
    strcat(w_buff, LOGIN_ID);

    w_bytes = write(connect_FD, w_buff, strlen(w_buff));      //client sees ADMIN_LOGIN_WELCOME or CUSTOMER_LOGIN_WELCOME-\n- LOGIN ID in screen and server wait for client respond( login id) 
    if (w_bytes == -1)
    {
        perror("Error writing WELCOME & LOGIN_ID message to the client!");
        return false;
    }

    r_bytes = read(connect_FD, r_buff, sizeof(r_buff));                                                               //server read the login id from connect_FD which is written by client on connect_FD 
    if (r_bytes == -1)
    {
        perror("Error reading login ID from client!");
        return false;
    }

    bool user_exist = false;

    if (isAdmin)
    {
        if (strcmp(r_buff, ADMIN_LOGIN_ID) == 0)                                                                       // For admin only-------comparing the admin login id with clients input 
            user_exist = true;
    }
    else                                                                                                                   // for customer
    {
        bzero(temp_buff, sizeof(temp_buff));                             
        strcpy(temp_buff, r_buff);
        strtok(temp_buff, "-");
        ID = atoi(strtok(NULL, "-"));

        int customerFileFD = open(CUSTOMER_FILE, O_RDONLY);                                                                //open the customer file
        if (customerFileFD == -1)
        {
            perror("Error opening customer file in read mode!");
            return false;
        }

        off_t offset = lseek(customerFileFD, ID * sizeof(struct Customer), SEEK_SET);                                      //move to particular record
        if (offset >= 0)
        {
            struct flock lock = {F_RDLCK, SEEK_SET, ID * sizeof(struct Customer), sizeof(struct Customer), getpid()};      // read lock

            int lock_status = fcntl(customerFileFD, F_SETLKW, &lock);
            if (lock_status == -1)
            {
                perror("Error obtaining read lock on customer record!");
                return false;
            }

            r_bytes = read(customerFileFD, &customer, sizeof(struct Customer));                                          // read the customer record
            if (r_bytes == -1)
            {
                ;
                perror("Error reading customer record from file!");
            }

            lock.l_type = F_UNLCK;
            fcntl(customerFileFD, F_SETLK, &lock);                                                                         //unlock the file

            if (strcmp(customer.login, r_buff) == 0)                                                                   //comparing the login id       
                user_exist = true;

            close(customerFileFD);                                                                                         //close the file
        }
        else
        {
            w_bytes = write(connect_FD, CUSTOMER_LOGIN_ID_DOESNT_EXIT, strlen(CUSTOMER_LOGIN_ID_DOESNT_EXIT));
        }
    }

    if (user_exist)                                                                                                         // common to admin and customer ----now checking for passward
    {
        bzero(w_buff, sizeof(w_buff));
        w_bytes = write(connect_FD, PASSWORD, strlen(PASSWORD));                                                  // server promts "passward" in client window and wait for client to enter the passward
        if (w_bytes == -1)
        {
            perror("Error writing PASSWORD message to client!");
            return false;
        }

        bzero(r_buff, sizeof(r_buff));
        r_bytes = read(connect_FD, r_buff, sizeof(r_buff));                                         // as soon as client enter the passward it save in connect_FD from that server read into r_buff
        if (r_bytes == 1)
        {
            perror("Error reading password from the client!");
            return false;
        }

        char hashedPassword[1000];
        strcpy(hashedPassword,r_buff);             

        if (isAdmin)
        {
            if (strcmp(hashedPassword, ADMIN_PASSWORD) == 0)                                                              // for admin---if passward match return true
                return true;
        }
        else
        {
            if (strcmp(hashedPassword, customer.password) == 0)                                                           // for client---if passward match return true
            {
                *ptrToCustomer_ID = customer;
                return true;
            }
        }

        bzero(w_buff, sizeof(w_buff));
        w_bytes = write(connect_FD, INVALID_PASSWORD, strlen(INVALID_PASSWORD));                                           // server send passward invalid to client 
    }
    else
    {
        bzero(w_buff, sizeof(w_buff));
        w_bytes = write(connect_FD, INVALID_LOGIN, strlen(INVALID_LOGIN));
    }

    return false;
}


// ========================================================================customer====================================================================================================================================================================================================================================


bool customer_operation_handler(int connFD)
{
    if (login_handler(false, connFD, &loggedInCustomer))
    {
        ssize_t w_bytes, r_bytes;                                                                                 // Number of bytes read from / written to the client
        char r_buff[1000], w_buff[1000];                                                                      // A buffer used for reading & writing to the client

                                                                                                                       // Get a semaphore for the user
        key_t semKey = ftok(CUSTOMER_FILE, loggedInCustomer.account);                            // Generate a key based on the account number hence, different customers will have different semaphores

        union semun
        {
            int val;                                                                                                  // Value of the semaphore
        } semSet;

        int semStatus;
        sem_iden = semget(semKey, 1, 0);                                                                        // Get the semaphore if it exists
        if (sem_iden == -1)
        {
            sem_iden = semget(semKey, 1, IPC_CREAT | 0700);                                                     // Create a new semaphore
            if (sem_iden == -1)
            {
                perror("Error while creating semaphore!");
                _exit(1);
            }

            semSet.val = 1;                                                                                          // Set a binary semaphore 
            semStatus = semctl(sem_iden, 0, SETVAL, semSet);
            if (semStatus == -1)
            {
                perror("Error while initializing a binary sempahore!");
                _exit(1);
            }
        }

        bzero(w_buff, sizeof(w_buff));
        strcpy(w_buff, CUSTOMER_LOGIN_SUCCESS);
        while (1)
        {
            strcat(w_buff, "\n");
            strcat(w_buff, CUSTOMER_MENU);
            w_bytes = write(connFD, w_buff, strlen(w_buff));                                    //server send the menu to client and wait for the client response
            if (w_bytes == -1)
            {
                perror("Error while writing CUSTOMER_MENU to client!");
                return false;
            }
            bzero(w_buff, sizeof(w_buff));

            bzero(r_buff, sizeof(r_buff));
            r_bytes = read(connFD, r_buff, sizeof(r_buff));                                         //server read the client response 
            if (r_bytes == -1)
            {
                perror("Error while reading client's choice for CUSTOMER_MENU");
                return false;
            }
            
            // printf("READ BUFFER : %s\n", r_buff);
            int choice = atoi(r_buff);
            // printf("CHOICE : %d\n", choice);
            switch (choice)                                                                                  // based on the choice ,server call the appropriate function
            {
            case 1:
                get_customer_details(connFD, loggedInCustomer.id);
                break;
            case 2:
                deposit(connFD);
                break;
            case 3:
                withdraw(connFD);
                break;
            case 4:
                get_balance(connFD);
                break;
            case 5:
                get_transaction_details(connFD, loggedInCustomer.account);
                break;
            case 6:
                change_password(connFD);
                break;
            default:
                w_bytes = write(connFD, CUSTOMER_LOGOUT, strlen(CUSTOMER_LOGOUT));
                return false;
            }
        }
    }
    else
    {
                                                                                                                                      // CUSTOMER LOGIN FAILED
        return false;
    }
    return true;
}


//==================================================================================  GET BALANCE ====================================================================================================

bool get_balance(int connFD)
{
    char buffer[1000];
    struct Account account;
    account.accountNumber = loggedInCustomer.account;                                                                                   // current login account
    if (get_account_details(connFD, &account))
    {
        bzero(buffer, sizeof(buffer));
        if (account.active)
        {
            sprintf(buffer, "You have ₹ %ld!^", account.balance);     
            write(connFD, buffer, strlen(buffer));                                                                                     // display the current balance 
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));                                                           // if account is deactivated it shows message account deactivated
        read(connFD, buffer, sizeof(buffer)); // Dummy read        
    }
    else
    {
        // ERROR while getting balance
        return false;
    }
}

//=============================================================================          DEPOSITE       ================================================================================================

bool deposit(int connFD)
{
    char r_buff[1000], w_buff[1000];                        	
    ssize_t r_bytes, w_bytes;
    struct Account account;
    struct sembuf semOp;
    

    account.accountNumber = loggedInCustomer.account;
    long int deposit_amount = 0;
                                                                                                                                      // Lock the critical section
    lock_critical_section(&semOp);

    if (get_account_details(connFD, &account))
    {
        
        if (account.active)
        {

            w_bytes = write(connFD, DEPOSIT_AMOUNT, strlen(DEPOSIT_AMOUNT));                                                      // server send 'DEPOSIT_AMOUNT' to client
            if (w_bytes == -1)
            {
                perror("Error writing DEPOSIT_AMOUNT to client!");
                unlock_critical_section(&semOp);
                return false;
            }

            bzero(r_buff, sizeof(r_buff));    
            r_bytes = read(connFD, r_buff, sizeof(r_buff));                                                               // read the response from the client
            if (r_bytes == -1)
            {
                perror("Error reading deposit money from client!");
                unlock_critical_section(&semOp);
                return false;
            }

            deposit_amount = atol(r_buff);                                                                                                            
            if (deposit_amount != 0)
            {

                int newTransactionID = write_transaction_to_file(account.accountNumber, account.balance, account.balance + deposit_amount, 1);
                write_transaction_to_array(account.transactions, newTransactionID);

                account.balance += deposit_amount;

                int accountFD = open(ACCOUNT_FILE, O_WRONLY);                                                           // open the account file for update the balance 
                off_t offset = lseek(accountFD, account.accountNumber * sizeof(struct Account), SEEK_SET);              // move to particular account

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};
                int lockStatus = fcntl(accountFD, F_SETLKW, &lock);                                                  // lock the file
                if (lockStatus == -1)
                {
                    perror("Error obtaining write lock on account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                w_bytes = write(accountFD, &account, sizeof(struct Account));                                        // update the record
                if (w_bytes == -1)
                {
                    perror("Error storing updated deposit money in account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;                                                                                              
                fcntl(accountFD, F_SETLK, &lock);                                                                      

                write(connFD, DEPOSIT_AMOUNT_SUCCESS, strlen(DEPOSIT_AMOUNT_SUCCESS));                                              // display the desposit account success to client
                read(connFD, r_buff, sizeof(r_buff)); // Dummy read

                get_balance(connFD);                                                                                                // call get balance 

                unlock_critical_section(&semOp);

                return true;
            }
            else
                w_bytes = write(connFD, DEPOSIT_AMOUNT_INVALID, strlen(DEPOSIT_AMOUNT_INVALID));
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));
        read(connFD, r_buff, sizeof(r_buff)); // Dummy read

        unlock_critical_section(&semOp);
    }
    else
    {
        // FAIL
        unlock_critical_section(&semOp);
        return false;
    }
}


//===================================================================================    WITHDRAW    ==============================================================================================

bool withdraw(int connFD)
{
    char r_buff[1000], w_buff[1000];
    ssize_t r_bytes, w_bytes;
    struct sembuf semOp;
    struct Account account;
    
    account.accountNumber = loggedInCustomer.account;                                                                   // current logged in customer account
    long int withdraw_amount = 0;

                                                                                                                        // Lock the critical section
    
    lock_critical_section(&semOp);

    if (get_account_details(connFD, &account))                                                                          
    {
        if (account.active)
        {

            w_bytes = write(connFD, WITHDRAW_AMOUNT, strlen(WITHDRAW_AMOUNT));                                        // server asking for withdraw amount
            if (w_bytes == -1)
            {
                perror("Error writing WITHDRAW_AMOUNT message to client!");
                unlock_critical_section(&semOp);
                return false;
            }

            bzero(r_buff, sizeof(r_buff));
            r_bytes = read(connFD, r_buff, sizeof(r_buff));                                                    // server read the withdraw acount
            if (r_bytes == -1)
            {
                perror("Error reading withdraw amount from client!");
                unlock_critical_section(&semOp);
                return false;
            }

            withdraw_amount = atol(r_buff);

            if (withdraw_amount != 0 && account.balance - withdraw_amount >= 0)                                            // for valid amount
            {

                int newTransactionID = write_transaction_to_file(account.accountNumber, account.balance, account.balance - withdraw_amount, 0);   // add transaction in transaction file
                write_transaction_to_array(account.transactions, newTransactionID);

                account.balance -= withdraw_amount;                                                                       // update the balance 

                int accountFD = open(ACCOUNT_FILE, O_WRONLY);                                                 // open the account file for updation
                off_t offset = lseek(accountFD, account.accountNumber * sizeof(struct Account), SEEK_SET);    // move to particular record

                struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Account), getpid()};                        // write lock on record
                int lockStatus = fcntl(accountFD, F_SETLKW, &lock);
                if (lockStatus == -1)
                {
                    perror("Error obtaining write lock on account record!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                w_bytes = write(accountFD, &account, sizeof(struct Account));                              // update the record in account file
                if (w_bytes == -1)
                {
                    perror("Error writing updated balance into account file!");
                    unlock_critical_section(&semOp);
                    return false;
                }

                lock.l_type = F_UNLCK;
                fcntl(accountFD, F_SETLK, &lock);                                                             // unlock the record

                write(connFD, WITHDRAW_AMOUNT_SUCCESS, strlen(WITHDRAW_AMOUNT_SUCCESS));                                  // display the success message 
                read(connFD, r_buff, sizeof(r_buff)); // Dummy read

                get_balance(connFD);                                                                                      // display the available balance 

                unlock_critical_section(&semOp);

                return true;
            }
            else
                w_bytes = write(connFD, WITHDRAW_AMOUNT_INVALID, strlen(WITHDRAW_AMOUNT_INVALID));                     // for invalid amount
        }
        else
            write(connFD, ACCOUNT_DEACTIVATED, strlen(ACCOUNT_DEACTIVATED));                                              // if the account is deactivated
        read(connFD, r_buff, sizeof(r_buff)); // Dummy read

        unlock_critical_section(&semOp);                                                                                  // unlock the semaphore
    }
    else
    {
                                                                                                                          // FAILURE while getting account information
        unlock_critical_section(&semOp);
        return false;
    }
}


// =========================================================================  CHANGE PASSWARD   ======================================================================================================

bool change_password(int connFD)
{
    ssize_t r_bytes, w_bytes;
    char r_buff[1000], w_buff[1000], new_pass[1000],hashedPassword[1000];
                                                                                                                                   
    struct sembuf semOp = {0, -1, SEM_UNDO};
    int semopStatus = semop(sem_iden, &semOp, 1);                                                                                    // Lock the critical section
    if (semopStatus == -1)
    {
        perror("Error while locking critical section");
        return false;
    }

    w_bytes = write(connFD, PASSWORD_CHANGE_OLD_PASS, strlen(PASSWORD_CHANGE_OLD_PASS));                                            // server asking for old passward
    if (w_bytes == -1)
    {
        perror("Error writing PASSWORD_CHANGE_OLD_PASS message to client!");
        unlock_critical_section(&semOp);
        return false;
    }

    bzero(r_buff, sizeof(r_buff));
    r_bytes = read(connFD, r_buff, sizeof(r_buff));                                                                                  //server read the old passward
    if (r_bytes == -1)
    {
        perror("Error reading old password response from client");
        unlock_critical_section(&semOp);
        return false;
    }

    if (strcmp(r_buff, loggedInCustomer.password) == 0)                                                             // comapre the login passward with read passward
    {																       // if password matches
                                                                                                                                     //  server asking for the new passward
        w_bytes = write(connFD, PASSWORD_CHANGE_NEW_PASS, strlen(PASSWORD_CHANGE_NEW_PASS));
        if (w_bytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS message to client!");
            unlock_critical_section(&semOp);
            return false;
        }
        bzero(r_buff, sizeof(r_buff));
        r_bytes = read(connFD, r_buff, sizeof(r_buff));                                                                       // server read the new passward
        if (r_bytes == -1)
        {
            perror("Error reading new password response from client");
            unlock_critical_section(&semOp);
            return false;
        }

        strcpy(new_pass, r_buff);                                                                                

        w_bytes = write(connFD, PASSWORD_CHANGE_NEW_PASS_RE, strlen(PASSWORD_CHANGE_NEW_PASS_RE));                                   // re-enter the passward 
        if (w_bytes == -1)
        {
            perror("Error writing PASSWORD_CHANGE_NEW_PASS_RE message to client!");
            unlock_critical_section(&semOp);
            return false;
        }
        bzero(r_buff, sizeof(r_buff));
        r_bytes = read(connFD, r_buff, sizeof(r_buff));                                                                        // read the re-entered passward
        if (r_bytes == -1)
        {
            perror("Error reading new password reenter response from client");
            unlock_critical_section(&semOp);
            return false;
        }

        if (strcmp(r_buff, new_pass) == 0)                                                                        // compare new and reentered passward
        {
                                                                                                                                          // If New & reentered passwords match

            strcpy(loggedInCustomer.password, new_pass);                                                                               // copy the new passward to logged in customer

            int customerFD = open(CUSTOMER_FILE, O_WRONLY);                                                                   // open the customer file to update the entery
            if (customerFD == -1)
            {
                perror("Error opening customer file!");
                unlock_critical_section(&semOp);
                return false;
            }

            off_t offset = lseek(customerFD, loggedInCustomer.id * sizeof(struct Customer), SEEK_SET);                        // move to current customer account
            if (offset == -1)
            {
                perror("Error seeking to the customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            struct flock lock = {F_WRLCK, SEEK_SET, offset, sizeof(struct Customer), getpid()};                                            // write lock on record
            int lockStatus = fcntl(customerFD, F_SETLKW, &lock);
            if (lockStatus == -1)
            {
                perror("Error obtaining write lock on customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            w_bytes = write(customerFD, &loggedInCustomer, sizeof(struct Customer));                                         // update the customer account detail
            if (w_bytes == -1)
            {
                perror("Error storing updated customer password into customer record!");
                unlock_critical_section(&semOp);
                return false;
            }

            lock.l_type = F_UNLCK;
            lockStatus = fcntl(customerFD, F_SETLK, &lock);                                                                   // unlock the record

            close(customerFD);                                                                                                   // close the file

            w_bytes = write(connFD, PASSWORD_CHANGE_SUCCESS, strlen(PASSWORD_CHANGE_SUCCESS));                                            // server send passward change successfully
            r_bytes = read(connFD, r_buff, sizeof(r_buff)); // Dummy read

            unlock_critical_section(&semOp);

            return true;
        }
        else
        {
                                                                                                                                              // New & reentered passwords don't match
            w_bytes = write(connFD, PASSWORD_CHANGE_NEW_PASS_INVALID, strlen(PASSWORD_CHANGE_NEW_PASS_INVALID));
            r_bytes = read(connFD, r_buff, sizeof(r_buff)); // Dummy read
        }
    }
    else
    {
                                                                                                                                              // Password doesn't match with old password
        w_bytes = write(connFD, PASSWORD_CHANGE_OLD_PASS_INVALID, strlen(PASSWORD_CHANGE_OLD_PASS_INVALID));
        r_bytes = read(connFD, r_buff, sizeof(r_buff)); // Dummy read
    }

    unlock_critical_section(&semOp);

    return false;
}


//====================================================================================================================================================================================================

bool lock_critical_section(struct sembuf *semOp)
{
    semOp->sem_flg = SEM_UNDO;                                                                                              //it will be automatically undone when the process terminates.
    semOp->sem_op = -1;                                                                                                     // -1 for lock
    semOp->sem_num = 0;                                                                                                     //first semaphore
    int semopStatus = semop(sem_iden, semOp, 1);
    if (semopStatus == -1)
    {
        perror("Error while locking critical section");
        return false;
    }
    return true;
}

bool unlock_critical_section(struct sembuf *semOp)
{
    semOp->sem_op = 1;                                                                                                     // 1 for unlock
    int semopStatus = semop(sem_iden, semOp, 1);                                             
    if (semopStatus == -1)
    {
        perror("Error while operating on semaphore!");
        _exit(1);
    }
    return true;
}


int write_transaction_to_file(int accountNumber, long int oldBalance, long int newBalance, bool operation)               //function to upadate the transaction into  transaction file 
{
    struct Transaction new_transaction;
    new_transaction.accountNumber = accountNumber;
    new_transaction.oldBalance = oldBalance;
    new_transaction.newBalance = newBalance;
    new_transaction.operation = operation;                                                                                 // 0 -> Withdraw, 1 -> Deposit  
    new_transaction.transactionTime = time(NULL);

    ssize_t r_bytes, w_bytes;

    int transactionFD = open(TRANSACTION_FILE, O_CREAT | O_APPEND | O_RDWR, S_IRWXU);                         // open transaction file

    
    off_t offset = lseek(transactionFD, -sizeof(struct Transaction), SEEK_END);                               // move to last record or most recent record
    if (offset >= 0)
    {
                                                                                                                          // There exists at least one transaction record
        struct Transaction prevTransaction;
        r_bytes = read(transactionFD, &prevTransaction, sizeof(struct Transaction));

        new_transaction.transactionID = prevTransaction.transactionID + 1;                                                 //new_id = perv_id +1
    }
    else
                                                                                                                          // No transaction records exist then firt transaction
        new_transaction.transactionID = 0;

    w_bytes = write(transactionFD, &new_transaction, sizeof(struct Transaction));                           // update the new record in transaction file

    return new_transaction.transactionID;
}

void write_transaction_to_array(int *transactionArray, int ID)
{
                                                                                                                         // Check if there's any free space in the array to write the new transaction ID
    int iter = 0;
    while (transactionArray[iter] != -1)
        iter++;

    if (iter >= MAX_TRANSACTIONS)
    {
                                                                                                                          // No space
        for (iter = 1; iter < MAX_TRANSACTIONS; iter++)
                                                                                                                          // Shift elements one step back discarding the oldest transaction
            transactionArray[iter - 1] = transactionArray[iter];
        transactionArray[iter - 1] = ID;
    }
    else
    {
                                                                                                                         // Space available
        transactionArray[iter] = ID;
    }
}

// ===================================================================================================================================================================================================
