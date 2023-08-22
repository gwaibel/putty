/*
 * PEAK-CAN-API2 Implementation
 * This module implements the Canapi2 functions by loading it from the canapi2.dll
 */

#include <windows.h>
#include "canapi2.h"


//function pointer typedefs (canapi2.dll functions)
typedef DWORD (__stdcall *PR_PEAKFunc_RegisterClient)(LPCSTR, DWORD, HCANCLIENT*);
typedef DWORD (__stdcall *PR_PEAKFunc_ConnectToNet)(HCANCLIENT, LPSTR, HCANNET*);
typedef DWORD (__stdcall *PR_PEAKFunc_DisconnectFromNet)(HCANCLIENT, HCANNET);
typedef DWORD (__stdcall *PR_PEAKFunc_GetNetParam)(HCANNET, WORD, void*, WORD);
typedef DWORD (__stdcall *PR_PEAKFunc_ResetHardware)(HCANHW);
typedef DWORD (__stdcall *PR_PEAKFunc_RegisterMsg)(HCANCLIENT, HCANNET, TCANMsg*, TCANMsg*);
typedef DWORD (__stdcall *PR_PEAKFunc_RegisterNet)(HCANNET, LPCSTR, HCANHW, WORD );
typedef DWORD (__stdcall *PR_PEAKFunc_GetClientParam)(HCANCLIENT, WORD, void*, WORD);
typedef DWORD (__stdcall *PR_PEAKFunc_SetClientParam)(HCANCLIENT, WORD, DWORD);
typedef DWORD (__stdcall *PR_PEAKFunc_SetHwParam)(HCANHW, WORD, DWORD);
typedef DWORD (__stdcall *PR_PEAKFunc_RemoveClient)(HCANCLIENT);
typedef DWORD (__stdcall *PR_PEAKFunc_Read)(HCANCLIENT, TCANMsg*, HCANNET*, TCANTimestamp*);
typedef DWORD (__stdcall *PR_PEAKFunc_Write)(HCANCLIENT,HCANNET, TCANMsg *, TCANTimestamp*);
typedef DWORD (__stdcall *PR_PEAKFunc_GetSystemTime)(TCANTimestamp*);
typedef DWORD (__stdcall *PR_PEAKFunc_Status)(HCANHW);
typedef DWORD (__stdcall *PR_PEAKFunc_GetErrText)(DWORD, LPSTR);
typedef DWORD (__stdcall *PR_PEAKFunc_GetHwParam)(HCANHW, WORD, void*, WORD);
typedef DWORD (__stdcall *PR_PEAKFunc_GetDeviceName)(LPSTR);
typedef DWORD (__stdcall *PR_PEAKFunc_SetDeviceName)(LPSTR);

static struct {
    //pointer instances
    PR_PEAKFunc_RegisterClient      PEAK_RegisterClient;
    PR_PEAKFunc_ConnectToNet        PEAK_ConnectToNet;
    PR_PEAKFunc_DisconnectFromNet   PEAK_DisconnectFromNet;
    PR_PEAKFunc_GetNetParam         PEAK_GetNetParam;
    PR_PEAKFunc_ResetHardware       PEAK_ResetHardware;
    PR_PEAKFunc_RegisterMsg         PEAK_RegisterMsg;
    PR_PEAKFunc_RegisterNet         PEAK_RegisterNet;
    PR_PEAKFunc_GetClientParam      PEAK_GetClientParam;
    PR_PEAKFunc_SetClientParam      PEAK_SetClientParam;
    PR_PEAKFunc_SetHwParam          PEAK_SetHwParam;
    PR_PEAKFunc_RemoveClient        PEAK_RemoveClient;
    PR_PEAKFunc_Read                PEAK_Read;
    PR_PEAKFunc_Write               PEAK_Write;
    PR_PEAKFunc_GetSystemTime       PEAK_GetSystemTime;
    PR_PEAKFunc_Status              PEAK_Status;
    PR_PEAKFunc_GetErrText          PEAK_GetErrText;
    PR_PEAKFunc_GetHwParam          PEAK_GetHwParam;
    PR_PEAKFunc_GetDeviceName       PEAK_GetDeviceName;
    PR_PEAKFunc_SetDeviceName       PEAK_SetDeviceName;
    HMODULE handle;
} dll;

/*
 * Load the Canapi2 dll and import the API functions.
 * This function shall be called before any Canapi2 function!
 */
