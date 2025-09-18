/******************************************************************************/
/*                                                                            */
/* Application: WSserver                                                      */
/*                                                                            */
/* File:        WSserver.c                                                    */
/*                                                                            */
/* Purpose:     Listen for a client to connect to port 3490.  Send the string */
/*              "Hello, world!" to the client.                                */
/*                                                                            */
/* Reference:   This function is based on server.c in Brian "Beej Jorgensen"  */
/*              Hall's excellent socket programming guide:                    */
/*                 Hall, B. (2019). "Beej's Guide to Network Programming      */
/*                 Using Internet Sockets"                                    */
/*                 https://beej.us/guide/bgnet/                               */
/*              It was ported to Windows 10 (Winsock version 2.2) by Steven   */
/*              Mitchell.                                                     */
/*                                                                            */
/* Reference:   Thread creation and use section that replaces fork() from the */
/*              original Unix/Linux version is based on Microsoft's online    */
/*              documentation on CreateThread available at                    */
/*              https://learn.microsoft.com/en-us/windows/win32/procthread/   */
/*              creating-threads/                                             */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-11-09 Port from Unix/Linux                      */
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Prevent automatic include of winsock.h which does not play nice with       */
/* winsock2.h:                                                                */
/*                                                                            */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT "3490" // The port to which client will be connecting
#define BACKLOG 10 // how many pending connections queue will hold

struct thread_info
{
    HANDLE h_thread;
    SOCKET   new_fd;
};

