/******************************************************************************/
/*                                                                            */
/* File:    select.c                                                          */
/*                                                                            */
/* Purpose: Monitor a socket the old way.  The original select.c monitor      */
/*          stdin which is a file descriptor.  This works in Unix/Linux       */
/*          because file descriptors and socket descriptors are essentially   */
/*          the same and even share some functions (e.g., close).  This is    */
/*          not true in Windows.  This program is a Unix/Linux program that   */
/*          demonstrates select using a socket.                               */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-12-12 Orignal creation                          */
/*                                                                            */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>

#define PORT "3490" // the port users will be connecting to
#define BACKLOG 10  // how many pending connections queue will hold

int open_a_socket(char*);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int main(void)
{
    int            i_errno;
    int            i_rv;
    int            i_sockfd;
    struct timeval s_tv;
    fd_set         s_readfds;
    /*                                                                            */
    /* Open a socket:                                                             */
    /*                                                                            */
    i_sockfd = open_a_socket(PORT);

    if (i_sockfd == -1)
    {
        return 1;
    }
    /*                                                                            */
    /* Listen to this socket:                                                     */
    /*                                                                            */
    errno = 0;
    i_rv = listen(i_sockfd, BACKLOG);
    i_errno = errno;

    if (i_rv == -1)
    {
        fprintf(stderr, "listen failed with code %d.\n", i_errno);
        fprintf(stderr, "%s\n", strerror(i_errno));
        close(i_sockfd);

        return 2;
    }
    /*                                                                            */
    /* Use the select function:                                                   */
    /*                                                                            */
    s_tv.tv_sec = 2;
    s_tv.tv_usec = 500000;

    FD_ZERO(&s_readfds);
    FD_SET(i_sockfd, &s_readfds);
    /* Don't care about writefds and exceptfds so set to NULL:                    */
    select(i_sockfd + 1, &s_readfds, NULL, NULL, &s_tv);

    if (FD_ISSET(i_sockfd, &s_readfds))
    {
        printf("A client connected!\n");
    }
    else
    {
        printf("Timed out.\n");
    }
    /*                                                                            */
    /* Success so close socket and return success code:                           */
    /*                                                                            */
    close(i_sockfd);

    return 0;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
int open_a_socket
(
    char* nc_port
)
{
    struct addrinfo* ps_address;
    int               i_errno;
    struct addrinfo   s_hints;
    int               i_rv;
    struct addrinfo* ps_servinfo;
    int               i_sockfd;
    int               i_yes;
    /*                                                                            */
    /* Get a list of possible connections:                                        */
    /*                                                                            */
    memset(&s_hints, 0, sizeof(s_hints));
    s_hints.ai_family = AF_UNSPEC;
    s_hints.ai_socktype = SOCK_STREAM;
    s_hints.ai_flags = AI_PASSIVE; // use my IP

    i_rv = getaddrinfo(NULL, nc_port, &s_hints, &ps_servinfo);
    if (i_rv != 0)
    {
        fprintf(stderr, "getaddrinfo failed with code %d.\n", i_rv);
        fprintf(stderr, "%s\n", gai_strerror(i_rv));

        return(-1);
    }
    /*                                                                            */
    /* Loop through all the results and bind to the first we can:                 */
    /*                                                                            */
    for (ps_address = ps_servinfo;
        ps_address != NULL;
        ps_address = ps_address->ai_next)
    {
        /* Attempt to open the socket:                                                */
        i_sockfd = socket(ps_address->ai_family, ps_address->ai_socktype,
            ps_address->ai_protocol);
        if (i_sockfd == -1)
        {
            continue;
        }
        /* Allow reuse of the socket:                                                 */
        i_yes = 1;
        errno = 0;
        i_rv = setsockopt(i_sockfd, SOL_SOCKET, SO_REUSEADDR, &i_yes, sizeof(int));
        i_errno = errno;

        if (i_rv == -1)
        {
            fprintf(stderr, "setsockopt failed with code %d.\n", i_errno);
            fprintf(stderr, "%s\n", strerror(i_errno));
            close(i_sockfd);
            freeaddrinfo(ps_servinfo);

            return(-1);
        }
        /* Bind to the socket:                                                        */
        i_rv = bind(i_sockfd, ps_address->ai_addr, ps_address->ai_addrlen);
        if (i_rv == -1)
        {
            close(i_sockfd);

            continue;
        }

        break;
    }
    /*                                                                            */
    /* All done with this structure:                                              */
    /*                                                                            */
    freeaddrinfo(ps_servinfo);
    /*                                                                            */
    /* Check for a connection:                                                    */
    /*                                                                            */
    if (ps_address == NULL)
    {
        fprintf(stderr, "select failed to bind to a socket.\n");
        close(i_sockfd);

        return(-1);
    }
    /*                                                                            */
    /* Successful so return the socket:                                           */
    /*                                                                            */
    return(i_sockfd);
}
