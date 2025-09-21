/******************************************************************************/
/*                                                                            */
/* Application: WSselectserver                                                */
/*                                                                            */
/* File:        WSpollserver.c                                                */
/*                                                                            */
/* Purpose:     Implement a crude, multi-person chat server to demonstrate    */
/*              using select on multiple servers.                             */
/*                                                                            */
/* Reference:   This function is based on selectserver.c in Brian "Beej       */
/*              Jorgensen" Hall's excellent socket programming guide:         */
/*                 Hall, B. (2019). "Beej's Guide to Network Programming      */
/*                 Using Internet Sockets"                                    */
/*                 https://beej.us/guide/bgnet/                               */
/*              It was ported to Windows 10 (Winsock version 2.2) by Steven   */
/*              Mitchell.                                                     */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2023-01-03 Port from Unix/Linux                      */
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
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT "9034" // port we're listening on
#define BACKLOG 10  // how many pending connections queue will hold

void* get_in_addr(struct sockaddr*);
void    get_msg_text(DWORD, char**);
int     initialize_winsock(void);
SOCKET  open_a_socket(char*, int);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int main
(
    void
)
{
    int                       i_addrlen;
    char                     ac_buf[256];   // buffer for client data
    DWORD                    dw_error;      // error code
    char* nc_error;      // error message
    SOCKET                      fdmax;      // maximum file descriptor number
    int                         i;          // outer loop counter
    int                         j;          // inner loop counter
    SOCKET                      listener;   // listening socket descriptor
    fd_set                    s_master;     // master file descriptor list
    int                       i_nbytes;
    SOCKET                      newfd;      // newly accept()ed socket descriptor
    int                       i_nfds;
    fd_set                    s_read_fds;   // temp file descriptor list for select()
    struct sockaddr_storage   s_remoteaddr; // client address
    char                     ac_remoteIP[INET6_ADDRSTRLEN];
    int                       i_rv;         // Value returned by a function
    /*                                                                            */
    /* Initialize Winsock:                                                        */
    /*                                                                            */
    if (!initialize_winsock())
    {
        exit(EXIT_FAILURE);
    }
    /*                                                                            */
    /* Get a socket and bind to it:                                               */
    /*                                                                            */
    listener = open_a_socket((char *)PORT, BACKLOG);

    if (listener == INVALID_SOCKET)
    {
        WSACleanup();

        exit(EXIT_FAILURE);
    }
    /*                                                                            */
    /* Clear the master and temp sets:                                            */
    /*                                                                            */
    FD_ZERO(&s_master);

    FD_ZERO(&s_read_fds);
    /*                                                                            */
    /* Add the listener to the master set:                                        */
    /*                                                                            */
    FD_SET(listener, &s_master);
    /*                                                                            */
    /* Keep track of the biggest file descriptor:                                 */
    /*                                                                            */
    fdmax = listener; // so far, it's this one
    /*                                                                            */
    /* Main loop:                                                                 */
    /*                                                                            */
    for (;;)
    {
        /*                                                                            */
        /* Copy the master list of sockets to the working list:                       */
        /*                                                                            */
        s_read_fds = s_master;
        /*                                                                            */
        /* Call select on the working copy:                                           */
        /*                                                                            */
        i_nfds = (int)fdmax + 1;
        i_rv = select(i_nfds, &s_read_fds, NULL, NULL, NULL);

        if (i_rv == SOCKET_ERROR)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "select failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);

            WSACleanup();
            exit(EXIT_FAILURE);
        }
        /*                                                                            */
        /* Run through the existing connections looking for data to read:             */
        /*                                                                            */
        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &s_read_fds)) // we got one!!
            {
                if (i == listener)
                {
                    /*                                                                            */
                    /* Handle new connections:                                                    */
                    /*                                                                            */
                    i_addrlen = (int)sizeof(s_remoteaddr);
                    newfd = accept(listener, (struct sockaddr*)&s_remoteaddr,
                        &i_addrlen);
                    if (newfd == INVALID_SOCKET)
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
                        FD_SET(newfd, &s_master); // add to master set

                        if (newfd > fdmax) // keep track of the max
                        {
                            fdmax = newfd;
                        }

                        printf("selectserver: new connection from %s on socket %lld\n",
                            inet_ntop(s_remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&s_remoteaddr),
                                ac_remoteIP,
                                INET6_ADDRSTRLEN),
                            newfd);
                    }
                }
                else
                {
                    /*                                                                            */
                    /* Handle data from a client:                                                 */
                    /*                                                                            */
                    i_nbytes = recv(i, ac_buf, sizeof(ac_buf), 0);

                    if (i_nbytes <= 0) // got error or connection closed by client
                    {
                        if (i_nbytes == 0) // connection closed
                        {
                            printf("selectserver: socket %d hung up\n", i);
                        }
                        else
                        {
                            dw_error = (DWORD)WSAGetLastError();
                            get_msg_text(dw_error,
                                &nc_error);
                            fprintf(stderr, "recv failed with code %ld.\n", dw_error);
                            fprintf(stderr, "%s\n", nc_error);
                            LocalFree(nc_error);
                        }

                        closesocket(i);      // bye!
                        FD_CLR(i, &s_master); // remove from master set
                    }
                    else // we got some data from a client
                    {
                        for (j = 0; j <= fdmax; j++) // send to everyone!
                        {
                            if (FD_ISSET(j, &s_master))
                            {
                                if (j != listener && j != i) // except the listener and ourselves
                                {
                                    i_rv = send(j, ac_buf, i_nbytes, 0);

                                    if (i_rv == SOCKET_ERROR)
                                    {
                                        dw_error = (DWORD)WSAGetLastError();
                                        get_msg_text(dw_error,
                                            &nc_error);
                                        fprintf(stderr, "send failed with code %ld.\n", dw_error);
                                        fprintf(stderr, "%s\n", nc_error);
                                        LocalFree(nc_error);
                                    }
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
 /*                                                                            */
 /* Cleanup and exit:                                                          */
 /*                                                                            */
    WSACleanup();

    exit(EXIT_FAILURE);
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* get sockaddr, IPv4 or IPv6:                                                */
/*                                                                            */
void* get_in_addr
(
    struct sockaddr* sa
)
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
/* Initialize Winsock and request version 2.2:                                */
/*                                                                            */
int initialize_winsock
(
)
{
    char* nc_error;
    DWORD    dw_error;
    int       i_status;
    WSADATA   s_wsaData;
    /*                                                                            */
    /* Initialize Winsock version 2.2:                                            */
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

        return(0);
    }
    /*                                                                            */
    /* Verify that version 2.2 is available:                                      */
    /*                                                                            */
    if (LOBYTE(s_wsaData.wVersion) < 2 ||
        HIBYTE(s_wsaData.wVersion) < 2)
    {
        fprintf(stderr, "Version 2.2 of Winsock is not available.\n");
        WSACleanup();

        return(0);
    }
    /*                                                                            */
    /* If the function gets to here, it was successful:                           */
    /*                                                                            */
    return(1);
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Open a socket on the specified port:                                       */
/*                                                                            */
SOCKET open_a_socket
(
    char* nc_port,   /* in   - Port to use for connection                      */
    int    i_backlog /* in   - Number of pending connections queue will hold   */
)
{
    struct addrinfo* ps_address;
    struct addrinfo* ps_address_list;
    int               i_addrlen;
    char* nc_error;
    DWORD            dw_error;
    struct addrinfo   s_hints;
    SOCKET              sockfd;
    int               i_status;
    BOOL              B_yes;
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
    i_status = getaddrinfo(NULL, nc_port, &s_hints, &ps_address_list);

    if (i_status != 0)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "getaddrinfo failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        return(INVALID_SOCKET);
    }
    /*                                                                            */
    /* Loop through the results and bind to the first we can:                     */
    /*                                                                            */
    for (ps_address = ps_address_list;
        ps_address != NULL;
        ps_address = ps_address->ai_next)
    {
        /* Attempt to open the socket:                                                */
        sockfd = socket(ps_address->ai_family,
            ps_address->ai_socktype,
            ps_address->ai_protocol);
        if (sockfd == INVALID_SOCKET)
        {
            continue;
        }
        /* Allow reuse of the socket:                                                 */
        B_yes = TRUE;
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
            freeaddrinfo(ps_address_list);

            return(INVALID_SOCKET);
        }
        /* Bind to the socket:                                                        */
        i_addrlen = (int)ps_address->ai_addrlen;
        i_status = bind(sockfd, ps_address->ai_addr, i_addrlen);

        if (i_status == SOCKET_ERROR)
        {
            closesocket(sockfd);

            continue;
        }

        break;
    }
    /*                                                                            */
    /* Done with the list of addresses:                                           */
    /*                                                                            */
    freeaddrinfo(ps_address_list);
    /*                                                                            */
    /* Check for a connection:                                                    */
    /*                                                                            */
    if (ps_address == NULL)
    {
        fprintf(stderr, "Failed to bind to a socket.\n");
        closesocket(sockfd);

        return(INVALID_SOCKET);
    }
    /*                                                                            */
    /* Tell the connection to listen for incoming traffic:                        */
    /*                                                                            */
    i_status = listen(sockfd, i_backlog);

    if (i_status == SOCKET_ERROR)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "listen failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        closesocket(sockfd);

        return(INVALID_SOCKET);
    }
    /*                                                                            */
    /* Successful so return the socket:                                           */
    /*                                                                            */
    return(sockfd);
}