int active_thread_count(struct thread_info*);
void add_socket_to_thread_list(SOCKET, struct thread_info*, int*);
void* get_in_addr(struct sockaddr*);
void get_msg_text(DWORD, char**);
DWORD WINAPI thread_function(LPVOID);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Main:                                                                      */
/*                                                                            */
int main(int argc, char* argv[])
{
    struct addrinfo* ps_address;
    int                       i_addrlen;
    struct sockaddr_storage   s_client; // clients's address information
    char* nc_error;
    DWORD                    dw_error;
    struct addrinfo           s_hints;
    int                       i_lc;
    int                       i_location; // location in thread list of new thread
    SOCKET                      new_fd;  // new connection on new_fd
    struct addrinfo* ps_servinfo;
    char                     ac_server[INET6_ADDRSTRLEN];
    socklen_t                   sin_size;
    SOCKET                      sockfd;  // listen on sock_fd
    int                       i_status;
    DWORD                    dw_status; // wait status of thread
    struct thread_info       as_threads[BACKLOG];
    HANDLE                    h_thread;
    DWORD                    dw_thread_id;
    WSADATA                   s_wsaData;
    BOOL                      B_yes = TRUE;
    /*                                                                            */
    /* The program does not expect any arguments on the command line:             */
    /*                                                                            */
    if (argc != 1)
    {
        fprintf(stderr, "usage: WSserver\n");

        return 1;
    }
    /*                                                                            */
    /* Initialize the list of threads:                                            */
    /*                                                                            */
    for (i_lc = 0; i_lc < BACKLOG; i_lc++)
    {
        as_threads[i_lc].h_thread = NULL;
        as_threads[i_lc].new_fd = 0;
    }
    /*                                                                            */
    /* Initialize Winsock and request version 2.2:                                */
    /*                                                                            */
    i_status = WSAStartup(MAKEWORD(2, 2), &s_wsaData);

    if (i_status != 0)
    {
        dw_error = (DWORD)i_status;
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "WSAStartup failed with code %d.\n", i_status);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        return 1;
    }
    /*                                                                            */
    /* Verify that version 2.2 is available:                                      */
    /*                                                                            */
    if (LOBYTE(s_wsaData.wVersion) < 2 ||
        HIBYTE(s_wsaData.wVersion) < 2)
    {
        fprintf(stderr, "Version 2.2 of Winsock is not available.\n");
        WSACleanup();

        return 2;
    }
    /*                                                                            */
    /* Set the desired IP address characteristics:                                */
    /*                                                                            */
    memset(&s_hints, 0, sizeof(s_hints));

    s_hints.ai_family = AF_UNSPEC;
    s_hints.ai_socktype = SOCK_STREAM;
    s_hints.ai_flags = AI_PASSIVE; // use my IP
    /*                                                                            */
    /* Request a list of IP addresses:                                            */
    /*                                                                            */
    i_status = getaddrinfo(NULL, PORT, &s_hints, &ps_servinfo);

    if (i_status != 0)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "getaddrinfo failed with code %d.\n", i_status);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        WSACleanup();

        return 3;
    }
    /*                                                                            */
    /* Loop through all the results and bind to the first we can:                 */
    /*                                                                            */
    for (ps_address = ps_servinfo;
        ps_address != NULL;
        ps_address = ps_address->ai_next)
    {
        sockfd = socket(ps_address->ai_family, ps_address->ai_socktype,
            ps_address->ai_protocol);
        if (sockfd == INVALID_SOCKET)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "socket failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);

            continue;
        }
        /* Allow reuse of the socket:                                                 */
        i_status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&B_yes,
            sizeof(B_yes));
        if (i_status == SOCKET_ERROR)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "setsockopt failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);
            closesocket(sockfd);
            freeaddrinfo(ps_servinfo);
            WSACleanup();

            return 4;
        }
        /* Bind to the socket:                                                        */
        i_addrlen = (int)ps_address->ai_addrlen;
        i_status = bind(sockfd, ps_address->ai_addr, i_addrlen);

        if (i_status == SOCKET_ERROR)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "bind failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);
            closesocket(sockfd);

            continue;
        }

        break;
    }
    /*                                                                            */
    /* Check for a connection:                                                    */
    /*                                                                            */
    if (ps_address == NULL)
    {
        fprintf(stderr, "Failed to bind to a socket.\n");
        closesocket(sockfd);
        freeaddrinfo(ps_servinfo);
        WSACleanup();

        return 5;
    }
    /*                                                                            */
    /* Listen for a connection:                                                   */
    /*                                                                            */
    i_status = listen(sockfd, BACKLOG);

    if (i_status == SOCKET_ERROR)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "listen failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        closesocket(sockfd);
        freeaddrinfo(ps_servinfo);
        WSACleanup();

        return 6;
    }
    /*                                                                            */
    /* Free the list of addresses:                                                */
    /*                                                                            */
    freeaddrinfo(ps_servinfo);
    /*                                                                            */
    /* Wait for a connection:                                                     */
    /*                                                                            */
    printf("Waiting for connections...\n");

    while (1)  // main accept() loop
    {
        /*                                                                            */
        /* Accept a connection:                                                       */
        /*                                                                            */
        if (active_thread_count(as_threads) < BACKLOG)
        {
            sin_size = sizeof(s_client);
            new_fd = accept(sockfd, (struct sockaddr*)&s_client, &sin_size);

            if (new_fd == INVALID_SOCKET)
            {
                dw_error = (DWORD)WSAGetLastError();
                get_msg_text(dw_error,
                    &nc_error);
                fprintf(stderr, "accept failed with code %ld.\n", dw_error);
                fprintf(stderr, "%s\n", nc_error);
                LocalFree(nc_error);
            }
            else
            {
                inet_ntop(s_client.ss_family,
                    get_in_addr((struct sockaddr*)&s_client),
                    ac_server, sizeof(ac_server));
                printf("Got connection from %s\n", ac_server);
                /*                                                                            */
                /* Start a thread that will send the message to the client:                   */
                /*                                                                            */
                add_socket_to_thread_list(new_fd,
                    as_threads,
                    &i_location);
                h_thread = CreateThread(NULL, 0, thread_function,
                    (void*)&(as_threads[i_location].new_fd),
                    0, &dw_thread_id);
                as_threads[i_location].h_thread = h_thread;

                if (h_thread == NULL)
                {
                    dw_error = GetLastError();
                    get_msg_text(dw_error,
                        &nc_error);
                    fprintf(stderr, "CreateThread failed with code %ld.\n", dw_error);
                    fprintf(stderr, "%s\n", nc_error);
                    closesocket(new_fd); // No longer needed
                }
            }
        }
        else
        {
            fprintf(stderr, "Connection refused.  Backlog is full.\n");
        }
        /*                                                                            */
        /* Check for finished threads:                                                */
        /*                                                                            */
        for (i_lc = 0; i_lc < BACKLOG; i_lc++)
        {
            if (as_threads[i_lc].h_thread != NULL)
            {
                dw_status = WaitForSingleObject(as_threads[i_lc].h_thread, 0);

                if (dw_status == WAIT_OBJECT_0)
                {
                    CloseHandle(as_threads[i_lc].h_thread);
                    as_threads[i_lc].h_thread = NULL;
                }
            }
        }
    }
    /*                                                                            */
    /* Should not get here:                                                       */
    /*                                                                            */
    return 7;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Determine number of active worker threads:                                 */
