/******************************************************************************/
/*                                                                            */
/* Application: WSpack                                                        */
/*                                                                            */
/* File:        WSpack.c                                                      */
/*                                                                            */
/* Purpose:     Quick and dirty method of packing a floating point number for */
/*              transmission.                                                 */
/*                                                                            */
/* Reference:   This function is based on showip.c in Brian "Beej Jorgensen"  */
/*              Hall's excellent socket programming guide:                    */
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
/*    Steven C. Mitchell 2023-01-06 Port from Unix/Linux                      */
/*                                                                            */
/******************************************************************************/
#include <stdio.h>
#include <stdint.h>

uint32_t htonf(float);
float ntohf(uint32_t);
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Main:                                                                      */
/*                                                                            */
int main(void)
{
    float    f;
    float    f2;
    uint32_t netf;

    f = 3.1415926F;

    netf = htonf(f);
    f2 = ntohf(netf);

    printf("Original: %f\n", f);
    printf(" Network: 0x%08X\n", netf);
    printf("Unpacked: %f\n", f2);

    return 0;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Convert from host to network byte order:                                   */
/*                                                                            */
uint32_t htonf(float f)
{
    uint32_t p;
    uint32_t sign;
    /* Sign bit:                                                                  */
    if (f < 0)
    {
        sign = 1;
        f = -f;
    }
    else
    {
        sign = 0;
    }
    /* Whole part and sign:                                                       */
    p = ((((uint32_t)f) & 0x7fff) << 16) | (sign << 31);
    /* Fraction:                                                                  */
    p |= (uint32_t)(((f - (int)f) * 65536.0f)) & 0xffff;
    /* Return result:                                                             */
    return p;
}
/*                                                                            */
/******************************************************************************/
/*                                                                            */
/* Convert from network to host byte order:                                   */
/*                                                                            */
float ntohf(uint32_t p)
{
    float f;
    /* Whole part:                                                                */
    f = (float)((p >> 16) & 0x7fff);
    /* Fraction:                                                                  */
    f += (p & 0xffff) / 65536.0f;
    /* Sign bit set:                                                              */
    if (((p >> 31) & 0x1) == 0x1)
    {
        f = -f;
    }
    /* Return result:                                                             */
    return f;
}
