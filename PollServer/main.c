/******************************************************************************/
/*                                                                            */
/* Application: WSpollserver                                                  */
/*                                                                            */
/* File:        WSpollserver.c                                                */
/*                                                                            */
/* Purpose:     Implement a crude, multi-person chat server to demonstrate    */
/*              using poll on multiple servers.                               */
/*                                                                            */
/* Reference:   This function is based on pollserver.c in Brian "Beej         */
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
/*    Steven C. Mitchell 2022-11-23 Port from Unix/Linux                      */
/*    Steven C. Mitchell 2023-01-04 Fixed code output if getaddrinfo error    */
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

#define PORT "9034" // Port we're listening on
#define BACKLOG 10  // how many pending connections queue will hold

void add_to_pfds(WSAPOLLFD**, SOCKET, int*, int*);
void del_from_pfds(WSAPOLLFD*, int, int*);
void* get_in_addr(struct sockaddr*);
SOCKET get_listener_socket(void);
void get_msg_text(DWORD, char**);
void initialize_WSAPOLLFD_values(WSAPOLLFD**, int, int);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int main
(
    void
)
{
    int                       i_addrlen;
    char                     ac_buf[256];   // Buffer for client data
    SOCKET                      dest_fd;
    char* nc_error;
    DWORD                    dw_error;
    int                       i_fd_count;
    int                       i_fd_size;
    int                         i;          // Outer loop counter
    int                         j;          // Inner loop counter
    SOCKET                      listener;   // Listening socket descriptor
    size_t                   st_memory;     // Size of item in bytes
    int                       i_nbytes;
    SOCKET                      newfd;      // Newly accept()ed socket descriptor
    WSAPOLLFD* ns_pfds;
    int                       i_poll_count;
    struct sockaddr_storage   s_remoteaddr; // Client address
    char                     ac_remoteIP[INET6_ADDRSTRLEN];
    SOCKET                      sender_fd;
    int                       i_status;
    WSADATA                   s_wsaData;
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
    /* Start off with room for 5 connections (We'll realloc as necessary):        */
    /*                                                                            */
    i_fd_count = 0;
    i_fd_size = 5;

    st_memory = sizeof(WSAPOLLFD) * i_fd_size;
    ns_pfds = (WSAPOLLFD*)malloc(st_memory);
    initialize_WSAPOLLFD_values(&ns_pfds, 0, i_fd_size);
    /*                                                                            */
    /* Set up a listening socket:                                                 */
    /*                                                                            */
    listener = get_listener_socket();

    if (listener == INVALID_SOCKET)
    {
        free(ns_pfds);
        WSACleanup();

        return 3;
    }
    /*                                                                            */
    /* Add the listener to set:                                                   */
    /*                                                                            */
    ns_pfds[0].fd = listener;
    ns_pfds[0].events = POLLIN; // Report ready to read on incoming connection
    ns_pfds[0].revents = 0;

    i_fd_count = 1; // For the listener
    /*                                                                            */
    /* Main loop:                                                                 */
    /*                                                                            */
    for (;;)
    {
        i_poll_count = WSAPoll(ns_pfds, i_fd_count, -1); // -1 = no timeout

        if (i_poll_count == SOCKET_ERROR)
        {
            fprintf(stderr, "Unexpected event occurred: %d\n", ns_pfds[0].revents);
            closesocket(listener);
            free(ns_pfds);
            WSACleanup();

            return 4;
        }
        /* Run through the existing connections looking for data to read:             */
        for (i = 0; i < i_fd_count; i++)
        {
            if (ns_pfds[i].revents & POLLIN || // We got one!!
                ns_pfds[i].revents & POLLHUP)  // Hang up
            {
                /* If listener is ready to read, handle new connection:                       */
                if (ns_pfds[i].fd == listener)
                {
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
                        add_to_pfds(&ns_pfds, newfd, &i_fd_count, &i_fd_size);

                        inet_ntop(s_remoteaddr.ss_family,
                            get_in_addr((struct sockaddr*)&s_remoteaddr),
                            ac_remoteIP, INET6_ADDRSTRLEN);

                        printf("pollserver: new connection from %s on socket %lld\n",
                            ac_remoteIP, newfd);
                    }
                }
                /* If not the listener, we're just a regular client:                          */
                else
                {
                    i_nbytes = recv(ns_pfds[i].fd, ac_buf, sizeof(ac_buf), 0);
                    sender_fd = ns_pfds[i].fd;
                    /* Got error or connection closed by client:                                  */
                    if (i_nbytes == SOCKET_ERROR || i_nbytes == 0)
                    {
                        if (i_nbytes == 0) // Connection closed
                        {
                            fprintf(stderr, "socket %lld hung up\n", sender_fd);
                        }
                        else
                        {
                            dw_error = (DWORD)WSAGetLastError();
                            get_msg_text(dw_error,
                                &nc_error);
                            fprintf(stderr, "accept failed with code %ld.\n", dw_error);
                            fprintf(stderr, "%s\n", nc_error);
                            LocalFree(nc_error);
                        }
                        closesocket(ns_pfds[i].fd); // Bye!
                        del_from_pfds(ns_pfds, i, &i_fd_count);
                    }
                    /* We got some good data from a client:                                       */
                    else
                    {
                        /* Send to everyone!                                                          */
                        for (j = 0; j < i_fd_count; j++)
                        {
                            dest_fd = ns_pfds[j].fd;
                            /* Except the listener and sender:                                            */
                            if (dest_fd != listener && dest_fd != sender_fd)
                            {
                                i_status = send(dest_fd, ac_buf, i_nbytes, 0);

                                if (i_status == SOCKET_ERROR)
                                {
                                    dw_error = (DWORD)WSAGetLastError();
                                    get_msg_text(dw_error,
                                        &nc_error);
                                    fprintf(stderr, "send failed with code %ld\n",
                                        dw_error);
                                    fprintf(stderr, "%s\n", nc_error);
                                    LocalFree(nc_error);
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through socket descriptors
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
/* Add a new socket descriptor to the set:                                    */
void add_to_pfds
(
    WSAPOLLFD** pns_pfds,
    SOCKET          newfd,
    int* pi_fd_count,
    int* pi_fd_size
)
{
    size_t st_memory; // Number of bytes required to hold an item
    int     i_old_size;
    /*                                                                            */
    /* If we don't have room, add more space in the pfds array:                   */
    /*                                                                            */
    if ((*pi_fd_count) == (*pi_fd_size))
    {
        i_old_size = (*pi_fd_size);
        (*pi_fd_size) *= 2; // Double it
        st_memory = sizeof(WSAPOLLFD) * (*pi_fd_size);

        *pns_pfds = (WSAPOLLFD*)realloc(*pns_pfds, st_memory);
        initialize_WSAPOLLFD_values(pns_pfds, i_old_size, (*pi_fd_size));
    }
    /*                                                                            */
    /* Add the new entry:                                                         */
    /*                                                                            */
    (*pns_pfds)[*pi_fd_count].fd = newfd;
    (*pns_pfds)[*pi_fd_count].events = POLLIN; // Check ready-to-read
    (*pns_pfds)[*pi_fd_count].revents = 0;

    (*pi_fd_count)++;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Get sockaddr, IPv4 or IPv6:                                                */
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
/* Return a listening socket:                                                 */
/*                                                                            */
SOCKET get_listener_socket
(
    void
)
{
    struct addrinfo* ps_address;
    int               i_addrlen;
    struct addrinfo* ps_ai;
    char* nc_error;
    DWORD            dw_error;
    struct addrinfo   s_hints;
    SOCKET              listener; // Listening socket descriptor
    int               i_status;
    BOOL              B_yes;      // For setsockopt() SO_REUSEADDR, below
    /*                                                                            */
    /* Get us a socket and bind it:                                               */
    /*                                                                            */
    /* Get a list of addresses:                                                   */
    memset(&s_hints, 0, sizeof(s_hints));

    s_hints.ai_family = AF_UNSPEC;
    s_hints.ai_socktype = SOCK_STREAM;
    s_hints.ai_flags = AI_PASSIVE; // use my IP

    i_status = getaddrinfo(NULL, PORT, &s_hints, &ps_ai);

    if (i_status != 0)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "getaddrinfo failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        return INVALID_SOCKET;
    }
    /* Look for an address to which we can bind:                                  */
    for (ps_address = ps_ai;
        ps_address != NULL;
        ps_address = ps_address->ai_next)
    {
        listener = socket(ps_address->ai_family, ps_address->ai_socktype,
            ps_address->ai_protocol);
        if (listener == INVALID_SOCKET)
        {
            continue;
        }
        /* Allow reuse of the socket:                                                 */
        B_yes = TRUE;
        i_status = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char*)&B_yes,
            sizeof(B_yes));
        if (i_status == SOCKET_ERROR)
        {
            dw_error = (DWORD)WSAGetLastError();
            get_msg_text(dw_error,
                &nc_error);
            fprintf(stderr, "setsockopt failed with code %ld.\n", dw_error);
            fprintf(stderr, "%s\n", nc_error);
            LocalFree(nc_error);
            closesocket(listener);
            freeaddrinfo(ps_ai);

            return INVALID_SOCKET;
        }
        /* Bind to the socket:                                                        */
        i_addrlen = (int)ps_address->ai_addrlen;
        i_status = bind(listener, ps_address->ai_addr, i_addrlen);

        if (i_status == SOCKET_ERROR)
        {
            continue;
        }

        break;
    }
    /*                                                                            */
    /* Free the list of addresses:                                                */
    /*                                                                            */
    freeaddrinfo(ps_ai);
    /*                                                                            */
    /* Check for a connection:                                                    */
    /*                                                                            */
    if (ps_address == NULL)
    {
        fprintf(stderr, "Failed to bind to a socket.\n");
        closesocket(listener);

        return INVALID_SOCKET;
    }
    /*                                                                            */
    /* Listen:                                                                    */
    /*                                                                            */
    i_status = listen(listener, BACKLOG);

    if (i_status == SOCKET_ERROR)
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error,
            &nc_error);
        fprintf(stderr, "listen failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);
        closesocket(listener);

        return INVALID_SOCKET;
    }
    /*                                                                            */
    /* Success so return the socket:                                              */
    /*                                                                            */
    return listener;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Remove an index from the set                                               */
/*                                                                            */
void del_from_pfds
(
    WSAPOLLFD* ns_pfds,
    int           i,
    int* pi_fd_count
)
{
    ns_pfds[i] = ns_pfds[(*pi_fd_count) - 1];

    ns_pfds[(*pi_fd_count) - 1].fd = -1;
    ns_pfds[(*pi_fd_count) - 1].events = 0;
    ns_pfds[(*pi_fd_count) - 1].revents = 0;

    (*pi_fd_count)--;
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
/* Initialize the WSAPOLLFD structures in the list of structures:             */
/*                                                                            */
void initialize_WSAPOLLFD_values
(
    WSAPOLLFD** pns_pfds,
    int           i_start,
    int           i_end
)
{
    int i;

    for (i = i_start; i < i_end; i++)
    {
        (*pns_pfds)[i].fd = -1; // Documentation says to use a negative value
        // if WSAPOLL is to ignore a socket
        (*pns_pfds)[i].events = 0;
        (*pns_pfds)[i].revents = 0;
    }
}

// Last modified: 2023-01-04 22:36 (MST)