/*                                                                            */
int active_thread_count
(
    struct thread_info* ns_threads /* in   - List of threads                   */
)
{
    int i_count;
    int i_lc;

    i_count = 0;

    for (i_lc = 0; i_lc < BACKLOG; i_lc++)
    {
        if (ns_threads[i_lc].h_thread != NULL)
        {
            i_count++;
        }
    }

    return(i_count);
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Add a thread to the list (place its handle in the first unused location in */
/* the list):                                                                 */
/*                                                                            */
void add_socket_to_thread_list
(
    SOCKET                 new_fd,  /* in   - Socket to which thread should    */
    /*        send data                        */
    struct thread_info* ns_threads, /* both - List of threads                  */
    int* pi_location /* out  - Location in list to which the    */
    /*        socket was added                 */
)
{
    int i_lc;

    for (i_lc = 0; i_lc < BACKLOG; i_lc++)
    {
        if (ns_threads[i_lc].h_thread == NULL)
        {
            ns_threads[i_lc].new_fd = new_fd;
            *pi_location = i_lc;
            break;
        }
    }

    return;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Get sockaddr, IPv4 or IPv6:                                                */
/*                                                                            */
void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Get Error Message Text:                                                    */
/*                                                                            */
void get_msg_text
(
    DWORD    dw_error, /* in   - Error code                                    */
    char** pnc_msg    /* out  - Error message                                 */
)
{
    DWORD dw_flags;
    /*                                                                            */
    /* Set message options:                                                       */
    /*                                                                            */
    dw_flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS;
    /*                                                                            */
    /* Create the message string:                                                 */
    /*                                                                            */
    FormatMessage(dw_flags, NULL, dw_error, LANG_SYSTEM_DEFAULT, (LPTSTR)pnc_msg, 0,
        NULL);
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Function executed by a new thread to send a response to the client:        */
/*                                                                            */
DWORD WINAPI thread_function(LPVOID lpParam)
{
    char* nc_error;
    DWORD   dw_error;
    DWORD   dw_exit_code;
    SOCKET* p_new_fd;
    int      i_status;
    /*                                                                            */
    /* Initialize exit code:                                                      */
    /*                                                                            */
    dw_exit_code = 0;
    /*                                                                            */
    /* Decode the SOCKET to which the message should be sent:                     */
    /*                                                                            */
    p_new_fd = (SOCKET*)lpParam;
    /*                                                                            */
    /* Send the message to the client:                                            */
    /*                                                                            */
    i_status = send(*p_new_fd, "Hello, world!", 13, 0);

    if (i_status == SOCKET_ERROR)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "send failed with code %ld\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        dw_exit_code = 1;
    }
    /*                                                                            */
    /* Close the client's socket:                                                 */
    /*                                                                            */
    closesocket(*p_new_fd); // No longer needed
    /*                                                                            */
    /* Return:                                                                    */
    /*                                                                            */
    return(dw_exit_code);
}