int canapi2_init(void)
{
    if (dll.handle != NULL)
        return 0;       // dll already loaded

    dll.handle = LoadLibraryA("canapi2.dll");
    if (dll.handle == NULL) {
        return -1;
    }

    //get function addresses
    dll.PEAK_RegisterClient     = (PR_PEAKFunc_RegisterClient)      GetProcAddress(dll.handle,"CAN_RegisterClient");
    dll.PEAK_ConnectToNet       = (PR_PEAKFunc_ConnectToNet)        GetProcAddress(dll.handle,"CAN_ConnectToNet");
    dll.PEAK_DisconnectFromNet  = (PR_PEAKFunc_DisconnectFromNet)   GetProcAddress(dll.handle,"CAN_DisconnectFromNet");
    dll.PEAK_GetNetParam        = (PR_PEAKFunc_GetNetParam)         GetProcAddress(dll.handle,"CAN_GetNetParam");
    dll.PEAK_ResetHardware      = (PR_PEAKFunc_ResetHardware)       GetProcAddress(dll.handle,"CAN_ResetHardware");
    dll.PEAK_RegisterMsg        = (PR_PEAKFunc_RegisterMsg)         GetProcAddress(dll.handle,"CAN_RegisterMsg");
    dll.PEAK_RegisterNet        = (PR_PEAKFunc_RegisterNet)         GetProcAddress(dll.handle,"CAN_RegisterNet");
    dll.PEAK_GetClientParam     = (PR_PEAKFunc_GetClientParam)      GetProcAddress(dll.handle,"CAN_GetClientParam");
    dll.PEAK_SetClientParam     = (PR_PEAKFunc_SetClientParam)      GetProcAddress(dll.handle,"CAN_SetClientParam");
    dll.PEAK_SetHwParam         = (PR_PEAKFunc_SetHwParam)          GetProcAddress(dll.handle,"CAN_SetHwParam");
    dll.PEAK_RemoveClient       = (PR_PEAKFunc_RemoveClient)        GetProcAddress(dll.handle,"CAN_RemoveClient");
    dll.PEAK_Read               = (PR_PEAKFunc_Read)                GetProcAddress(dll.handle,"CAN_Read");
    dll.PEAK_Write              = (PR_PEAKFunc_Write)               GetProcAddress(dll.handle,"CAN_Write");
    dll.PEAK_GetSystemTime      = (PR_PEAKFunc_GetSystemTime)       GetProcAddress(dll.handle,"CAN_GetSystemTime");
    dll.PEAK_Status             = (PR_PEAKFunc_Status)              GetProcAddress(dll.handle,"CAN_Status");
    dll.PEAK_GetErrText         = (PR_PEAKFunc_GetErrText)          GetProcAddress(dll.handle,"CAN_GetErrText");
    dll.PEAK_GetHwParam         = (PR_PEAKFunc_GetHwParam)          GetProcAddress(dll.handle,"CAN_GetHwParam");
    dll.PEAK_GetDeviceName      = (PR_PEAKFunc_GetDeviceName)       GetProcAddress(dll.handle,"CAN_GetDeviceName");
    dll.PEAK_SetDeviceName      = (PR_PEAKFunc_SetDeviceName)       GetProcAddress(dll.handle,"CAN_SetDeviceName");

    if ((dll.PEAK_RegisterClient == NULL) || (dll.PEAK_ConnectToNet   == NULL) || (dll.PEAK_GetNetParam    == NULL) ||
        (dll.PEAK_ResetHardware  == NULL) || (dll.PEAK_RegisterMsg    == NULL) || (dll.PEAK_RegisterNet    == NULL) ||
        (dll.PEAK_GetNetParam    == NULL) || (dll.PEAK_GetClientParam == NULL) || (dll.PEAK_SetClientParam == NULL) ||
        (dll.PEAK_SetHwParam     == NULL) || (dll.PEAK_RemoveClient   == NULL) || (dll.PEAK_Read           == NULL) ||
        (dll.PEAK_Write          == NULL) || (dll.PEAK_GetSystemTime  == NULL) || (dll.PEAK_Status         == NULL) ||
        (dll.PEAK_GetErrText     == NULL) || (dll.PEAK_GetDeviceName  == NULL) || (dll.PEAK_GetHwParam     == NULL) ||
        (dll.PEAK_DisconnectFromNet == NULL))
    {
        FreeLibrary(dll.handle);
        dll.handle = NULL;
        return -1;
    }

    return 0;
}

void canapi2_close(void)
{
    if (dll.handle != NULL) {
        FreeLibrary(dll.handle);
        dll.handle = NULL;
    }
}

DWORD __stdcall CAN_RegisterClient(
        LPCSTR      lpszName,     // Name des Clients (zur Info)
        DWORD       hWnd,         // Window-Handle des Clients (zur Info)
        HCANCLIENT* phClient)     // Rueckgabe des Client-Handles
{
    return dll.PEAK_RegisterClient(lpszName, hWnd, phClient);
}

DWORD __stdcall CAN_ConnectToNet(
        HCANCLIENT hClient,       // schliesse diesen Client ...
        LPSTR      lpszNetName,   // an dieses Netz an
        HCANNET*   phNet)         // Rueckgabe der Net-Handle
{
   return dll.PEAK_ConnectToNet(hClient, lpszNetName, phNet);
}

