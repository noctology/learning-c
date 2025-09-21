/******************************************************************************/
/*                                                                            */
/* Application: WSgethostname                                                 */
/*                                                                            */
/* File:        main.c                                                        */
/*                                                                            */
/* Purpose:     Use gethostname to obtain the system's host name.             */
/*                                                                            */
/* Modifications:                                                             */
/*                                                                            */
/*           Name           Date                     Reason                   */
/*    ------------------ ---------- ----------------------------------------- */
/*    Steven C. Mitchell 2022-11-14 Orignal creation                          */
/*                                                                            */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

void get_msg_text(DWORD, char**);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Main:                                                                      */
/*                                                                            */
int main(int argc, char* argv[])
{
    char* nc_error;
    DWORD    dw_error;
    char     ac_hostname[256];
    int       i_return;
    int       i_status;
    WSADATA   s_wsaData;
    /*                                                                            */
    /* The program expects an empty command line:                                 */
    /*                                                                            */
    if (argc != 1)
    {
        fprintf(stderr, "usage: WSgethostname\n");
        return 1;
    }
    /*                                                                            */
    /* Initialize Winsock and request version 2.2:                                */
    /*                                                                            */
    i_status = WSAStartup(MAKEWORD(2, 2), &s_wsaData);

    if (i_status != 0)
    {
        dw_error = (DWORD)i_status;
        get_msg_text(dw_error, &nc_error);
        fprintf(stderr, "WSAStartup failed with code %d.\n", i_status);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        return 2;
    }
    /*                                                                            */
    /* Verify that version 2.2 is available:                                      */
    /*                                                                            */
    if (LOBYTE(s_wsaData.wVersion) < 2 ||
        HIBYTE(s_wsaData.wVersion) < 2)
    {
        fprintf(stderr, "Version 2.2 of Winsock is not available.\n");
        WSACleanup();

        return 3;
    }
    /*                                                                            */
    /* Get the host's name:                                                       */
    /*                                                                            */
    i_status = gethostname(ac_hostname, sizeof(ac_hostname));

    if (i_status == 0)
    {
        printf("Host name is %s\n", ac_hostname);

        i_return = 0;
    }
    else
    {
        dw_error = (DWORD)WSAGetLastError();
        get_msg_text(dw_error, &nc_error);
        fprintf(stderr, "gethostname failed with code %ld.\n", dw_error);
        fprintf(stderr, "%s\n", nc_error);
        LocalFree(nc_error);

        i_return = 4;
    }
    /*                                                                            */
    /* Terminate Winsock:                                                         */
    /*                                                                            */
    WSACleanup();
    /*                                                                            */
    /* Return:                                                                    */
    /*                                                                            */
    return i_return;
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