DWORD __stdcall CAN_DisconnectFromNet(
        HCANCLIENT hClient,  // haenge diesen Client ...
        HCANNET    hNet)     // von diesem Netz ab.
{
    return dll.PEAK_DisconnectFromNet(hClient, hNet);
}

DWORD __stdcall CAN_GetNetParam(
        HCANNET hNet,
        WORD    wParam,
        void*   pBuff,
        WORD    wBuffLen)
{
    return dll.PEAK_GetNetParam(hNet, wParam, pBuff, wBuffLen);
}

DWORD __stdcall CAN_ResetHardware(
        HCANHW hHw)          // Zurueckzusetzende Hardware
{
    return dll.PEAK_ResetHardware(hHw);
}

DWORD __stdcall CAN_RegisterMsg(
        HCANCLIENT     hClient, // dieser Client ...
        HCANNET        hNet,    // will aus diesen Netz ...
        const TCANMsg* pMsg1,   // alle Message von msg1 ...
        const TCANMsg* pMsg2)   // bis msg2 empfangen.
{
    return dll.PEAK_RegisterMsg(hClient, hNet, pMsg1, pMsg2);
}

DWORD __stdcall CAN_RegisterNet(
        HCANNET hNet,        // Handle des Netzes
        LPCSTR  lpszName,    // Name    "     "
        HCANHW  hHw,         // zugehoerige Hardware, 0 wenn keine Hardware
        WORD    wBTR0BTR1)   // Baudrate
{
    return dll.PEAK_RegisterNet(hNet, lpszName, hHw, wBTR0BTR1);
}

DWORD __stdcall CAN_GetClientParam(
        HCANCLIENT hClient,
        WORD       wParam,
        void*      pBuff,
        WORD       wBuffLen)
{
    return dll.PEAK_GetClientParam(hClient, wParam, pBuff, wBuffLen);
}

DWORD __stdcall CAN_SetClientParam(
        HCANCLIENT  hClient,
        WORD        wParam,
        DWORD       dwValue)
{
   return dll.PEAK_SetClientParam(hClient, wParam, dwValue);
}

DWORD __stdcall CAN_SetHwParam(
        HCANHW hHw,
        WORD   wParam,
        DWORD  dwValue)
{
    return dll.PEAK_SetHwParam(hHw, wParam, dwValue);
}

DWORD __stdcall CAN_RemoveClient(
        HCANCLIENT hClient)   // diesen Client komplett aufloesen
{
    return dll.PEAK_RemoveClient(hClient);
}

DWORD __stdcall CAN_Read(
        HCANCLIENT      hClient,        // Lies RcvPuffer dieses Clients
        TCANMsg*        pMsgBuff,       // Rueckgabe der Message/des Errors
        HCANNET*        phNet,          // Netz, aus dem Msg kam
        TCANTimestamp*  pRcvTime)       // Rueckgabe des Empfangszeitpunktes
{
    return dll.PEAK_Read(hClient, pMsgBuff, phNet, pRcvTime);
}

DWORD __stdcall CAN_Write(
        HCANCLIENT      hClient,        // Handle des sendenden Clients
        HCANNET         hNet,           // Schreibe auf dieses Netz
        TCANMsg*        pMsgBuff,       // Zu schreibende Message
        TCANTimestamp*  pSendTime)      // Sendezeitpunkt
{
    return dll.PEAK_Write(hClient, hNet, pMsgBuff, pSendTime);
}

DWORD __stdcall CAN_GetSystemTime(TCANTimestamp* pTimeBuff)
{
    return dll.PEAK_GetSystemTime(pTimeBuff);
}

DWORD __stdcall CAN_Status(
        HCANHW hHw)          // Zu untersuchende Hardware
{
    return dll.PEAK_Status(hHw);
}

DWORD __stdcall CAN_GetErrText(
        DWORD dwError,             // Zu interpretierender Fehler
        LPSTR lpszTextBuff)        // Textpuffer fuer Fehler-Text
{
    return dll.PEAK_GetErrText(dwError, lpszTextBuff);
}

DWORD __stdcall CAN_GetHwParam(
        HCANHW hHw,
        WORD   wParam,
        void*  pBuff,
        WORD   wBuffLen)
{
    return dll.PEAK_GetHwParam(hHw, wParam, pBuff, wBuffLen);
}

DWORD __stdcall CAN_GetDeviceName(LPSTR szBuff)
{
    return dll.PEAK_GetDeviceName(szBuff);
}

DWORD __stdcall CAN_SetDeviceName(LPSTR szDeviceName)
{
    return dll.PEAK_SetDeviceName(szDeviceName);
}
