#ifndef __CANAPI2H__        // Schutz gegen mehrfaches #include
#define __CANAPI2H__

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////
//  CanApi2.h: Definition der CAN-API, Version 2
//
//  Version 2.47.1.0
//
//  Grundidee:
//  ~~~~~~~~~~
//  Der Treiber unterstuetzt mehrere Clients (=Windows/DOS-Programme,
//  die mit CAN-Bussen kommunizieren wollen), und mehrere Hardwarekarten
//  mit 82C200 oder SJA 1000.
//  Zentral ist der Begriff des 'Netzes': er beschreibt einen in den
//  PC hinein verlaengerten CAN-Bus, an den sich mehrere Clients
//  anschliessen koennen, und der ueber eine Hardwarekarte mit einem
//  externen CAN-Bus verbunden sein kann.
//  Eine Netzdefinition benennt neben der Baudrate implizit
//  eine Menge von zu verarbeitetenden CAN-Messages.
//
//  Clients, die auf einen bestimmten CAN-Bus spezialisiert sind
//  (z.B. Schrittmotoransteuerung, Radio-Panel), sollten dem Benutzer
//  gar keine Hardwareauswahl mehr ermoeglichen, sondern fest ein
//  bestimmtes Netz ('Labor-Net', 'Vectra-MMI') ansprechen. Die Verbindung
//  "Netz - Hardware" wird dann mit einem separaten Konfigurationstool
//  in der Windows-Systemsteuerung durchgefuehrt (in Abh. vom
//  jeweiligen PC u. dessen CAN-Hardware).
//
//  Ggf. koennen die CAN-Knoten eines externen CAN-Busses auch durch
//  andere Clients am selben Netz simuliert werden. In diesem Fall
//  braucht keine CAN-Hardware vorhanden zu sein, der komplette Bus
//  kann im PC simuliert werden. Im Konfigurationstool wird das Netz dann
//  auf "intern" o.ae. eingestellt.
//
//
//  Beispiele fuer moegliche Netzkonfigurationen:
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  (auch alle zusammen realisierbar):
//                                                   externer
//                                    ,------------< CAN-Bus "A"
//  ,--------. ,--------.       ,-----+----.
//  |Client A| |Client B|       |Hardware 1|
//  `---+----' `----+---'       `-----+----'
//      `-----------+-----------------'
//              N e t z  I                           externer
//                                    ,------------< CAN-Bus "B"
//  ,--------. ,--------.       ,-----+----.
//  |Client C| |Client D|       |Hardware 2|
//  `---+--+-' `----+---'       `-----+----'
//      |  `--------+-----------------'              externer
//      |       N e t z  II           ,------------< CAN-Bus "C"
//      |      ,--------.       ,-----+----.
//      |      |Client E|       |Hardware 3|
//      |      `----+---'       `-----+----'
//      `-----------+-----------------'             "Gateway"
//              N e t z  III
//
//  ,--------. ,--------. ,--------.
//  |Client F| |Client G| |Client H|
//  `---+----' `---+----' `---+----'                "internes Netz"
//      `----------+----------'
//              N e t z  IV
//
//  Features:
//  ~~~~~~~~~
//  - 1 Client kann an mehrere Netze angeschlossen sein,
//  - 1 Netz versorgt mehrere Clients.
//  - 1 Hardware gehoert zu maximal 1 Netz.
//  - Einem Netz ist keine oder genau eine Hardware zugeordnet.
//
//  - Sendet ein Client, wird die Msg ueber die Hardware auf den externen
//    Bus und an alle anderen Clients gegeben.
//  - Wird eine Message ueber die Hardware empfangen, wird sie von allen
//    Clients empfangen. Jeder Client empfaengt nur Messages, die
//    sein Akzeptanzfilter passieren.
//
//  - ein Konfigurationstool definiert die eingebauten Hardwarekarten,
//    und die bekannten Netze.
//    Pro Hardware duerfen mehrere Netze (=Name + Baudrate) definiert
//    sein. Aber nur eins davon kann gleichzeitig aktiv sein
//    ('CAN_ConnectToNet()')
//  - Clients verbinden sich mit einem Netz ueber den Netznamen
//
//  - jede Hardware hat einen Xmt-Queue, um zu sendende Messages
//    zu puffern.
//
//  - jeder Client hat einen Rcv-Queue, in dem die an ihn geschickten
//    Messages gepuffert werden.
//
//  - jeder Client hat einen Xmt-Queue, in der zu sendendene Messages
//    bis zu ihrem Sendezeitpunkt warten. Bei Eintritt des
//    Sendezeitpunktes werden sie in den Xmt-Queue der Hardware
//    geschrieben.
//
//  - hClient: 'Client-Handle'= Nr. unter der der Treiber einen
//                          Clients verwaltet.
//  - hHw : 'Hardware-Handle' = Nr, unter der der Treiber eine
//                          Treiberhardware verwaltet.
//  - hNet: 'Netz-Handle' = Nr, unter der der Treiber ein Netz verwaltet.
//  - alle Handles laufen ab 1, 0 = illegale Handle
//
//  - in der Registry werden Hardware und Netze definiert,
//    die der Treiber beim Start von Windows laedt.
//
//    Schluessel:
//    WinNT/2000/XP:
//        HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Peakcan
//    Win95/98/ME:
//        HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\Vxd\Peakcan
//
//    Werte(als Zeichenfolgen):
//        Hardware<HWHandle>=<DriverNo>,<PortBase>,<IRQ>             ...
//        Net<NetHandle>=<Name>,<HWHandle>,<BTR0BTR1>
//                 ...
//    Bsp.:
//         Hardware1=1,0x300,15
//         Net7=TESTNET,1,0x001C
//
//  - die API-Funktionen sind in 3 Gruppen geteilt:
//    1) Control-API: Kontrolle des Treibers durch Konfigurationstools
//    2) Client-API: Lesen und Schreiben von Messages durch Anwendungen
//    3) Info-API: verschiedene Hilfsroutinen
//
//
//  Control-API
//  ~~~~~~~~~~~~
//  CAN_RegisterHardware(hHw, wDriverNo, wBusID, dwPortBase, wIntNo)
//                  1 Hardware aktivieren:
//                  Speichertest, INT installieren
//                  Hardware wird zukuenftig unter 'hHw' angesprochen.
//  CAN_RegisterNet(hNet, szName, hHw, wBTR0BTR1)
//                  1 Zuordnung Netz - Controllerhardware uebernehmen.
//                  Netz wird zukuenftig unter 'hNet' angesprochen.
//  CAN_RemoveNet(hNet)
//                  1 Netz abbauen
//  CAN_RemoveHardware(hHw)
//                  1 Hardware deaktivieren
//  CAN_CloseAll()
//                  alles wegschalten.
//
//
//  Client-API
//  ~~~~~~~~~~
//
//  Hardwaresteuerung:
//  ~~~~~~~~~~~~~~~~~~
//  CAN_Status(hHw)
//                  aktuellen Status der Hardware zurueckgeben.
//  CAN_ResetHardware(hHw)
//                  Controller Reset,  flush xmtqueue der hw.
//                  beinflusst andere Clients desselben Netzes!
//  CAN_ResetClient(hClient)
//                  flush Rcvpuffer des Clients
//
//  Read/Write:
//  ~~~~~~~~~~~
//  CAN_Write(hClient, hNet, &msgbuff, &sendtime)
//                  Schreibe Message zur zeit 'sendtime' an Netz 'hNet'.
//                  Geschrieben wird an Hardware des Netzes und alle
//                  Clients, die sich per 'ConnectToNet()' an das Netz
//                  angeschlossen haben.
//  CAN_Read(hClient, &msgbuff, &hNet, &rcvtime)
//                  Message aus Rcvqueue lesen.
//                  Infos: msg, Zeit, Fehler, rcv_hNet
//  CAN_Read_Multi(hClient, &buff, max_msg_count, &msg_count)
//                  Mehrere Messages aus RcvQueue lesen.
//
//
//  Verbindungsaufbau durch Clients:
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  CAN_RegisterClient(name, hWnd, &hClient)
//                  Client beim Treiber anmelden,
//                  ClientHandle und RcvPuffer besorgen
//                  (einmalig pro Client)
//  CAN_ConnectToNet(hClient, szNetName, &hNet)
//                  Client mit einem Netz verbinden.
//                  (einmal pro Client und Netz)
//  CAN_RegisterMsg(hClient, hNet, &msg1, &msg2)
//                  Anmeldung, dass aus dem Netz 'hNet' die Messages
//                  'msg1' bis 'msg2' empfangen werden sollen.
//                  Ausgewertet werden ID, RTR und standard/extended frame.
//                  msg1.ID1 <= msg2.ID, msg1.MSGTYPE = msg2.MSGTYPE.
//                  Es gibt nur ein Filter fuer Standard- und extended-messages,
//                  Standard-messages werden so registriert, als waere die ID Bits 28..18.
//                  D.h: Registrieren von standard ID 0x400
//                  bewirkt auch Empfang der extended ID 0x10000000.
//                  Jeder Aufruf dieser Funktion bewirkt evtl. eine Erweiterung
//                  des Empfangsfilters des betroffenen CAN-Controllers, der dadurch
//                  kurz RESET gehen kann.
//                  Mit 'CAN_RegisterMsg()' muessen auch zu beantwortende
//                  Remote-Request-Messages registriert werden.
//                  Es ist nicht garantiert, dass der Client NUR die
//                  registrierten Messages empfaengt.
//  CAN_RemoveAllMsgs(hClient, hNet)
//                  Setzt den Filter eines Clients fuer ein angeschlossenes
//                  Netz zurueck.
//  CAN_SetClientFilter(hClient, hNet, nExtended, dwAccCode, dwAccMask)
//  CAN_SetClientFilterEx(hClient, hNet, dwFilterIndex, dwFilterMode,
//                        nExtended, dwAccCode, dwAccMask)
//                  Setzt den Client-Filter direkt, im SJA100-Stil
//                  (Alternative zu CAN_RegisterMsg())
//  CAN_DisconnectFromNet(hClient, hNet)
//                  Client von einem Netz abhaengen.
//  CAN_RemoveClient(hClient)
//                  Client beim Treiber abmelden, Resourcen freigeben.
//
//
//  Info-API:
//  ~~~~~~~~~~
//  CAN_GetDriverName(i, &namebuff)
//                  Name der vom Treiber unterstuetzten Hardware
//                  Nr 'i' zurueck geben.
//  CAN_Msg2Text(&msgbuff, &textbuff)
//                  Debugging: Textdarstellung einer CAN-Msg erzeugen
//  CAN_GetDiagnostic(&textbuff)
//                  Debugging: den Diagnosetext-Puffer abfragen
//  CAN_GetSystemTime(&timebuff)
//                  Zugriff auf 'Get_System_Time()' des VMM:
//                  Zeit in Millisekunden seit Windows-Start.
//  CAN_GetErrText(err, &textbuff)
//                  gibt zu den Fehlern in 'err' einen String mit den
//                  Fehlertexten  zurueck.
//  CAN_VersionInfo(&textbuff)
//                  gibt einen Versions- und Copyrightvermerk zurueck.
//  CAN_GetHwParam(hHw, wParam, &buff, wBuffLen)
//                  Einen bestimmten Parameter (CAN_PARAM_*) der
//                  Hardware 'hHw' abfragen.
//  CAN_SetHwParam(hHw, wParam, dwValue)
//                  Einen bestimmten Parameter (CAN_PARAM_*) der
//                  Hardware 'hHw' setzen.
//  CAN_GetNetParam(hNet, wParam, &buff, wBuffLen)
//                  Einen bestimmten Parameter (CAN_PARAM_*) von
//                  Netz 'hNet' abfragen.
//  CAN_SetNetParam(hNet, wParam, dwValue)
//                  Einen bestimmten Parameter (CAN_PARAM_*) von
//                  Netz 'hNet' setzen.
//  CAN_GetClientParam(hClient, wParam, &buff, wBuffLen)
//                  Einen bestimmten Parameter (CAN_PARAM_*) von
//                  Client 'hClient' abfragen.
//  CAN_SetClientParam(hClient, wParam, dwValue)
//                  Einen bestimmten Parameter (CAN_PARAM_*) von
//                  Client 'hClient' setzen.
//  CAN_GetDriverParam(param, &buff, bufflen)
//                  Einen bestimmten Parameter (CAN_PARAM_*) des
//                  Treibers abfragen.
//  CAN_SetDriverParam(param, long buff)
//                  Einen bestimmten Parameter (CAN_PARAM_*) vom
//                  Treiber setzen.
//
//  Beispiele fuer den Gebrauch der API:
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  a) Initialisieren der Hardware und der Netze beim Start von Windows.
//     in 'VxD -OnDeviceInit()':
//          // gesteuert durch Registry:
//                  ...
//          CAN_RegisterHardware();   // jede erwaehnte Hardware starten
//          CAN_RegisterHardware();
//                  ...
//          CAN_RegisterNet();        // Netzdefinitionen laden
//          CAN_RegisterNet();
//                  ...
//
//  b) Konfigurationstool
//          LoadConfigFromRegistry()
//          EditConfig()            // Benutzer stellt sich was ein
//          SaveConfigToRegistry()
//          CAN_CloseAll()          // Treiber ruecksetzen
//          // Gesteuert durch Konfiguration
//          CAN_RegisterHardware(); SaveHardwareToRegistry();
//          CAN_RegisterHardware(); SaveHardwareToRegistry();
//                  ....
//          CAN_RegisterNet(); SaveNetToRegistry();
//          CAN_RegisterNet(); SaveNetToRegistry();
//                  ....
//          // Neue Konfiguration ist jetzt und beim naechsten Neustart von
//          // Windows aktiv. Clients sind jetzt natuerlich tot.
//
//  c) Client
//          CAN_RegisterClient();    // Einmalig
//          CAN_ConnectToNet(, &mynet)
//          // CAN_ConnectToNet();   // Evtl. mehrfach, z.B. wenn Gateway
//
//          if (eigene_baudrate)
//          {
//              long lBuff; HCANHW hHw; WORD wMyBaud;
//              CAN_GetNetParam(mynet, CAN_PARAM_NETHW, &lBuff, 0);
//              hw = lBuff;
//              CAN_SetHwParam(hHw, CAN_PARAM_BAUDRATE, wMyBaud);
//          }
//
//          CAN_RegisterMsg();
//
//          while (active)
//          {
//              if (!(CAN_Read() & CAN_ERR_QRCVEMPTY))
//              {
//                  // Es wurde was empfangen
//                  CAN_GetSystemTime(&time);
//                  dwDelay = time.millis - rcvtime.millis;
//              }
//
//              if (something_to_write)
//                  CAN_Write();
//              if (something_exceptional)
//              {
//                  CAN_ResetHardware();
//                  CAN_ResetClient();
//              }
//          }
//
//          CAN_RemoveClient();      // Einmalig, Resourcen freigeben
//
//
//  Bereitgestellte Konstanten:
//  CAN_BAUD_1M ... _5K     div. Baudraten.
//  CAN_PARAM_ ...          versch. Betriebsparameter.
//
//  Alle Funktionen geben ein Set der Fehlerbedingungen
//  CAN_ERR_xxx zurueck.
//
//
//  Autor  : Hoppe, Wolf
//  Sprache: ANSI-C
//
//  --------------------------------------------------------------------
//  Copyright (C) 1995-2004 PEAK-System Technik GmbH, Darmstadt, Germany
//  All rights reserved.


// Konstantendefinitionen

#define CAN_MAX_STANDARD_ID     0x7ff
#define CAN_MAX_EXTENDED_ID     0x1fffffff

// Baudratencodes = Registerwerte BTR0/BTR1
#define CAN_BAUD_1M     0x0014    //   1 MBit/s
#define CAN_BAUD_500K   0x001C    // 500 kBit/s
#define CAN_BAUD_250K   0x011C    // 250 kBit/s
#define CAN_BAUD_125K   0x031C    // 125 kBit/s
#define CAN_BAUD_100K   0x432F    // 100 kBit/s
#define CAN_BAUD_50K    0x472F    //  50 kBit/s
#define CAN_BAUD_20K    0x532F    //  20 kBit/s
#define CAN_BAUD_10K    0x672F    //  10 kBit/s
#define CAN_BAUD_5K     0x7F7F    //   5 kBit/s

// Fehlerzustaende
#define CAN_ERR_OK        0x0000
#define CAN_ERR_XMTFULL   0x0001  // Sendepuffer im Controller ist voll
#define CAN_ERR_OVERRUN   0x0002  // CAN-Controller wurde zu spaet gelesen
#define CAN_ERR_BUSLIGHT  0x0004  // Busfehler: ein Errorcounter erreichte Limit
#define CAN_ERR_BUSHEAVY  0x0008  // Busfehler: ein Errorcounter erreichte Limit
#define CAN_ERR_BUSOFF    0x0010  // Busfehler: CAN_Controller ging 'Bus-Off'
#define CAN_ERR_QRCVEMPTY 0x0020  // RcvQueue ist leergelesen
#define CAN_ERR_QOVERRUN  0x0040  // RcvQueue wurde zu spaet gelesen
#define CAN_ERR_QXMTFULL  0x0080  // Sendequeue ist voll
#define CAN_ERR_REGTEST   0x0100  // RegisterTest des 82C200 schlug fehl
#define CAN_ERR_NOVXD     0x0200  // VxD nicht geladen, Lizenz ausgelaufen
#define CAN_ERRMASK_ILLHANDLE  0x1C00  // Maske fuer alle Handlefehler
#define CAN_ERR_HWINUSE   0x0400  // Hardware ist von Netz belegt
#define CAN_ERR_NETINUSE  0x0800  // an Netz ist Client angeschlossen
#define CAN_ERR_ILLHW     0x1400  // Hardwarehandle war ungueltig
#define CAN_ERR_ILLNET    0x1800  // Netzhandle war ungueltig
#define CAN_ERR_ILLCLIENT 0x1C00  // Clienthandle war ungueltig
#define CAN_ERR_RESOURCE  0x2000  // Resource (FIFO, Client, Timeout) nicht erzeugbar
#define CAN_ERR_ILLPARAMTYPE 0x4000  // Parameter hier nicht erlaubt/anwendbar
#define CAN_ERR_ILLPARAMVAL  0x8000  // Parameterwert ist ungueltig
#define CAN_ERR_UNKNOWN  0x10000  // Unbekannter Fehler

#define CAN_ERR_ANYBUSERR (CAN_ERR_BUSLIGHT | CAN_ERR_BUSHEAVY | CAN_ERR_BUSOFF)

// CAN-Treibertypen
#define CAN_DRIVERTYPE_UNKNOWN  0
#define CAN_DRIVERTYPE_9X       1
#define CAN_DRIVERTYPE_NT       2
#define CAN_DRIVERTYPE_WDM      3

// Prinzipielle Objekte in diesem Treiber
#define CAN_OBJECT_DRIVER       0
#define CAN_OBJECT_HARDWARE     1
#define CAN_OBJECT_NET          2
#define CAN_OBJECT_CLIENT       3

// Parametercodes fuer Statusmessages und (Set|Get)(Hw|Net|Client)Param()

// Ein Busfehler, Wert = CAN_ERR_...
#define CAN_PARAM_BUSERROR      1

// Nr. des Treibertyps (ISA, Dongle, ...)
#define CAN_PARAM_HWDRIVERNR    2

// Name des Hardwaretreibers/des Netzes/des Client
#define CAN_PARAM_NAME          3

// Portadresse der Hardware (DWORD)
#define CAN_PARAM_HWPORT        4

// Hardware Interrupt
#define CAN_PARAM_HWINT         5

// Netz, zu dem die Hardware gehoert
#define CAN_PARAM_HWNET         6

// Baudrate, als BTR0BTR1-Code
#define CAN_PARAM_BAUDRATE      7

// Acc-Filter-Werte (nur ID, Bits 28..0 relevant, auch bei reinem 11-Bit-Betrieb!)
// siehe CAN_PARAM_ACCCODE_STD/CAN_PARAM_ACCMASK_STD
#define CAN_PARAM_ACCCODE_EXTENDED 8
#define CAN_PARAM_ACCMASK_EXTENDED 9
//#define CAN_PARAM_ACCCODE CAN_PARAM_ACCCODE_EXTENDED // Abwaertskompatibilitaet
//#define CAN_PARAM_ACCMASK CAN_PARAM_ACCMASK_EXTENDED // Abwaertskompatibilitaet

// 0: Controller ist reset, 1= am Bus
#define CAN_PARAM_ACTIVE        10

// ungesendete Messages im XmtQueue
#define CAN_PARAM_XMTQUEUEFILL  11

// unverarbeitete Messages im RcvQueue
#define CAN_PARAM_RCVQUEUEFILL  12

// insgesamt empfangene Messages
#define CAN_PARAM_RCVMSGCNT     13

// insgesamt empfangene Bits
#define CAN_PARAM_RCVBITCNT     14

// insgesamt gesendete Messages
#define CAN_PARAM_XMTMSGCNT     15

// insgesamt gesendete Bits
#define CAN_PARAM_XMTBITCNT     16

// insgesamt gesendete oder empfangene Messages
#define CAN_PARAM_MSGCNT        17

// insgesamt gesendete oder empfangene Bits
#define CAN_PARAM_BITCNT        18

// Hardware-Handle eines Netzes
#define CAN_PARAM_NETHW         19

// Flag: Clients[i] <> 0: Client 'i' gehoert zu diesem Netz,
// char[MAX_HCANCLIENT+1]
#define CAN_PARAM_NETCLIENTS    20

// Fenster-Handle des Client
#define CAN_PARAM_HWND          21

// Flag: Nets[i] <> 0: Netz 'i' gehoert zu diesem Client, char[MAX_HCANNET+1]
#define CAN_PARAM_CLNETS        22

// Sendepuffergroesse (HW,CL)
#define CAN_PARAM_XMTBUFFSIZE   23
#define CAN_PARAM_XMTQUEUESIZE  CAN_PARAM_XMTBUFFSIZE // Besserer Name

// Empfangspuffergroesse
#define CAN_PARAM_RCVBUFFSIZE   24
#define CAN_PARAM_RCVQUEUESIZE  CAN_PARAM_RCVBUFFSIZE // Besserer Name

// RCVFULL-Event, wenn nur noch soviele Messages empfangen werden koennen (Client)
#define CAN_PARAM_ONRCV_TRESHOLD 25

// Handle des RCVFULL-Events
#define CAN_PARAM_ONRCV_EVENT_HANDLE  26

// Triggermode des RCVFULL-Events ( 1 = Pulse, 0 = Set)
#define CAN_PARAM_ONRCV_EVENT_PULSE  27

// Self Receive
// 1 = Client empfaengt auch die Messages, die er selbst gesendet hat
#define CAN_PARAM_SELF_RECEIVE 28

// Delayed Message Distribution
// 0 = Sendmessages beim Schreiben in die Hardware-Queue an
//     andere Clients. (Netzeigenschaft)
// 1 = Sendmsgs erst an andere Clients, wenn sie auch in die Hardware
//     gesendet werden.
#define CAN_PARAM_DELAYED_MESSAGE_DISTRIBUTION 29

// Hersteller-Code fuer einen OEM im Dongle; 32 bit Unsigned Integer
#define CAN_PARAM_HW_OEM_ID 30

// Ein Text der die Position der HW beschreibt.
// z.B: "ISA adr 0x220", "I/O adr 0x378", "PCI bus 0, slot 7, controller 1", "USB Bus 1, adr 4".
// Dieser Text kann in der Registry f�r die HW angegeben werden, wird sonst vom Startup
// automatisch erzeugt.
#define CAN_PARAM_LOCATION_INFO 31

// Nr des Busses, an den die Hardware angeschlossen ist
#define CAN_PARAM_HWBUS         32

// PCI SlotNr, an den die Hardware angeschlossen ist
#define CAN_PARAM_HWDEVICE      33

// PCI Function der Karte
#define CAN_PARAM_HWFUNCTION    34

// Nr des CAN-Controllers auf der Karte
#define CAN_PARAM_HWCONTROLLER  35

// UNLOCK-Code f�r die kleinen Treiber. Longint.
#define CAN_PARAM_UNLOCKCODE    36

// Typ des Treibers: 1=9x, 2=NT, 3=WDM
#define CAN_PARAM_DRIVERTYPE    37

// Messwerte des USB-Dongles
#define CAN_PARAM_BUSLOAD       38
#define CAN_PARAM_ANALOG0       39
#define CAN_PARAM_ANALOG1       40
#define CAN_PARAM_ANALOG2       41
#define CAN_PARAM_ANALOG3       42
#define CAN_PARAM_ANALOG4       43
#define CAN_PARAM_ANALOG5       44
#define CAN_PARAM_ANALOG6       45
#define CAN_PARAM_ANALOG7       46

// Quartzfrequenz des CAN-Controllers
#define CAN_PARAM_CHIP_QUARTZ   47

// Tatsaechlicher Wert des COntroller-Timingregister
#define CAN_PARAM_CHIP_TIMING   48

// Listen Only: gilt fuer Hardware: 1 = keine CAN-Aktivitaet
#define CAN_PARAM_LISTEN_ONLY   49

// USB-DeviceNr
#define CAN_PARAM_HW_DEVICENR   50

// PEAK-Seriennr
#define CAN_PARAM_HW_SERNR      51

// ISR Timeout-Schutz in Mikrosekunden
#define CAN_PARAM_ISRTIMEOUT    52

// Error Frames
// <> 0: Errorframes werden wie Messages empfangen
#define CAN_PARAM_RCVERRFRAMES  53

// Code/Mask im 11-Bit Format
#define CAN_PARAM_ACCCODE_STD   54
#define CAN_PARAM_ACCMASK_STD   55  // write erst CODE, dann MASK setzen!

// Exact 11 Bit filtering
// 0 = Client filters by code/mask,
// 1 = Client filters exact Message ranges
#define CAN_PARAM_EXACT_11BIT_FILTER 56

// Location-Info, die der User selber setzen kann (USB-StringDescriptor)
#define CAN_PARAM_USER_LOCATION_INFO 57

// Schalte "Select" LED aus/an (USB)
#define CAN_PARAM_SELECT_LED 58

// Auslesen der Firmware (USB)
#define CAN_PARAM_FIRMWARE_MAJOR 59
#define CAN_PARAM_FIRMWARE_MINOR 60

// CPU-Frequenz in kHz (nur lesen, nur NT/WDM)
#define CAN_PARAM_FCPU          61

// USB: Wartepause nach hw_activate in ms
#define CAN_PARAM_USBACTIVATEDELAY 64

// TimerFix
// <> 0: Aktiviert die Korrektur von PerformanceCounter
#define CAN_PARAM_TIMERFIX      65

// Client-Handle des "Netz-Masters"
// 0 = kein Master definiert
#define CAN_PARAM_NET_MASTER    66

// F�r den CANOpen-SDO-Mode des USB-Dongles. Non-standard: Aufrufreihenfolge beachten!
#define CAN_PARAM_SDO_MODE      67    // set: CAN_Write(); get: CAN_Read() (MSGTYPE_STATUS)
#define CAN_PARAM_SDO_QUEUEFILL 72    // set: CAN_Write(); get: CAN_Read() (MSGTYPE_STATUS)
#define CAN_PARAM_SDO_STATUS    73    // get: CAN_Read() (MSGTYPE_STATUS)

// Unverarbeitete Messages im DelayedXmtQueue eines Client
#define CAN_PARAM_DELAYXMTQUEUEFILL  74

// Sendepuffergroesse Client
#define CAN_PARAM_DELAYXMTBUFFSIZE   75
#define CAN_PARAM_DELAYXMTQUEUESIZE  CAN_PARAM_DELAYXMTBUFFSIZE // Besserer Name

// USB: Abfrage, ob SDO-Mode moeglich ist
#define CAN_PARAM_SDO_SUPPORT     76

// Net: ClientHandle des "SDO-Masters"
// 0 = kein Master definiert
#define CAN_PARAM_SDO_NET_MASTER  77
// Client: 1 = SDO_Status wird empfangen
#define CAN_PARAM_SDO_RECEIVE     78

// 1394: Wartepause nach hw_activate in ms
#define CAN_PARAM_1394ACTIVATEDELAY 80

// USB/1394: 1 = keine Popup-Warnung bei HW-Auswurf
#define CAN_PARAM_SURPRISEREMOVALOK 81

// SelfReceive: wie will der Client SelfReceive in CAN_Read
// signalisiert bekommen ?
// 0 = altes Verhalten: hNet=0
// 1 = neues Verhalten: MSGTYPE_SELFRECEIVE
#define CAN_PARAM_MARK_SELFRECEIVED_MSG_WITH_MSGTYPE  82

// Error Warning Limit im SJA1000
#define CAN_PARAM_ERROR_WARNING_LIMIT  83

// Dual Filter Mode: Will der Client 1 oder 2 Filter nutzen?
#define CAN_PARAM_ACCFILTER_COUNT  84

// Dual Filter Mode: Code/Mask des zweiten Filter-Terms im 11-Bit Format
#define CAN_PARAM_ACCCODE1_STD        85
#define CAN_PARAM_ACCMASK1_STD        86 // write erst CODE, dann MASK setzen!
#define CAN_PARAM_ACCCODE1_EXTENDED   87
#define CAN_PARAM_ACCMASK1_EXTENDED   88 // write erst CODE, dann MASK setzen!

#define CAN_PARAM_BUSON 90 //AST!! missing !!!

#define MAX_HCANHW      16      // nur Hardwares 1 .. MAX_HCANHW erlauben
#define MAX_HCANNET     32      // nur Netze 1 .. MAX_HCANNET erlauben
#define MAX_HCANCLIENT  32      // nur Clients 1 .. MAX_HCANCLIENT erlauben
#define MAX_HCANCLIENT_LIGHT 3  // die Light-Version erlaubt nur 3
#define MAX_HCANMEM = 2 * MAX_HCANCLIENT // Max. 2 Memoryblocks pro Client

#define MAX_NETNAMELEN     20   // max. Laenge eines Netznamens
#define MAX_CLIENTNAMELEN  20   // max. Laenge eines Client-Namens
#define MAX_DRIVERNAMELEN  32   // max. Laenge eines Treibernamens

#define CAN_DIAGBUFFLEN   2048  // Groesse des internen Puffers fuer Debug-Ausgabe

#define MSGTYPE_STANDARD    0x00  // wenn Message einen Standard Frame beschreibt
#define MSGTYPE_RTR         0x01  // 1, wenn Remote Request Frame, sonst Data Frame
#define MSGTYPE_EXTENDED    0x02  // 1, wenn CAN 2.0B Frame (29-Bit ID)
#define MSGTYPE_SELFRECEIVE 0x04  // 1, wenn Message vom Controller selbst empfangen werden soll, bzw. wurde
#define MSGTYPE_SINGLESHOT  0x08  // 1, wenn f�r Message keine retransmission stattfinden soll. (Self ACK)
#define MSGTYPE_PARAMETER   0x20  // 1, wenn Message einen Parameter beschreibt (z.B. USB-SDO)
#define MSGTYPE_ERRFRAME    0x40  // 1, wenn Message einen Error Frame beschreibt
#define MSGTYPE_BUSEVENT  MSGTYPE_ERRFRAME // wegen erweiterter Bedeutung
#define MSGTYPE_STATUS      0x80  // 1, wenn Message eine Statusmeldung beschreibt
#define MSGTYPE_NONMSG      0xF0  // <> 0, wenn Message irgend ein Status ist

// Typendefinitionen

typedef BYTE HCANHW;        // Typ 'Hardware-Handle'
typedef BYTE HCANNET;       // Typ 'Net-Handle'
typedef BYTE HCANCLIENT;    // Typ 'Client-Handle'
typedef BYTE HCANMEM;       // Typ 'Memory-Handle'


#pragma pack(push, 1)       // diese Records Byte-aligned! (Visual-C)


// Timestamp of a receive/send event
// It's not a 64-bit value:
// - easy compatibility to 'millisecond'-timestamps of older versions
// - doesn't rely on 64bit-int-capabilities of compilers

// total microseconds = micros + 1000 * millis + 0xffffffff * 1000 * overflows
typedef struct tagTCANTimestamp
{
    DWORD millis;          // base-value: milliseconds: 0.. 2^32-1
    WORD  millis_overflow; // roll-arounds of millis
    WORD  micros;          // microssecond: 0..999
} TCANTimestamp;


// Eine CAN-Message
typedef struct tagTCANMsg
{
    DWORD ID;        // 11/29-Bit Kennung
    BYTE  MSGTYPE;   // Bits aus MSGTYPE_*
    BYTE  LEN;       // Anzahl der gueltigen Datenbytes (0.8)
    BYTE  DATA[8];   // Datenbytes 0..7
} TCANMsg;

// Eine CAN-Message, die ueber CAN_Read_Multi() empfangen wurde.
typedef struct tagTCANRcvMsg
{
    TCANMsg       msgbuff; // Message
    HCANNET       hNet;    // Netz, aus dem die Msg empfangen wurde
    TCANTimestamp rcvtime; // Rueckgabe des Empfangszeitpunktes
} TCANRcvMsg;


#pragma pack(pop)    // Wieder default Alignment


// Prototypen fuer Methoden

///////////////////////////////////////////////////////////////////////////////
//  CAN_GetDeviceName()
//  Den Namen des anzusprechenden Devices abfragen.
//
//  Moegliche Fehler: keine

DWORD __stdcall CAN_GetDeviceName(LPSTR szBuff);


///////////////////////////////////////////////////////////////////////////////
//  CAN_SetDeviceName()
//  Den Namen des anzusprechenden Devices setzen.
//
//  Moegliche Fehler: NOVXD

DWORD __stdcall CAN_SetDeviceName(LPSTR szDeviceName);


///////////////////////////////////////////////////////////////////////////////
//  CAN_RegisterHardware(), CAN_RegisterHardwarePCI()
//  Aktiviert eine Hardware, macht Registertest des 82C200/SJA1000,
//  teilt einen Sendepuffer und ein Hardware-Handle zu.
//  Programmiert Konfiguration der Sende/Empfangstreiber.
//  Controller bleibt im Reset-Zustand.
//  Es sind mehrere Hardwares am selben IRQ erlaubt.
//
//  Moegliche Fehler: NOVXD ILLHW REGTEST RESOURCE

DWORD __stdcall CAN_RegisterHardware(
        HCANHW hHw,        // gewuenschtes Hardware-Handle
        WORD   wDriverNo,  // Nr. des zu verwendenden Treibers
        WORD   wBusID,     // Code ueber Bus der Karte (0=ISA)
        DWORD  dwPortBase, // E/A-Adresse der Karte im PC
        WORD   wIntNo);    // verwendeter Hardwareinterrupt

DWORD __stdcall CAN_RegisterHardwarePCI(
        HCANHW hHw,               // gewuenschtes Hardware-Handle
        WORD   wDriverNo,         // Nr. des zu verwendenden Treibers
        DWORD  dwPCIslotBus,      // Welcher PCI-Bus?
        DWORD  dwPCIslotDevice,   // Welcher Slot?
        DWORD  dwPCIslotFunction, // Code ueber Bus der Karte
        DWORD  dwControllerNo);   // Welcher CAN-controller auf der Karte?


///////////////////////////////////////////////////////////////////////////////
//  CAN_RegisterNet()
//  Dem Treiber eine Zuordnung 'Netz - Controller-Info' mitteilen.
//
//  Moegliche Fehler: NOVXD ILLNET ILLHW

DWORD __stdcall CAN_RegisterNet(
        HCANNET hNet,        // Handle des Netzes
        LPCSTR  lpszName,    // Name    "     "
        HCANHW  hHw,         // zugehoerige Hardware, 0 wenn keine Hardware
        WORD    wBTR0BTR1);  // Baudrate


///////////////////////////////////////////////////////////////////////////////
//  CAN_RemoveNet()
//  Eine Netzdefinition aus dem Treiber loeschen
//
//  Moegliche Fehler: NOVXD ILLNET NETINUSE

DWORD __stdcall CAN_RemoveNet(
        HCANNET hNet);       // Handle des zu loeschenden Netzes


///////////////////////////////////////////////////////////////////////////////
//  CAN_RemoveHardware()
//  Die Hardware abschalten, Interrupt deaktivieren.
//  XmtQueue bleibt aber allokiert.
//
//  Moegliche Fehler: NOVXD ILLHW

DWORD __stdcall CAN_RemoveHardware(
        HCANHW hHw);         // Handle der zu deaktivierenden Hardware


///////////////////////////////////////////////////////////////////////////////
//  CAN_CloseAll()
//  Alle Hardware wegschalten.
//
//  Moegliche Fehler: NOVXD

DWORD __stdcall CAN_CloseAll(void);


///////////////////////////////////////////////////////////////////////////////
//  CAN_Status()
//  Aktuellen Status (z.B. BUS-OFF) der Hardware zurueckgeben.
//
//  Moegliche Fehler: NOVXD ILLHW BUSOFF BUSHEAVY OVERRUN

DWORD __stdcall CAN_Status(
        HCANHW hHw);         // Zu untersuchende Hardware


///////////////////////////////////////////////////////////////////////////////
//  CAN_ResetHardware()
//  Wenn an das Netz eine Hardware angeschlossen ist:
//  Controller Reset,  flush Transmit-Queue.
//  Beinflusst andere Clients desselben Netzes!
//
//  Moegliche Fehler: NOVXD ILLHW REGTEST

DWORD __stdcall CAN_ResetHardware(
        HCANHW hHw);         // Zurueckzusetzende Hardware


///////////////////////////////////////////////////////////////////////////////
//  CAN_ResetClient()
//  Flush Rcv-Queue des Clients.
//
//  Moegliche Fehler: NOVXD ILLCLIENT

DWORD __stdcall CAN_ResetClient(
        HCANCLIENT hClient); // Zurueckzusetzender Client


///////////////////////////////////////////////////////////////////////////////
//  CAN_Write()
//  Client 'hClient' schreibt eine Message zum Zeitpunkt
//  'pSendTime' an Netz 'hNet'.
//  Geschrieben wird in die Xmt-Queue der Hardware des Netzes und in
//  die Rcv-Queue aller anderen Clients, die sich per
//  'CAN_ConnectToNet()' an das Netz angeschlossen haben.
//  Wenn 'hClient' == 0: Msg wird auch in Rcv-Queue des sendenden
//  Clients rueckgeschrieben.
//
//  Moegliche Fehler: NOVXD RESOURCE ILLCLIENT ILLNET BUSOFF QXMTFULL

DWORD __stdcall CAN_Write(
        HCANCLIENT      hClient,        // Handle des sendenden Clients
        HCANNET         hNet,           // Schreibe auf dieses Netz
        TCANMsg*        pMsgBuff,       // Zu schreibende Message
        TCANTimestamp*  pSendTime);     // Sendezeitpunkt


///////////////////////////////////////////////////////////////////////////////
//  CAN_Read()
//  Gibt die naechste Message oder den naechsten Fehler aus der
//  Rcv-Queue des Clients zurueck.
//  Message wird nach 'pMsgBuff' geschrieben.
//
//  Moegliche Fehler: NOVXD ILLCLIENT QRCVEMPTY

DWORD __stdcall CAN_Read(
        HCANCLIENT      hClient,        // Lies RcvPuffer dieses Clients
        TCANMsg*        pMsgBuff,       // Rueckgabe der Message/des Errors
        HCANNET*        phNet,          // Netz, aus dem Msg kam
        TCANTimestamp*  pRcvTime);      // Rueckgabe des Empfangszeitpunktes


///////////////////////////////////////////////////////////////////////////////
//  CAN_Read_Multi()
//  Gibt mehrere empfangene Messages zurueck.
//  Verhaelt sich wie ein intern mehrfach ausgefuehrtes CAN_Read().
//  pMultiMsgBuff muss die Adresse eines Arrays mit 'n' Eintraegen sein.
//  Die Laenge 'n' des Arrays = maximaler Fuellstand wird in
//  nMaxMsgCount mitgeteilt.
//  Die Anzahl der tatsaechlich gelesenen Messages wird in
//  pMsgCount zurueckgegeben.
//  Der Rueckgabewert ist der des letzten CAN_Read().
//
//  Moegliche Fehler: NOVXD ILLCLIENT QRCVEMPTY

DWORD __stdcall CAN_Read_Multi(
        HCANCLIENT  hClient,       // Lies RcvPuffer dieses Clients
        TCANRcvMsg* pMultiMsgBuff, // Zielarray
        long        nMaxMsgCount,  // Groesse des Puffers
        long*       pMsgCount);    // Rueckgabe der Anzahl der gelesenen Msgs.


///////////////////////////////////////////////////////////////////////////////
//  CAN_RegisterClient()
//  Client beim Treiber anmelden, Client-Handle und RcvPuffer besorgen
//  (einmalig pro Client). hWnd ist 0 fuer DOS-Box-Clients.
//  Client empfaengt noch keine Messages, erst 'CAN_RegisterMsg()'
//  oder 'CAN_SetClientFilter()' aufrufen.
//
//  Moegliche Fehler: NOVXD RESOURCE

DWORD __stdcall CAN_RegisterClient(
        LPCSTR      lpszName,     // Name des Clients (zur Info)
        DWORD       hWnd,         // Window-Handle des Clients (zur Info)
        HCANCLIENT* phClient);    // Rueckgabe des Client-Handles


///////////////////////////////////////////////////////////////////////////////
//  CAN_ConnectToNet()
//  Client mit einem Netz verbinden.
//  Das Netz ueber den Namen finden, die zug. Hardware mit der
//  angegebenen Baudrate initialisieren, wenn es der erste Client
//  dieses Netzes ist (einmal pro Client und Netz).
//  Fehler CAN_ERR_HWINUSE, wenn Hardware schon von einem anderem
//  Netz belegt ist.
//
//  Moegliche Fehler: NOVXD ILLCLIENT ILLNET ILLHW HWINUSE REGTEST

DWORD __stdcall CAN_ConnectToNet(
        HCANCLIENT hClient,       // schliesse diesen Client ...
        LPSTR      lpszNetName,   // an dieses Netz an
        HCANNET*   phNet);        // Rueckgabe der Net-Handle


///////////////////////////////////////////////////////////////////////////////
//  CAN_RegisterMsg()
//  Anmeldung, dass aus dem Netz 'hNet' die Messages
//  'pMsg1' bis 'pMsg2' empfangen werden sollen.
//  Ausgewertet werden ID, RTR und Standard/Extended frame.
//  pMsg1->ID <= pMsg2->ID, pMsg1->MSGTYPE = pMsg2->MSGTYPE.
//  Schnelle Registrierung aller Messages mit pMsg1=pMsg2=NULL.
//  Es gibt nur ein Filter fuer Standard- und Extended-Messages,
//  Standard-Messages werden so registriert, als waere die ID Bits 28..18.
//  D.h: Registrieren von standard ID 0x400
//  bewirkt auch Empfang der extended ID 0x10000000.
//  Jeder Aufruf dieser Funktion bewirkt evtl. eine Erweiterung
//  des Empfangsfilters des betroffenen CAN-Controllers, der dadurch
//  kurz RESET gehen kann.
//  Mit 'CAN_RegisterMsg()' muessen auch zu beantwortende
//  Remote-Request-Messages registriert werden.
//
//  Neu in Ver 2.x: Standard Frames werden in den acc_mask/code
//                  als Bits 28..13 abgelegt.
//                  Hardwaretreiber fuer 82C200 muessen das
//                  wieder auf 10..0 schieben!
//  ACHTUNG: Es ist nicht garantiert, dass der Client NUR die
//           registrierten Messages empfaengt!
//
//  Moegliche Fehler: NOVXD ILLCLIENT ILLNET REGTEST

DWORD __stdcall CAN_RegisterMsg(
        HCANCLIENT     hClient, // dieser Client ...
        HCANNET        hNet,    // will aus diesen Netz ...
        const TCANMsg* pMsg1,   // alle Message von msg1 ...
        const TCANMsg* pMsg2);  // bis msg2 empfangen.


///////////////////////////////////////////////////////////////////////////////
//  CAN_RemoveAllMsgs()
//  Setzt den Filter eines Clients fuer ein angeschlossenes Netz zurueck.
//
//  Moegliche Fehler: NOVXD ILLCLIENT ILLNET

DWORD __stdcall CAN_RemoveAllMsgs(
        HCANCLIENT hClient,     // dieser Client will den Filter ...
		HCANNET    hNet);       // fuer dieses Netz zuruecksetzen


///////////////////////////////////////////////////////////////////////////////
//  CAN_SetClientFilter()
//  Setzt den Filter eines Clients, der angeschlossenen Netze und der
//  angeschlossenen Hardware neu.
//
//  Moegliche Fehler: NOVXD ILLCLIENT ILLNET

DWORD __stdcall CAN_SetClientFilter(
        HCANCLIENT hClient,     // dieser Client ...
        HCANNET    hNet,        // will aus diesen Netz ...
        long       nExtended,   // Messages mit diesem Filter empfangen
        DWORD      dwAccCode,
        DWORD      dwAccMask);


///////////////////////////////////////////////////////////////////////////////
//  CAN_SetClientFilterEx()
//  Setzt den Filter eines Clients, der angeschlossenen Netze und der
//  angeschlossenen Hardware neu.
//
//  Moegliche Fehler: NOVXD ILLCLIENT ILLNET

DWORD __stdcall CAN_SetClientFilterEx(
        HCANCLIENT hClient,       // dieser Client ...
        HCANNET    hNet,          // will aus diesen Netz ...
        DWORD      dwFilterIndex, // Filterauswahl
        DWORD      dwFilterMode,  // Filterfunktion
        long       nExtended,     // Messages mit diesem Filter empfangen
        DWORD      dwAccCode,
        DWORD      dwAccMask);


///////////////////////////////////////////////////////////////////////////////
//  CAN_DisconnectFromNet()
//  Client von einem Netz abhaengen, d.h., keine Messages mehr
//  empfangen.
//  Jeder Aufruf dieser Funktion bewirkt evtl. eine Verengung
//  des Empfangsfilters des betroffenen CAN-Controllers, der dadurch
//  kurz RESET gehen kann.
//
//  Moegliche Fehler: NOVXD ILLCLIENT ILLNET REGTEST

DWORD __stdcall CAN_DisconnectFromNet(
        HCANCLIENT hClient,  // haenge diesen Client ...
        HCANNET    hNet);    // von diesem Netz ab.


///////////////////////////////////////////////////////////////////////////////
//  CAN_RemoveClient()
//  Client beim Treiber abmelden, Resourcen freigeben.
//  Jeder Aufruf dieser Funktion bewirkt evtl. eine Verengung
//  der Empfangsfilter der betroffenen CAN-Controller, die dadurch
//  kurz RESET gehen koennen.
//
//  Moegliche Fehler: NOVXD ILLCLIENT

DWORD __stdcall CAN_RemoveClient(
        HCANCLIENT hClient);  // diesen Client komplett aufloesen


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetDriverName()
//  Name vom Treiber der unterstuetzten Hardware Nr 'i' zurueckgeben.
//  Beginn mit i=1; Listenende, wenn "" zurueckgegeben wird.
//
//  Moegliche Fehler: NOVXD

DWORD __stdcall CAN_GetDriverName(
        SHORT i,             // Index der Hardware
        LPSTR lpszNameBuff); // Zeiger auf Puffer fuer den Namen


///////////////////////////////////////////////////////////////////////////////
//  CAN_Msg2Text()
//  Debugging: Textdarstellung einer CAN-Msg erzeugen.
//
//  Moegliche Fehler: NOVXD

DWORD __stdcall CAN_Msg2Text(
        TCANMsg* pMsgBuff,         // Zu interpretierende Message
        LPSTR    lpszTextBuff);    // Zeiger auf Textpuffer


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetDiagnostic()
//  Debugging: den Diagnosetext-Puffer abfragen
//  (max. CAN_DIAGBUFFLEN Zeichen) und loeschen.
//
//  Moegliche Fehler: NOVXD

DWORD __stdcall CAN_GetDiagnostic(
        LPSTR lpszTextBuff);       // Textpuffer f�r Diagnose-Text


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetSystemTime()
//  Zugriff auf 'Get_System_Time()' des VMM:
//  Zeit in Millisekunden seit Windows-Start.
//
//  Moegliche Fehler: NOVXD

DWORD __stdcall CAN_GetSystemTime(TCANTimestamp* pTimeBuff);


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetErrText()
//  Gibt zu den Fehlern in 'dwError' einen String mit den Fehlertexten
//  zurueck.
//  Ist ausnahmsweise in CANAPI2.DLL und nicht PEAKCAN.SYS implementiert,
//  um schon den Fehler 'NOVXD' darstellen zu koennen.
//
//  Moegliche Fehler: keine

DWORD __stdcall CAN_GetErrText(
        DWORD dwError,             // Zu interpretierender Fehler
        LPSTR lpszTextBuff);       // Textpuffer f�r Fehler-Text


///////////////////////////////////////////////////////////////////////////////
//  CAN_SetHwParam()
//  Setzt einen Hardware-Parameter auf einen bestimmten Wert.
//  'hHw ist ein gueltiges Hardware-Handle
//  'wParam' ist CAN_PARAM_*
//  'dwValue' ist der zu setzende Wert des Parameters.
//  Erlaubte Parameter:
//  CAN_PARAM_BAUDRATE      long      Baudrate, als 16Bit BTR0BTR1-Code.
//                            Neueinstellung der Baudrate einer Hardware.
//                            Beinflusst andere Clients desselben Netzes!
//  CAN_PARAM_LOCATION_INFO char[250] Infotext zum Einbauort der Hardware
//  CAN_PARAM_LISTEN_ONLY   long      SJA1000 in "Listen Only" Mode
//  CAN_PARAM_HW_DEVICENR   long      USB: DeviceNr.
//  CAN_PARAM_HW_SERNR      long      USB: PEAK-Seriennr.
//  CAN_PARAM_USER_LOCATION_INFO
//                          char[250] User-Info �ber Hardware-Benutzung (USB)
//  CAN_PARAM_SELECT_LED    long      USB: LED an/aus
//  F�r den CANOpen-SDO-Mode des USB-Dongles:
//  CAN_PARAM_SDO_MODE      16bit     USB
//  CAN_PARAM_SDO_QUEUEFILL 8bit      USB

//
//  Erlaubte Parameter sind abhaengig von der CAN-Hardware!
//
//  Moegliche Fehler: NOVXD ILLHW ILLPARAMTYPE ILLPARAMVAL REGTEST

DWORD __stdcall CAN_SetHwParam(
        HCANHW hHw,
        WORD   wParam,
        DWORD  dwValue);


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetHwParam()
//  Liest einen Hardware-Parameter aus.
//  'hHw ist ein gueltiges Hardware-Handle
//  'wParam' ist CAN_PARAM_*
//  'pBuff' ist ein Zeiger auf einen Rueckgabepuffer
//  'wBuffLen' ist die Laenge des Rueckgabepuffers (nur fuer Rueckgabestrings)
//  Erlaubte Parameter:
//  CAN_PARAM_HWDRIVERNR       long      Nr. des Treibertyps (ISA, Dongle, ..)
//  CAN_PARAM_NAME             char[40]  Name der Hardware
//  CAN_PARAM_HWPORT           long      Portadresse der Harfware
//  CAN_PARAM_HWINT            long      Nr des Hardware Interrupts
//  CAN_PARAM_HWNET            long      Handle des Netzes, zu dem die Hardware
//                                       gehoert
//  CAN_PARAM_BAUDRATE         long      Baudrate, als 16Bit BTR0BTR1-Code
//  CAN_PARAM_ACCCODE_EXTENDED long      29-Bit Acc-Filter-Werte
//  CAN_PARAM_ACCMASK_EXTENDED long      (nur ID, Bits 28..0 relevant)
//  CAN_PARAM_ACCCODE_STD      long      11-Bit Acc-Filter-Werte
//  CAN_PARAM_ACCMASK_STD      long      (nur ID, Bits 10..0 relevant)
//  CAN_PARAM_ACTIVE           long      0 = Controller ist reset, 1 = am Bus
//  CAN_PARAM_XMTQUEUEFILL     long      ungesendete Messages im XmtQueue
//  CAN_PARAM_RCVMSGCNT        long      insgesamt empfangene Messages
//  CAN_PARAM_RCVBITCNT        long      insgesamt empfangene Bits
//  CAN_PARAM_XMTMSGCNT        long      insgesamt gesendete Messages
//  CAN_PARAM_XMTBITCNT        long      insgesamt gesendete Bits
//  CAN_PARAM_MSGCNT           long      insgesamt gesendete oder empfangene
//                                       Messages
//  CAN_PARAM_BITCNT           long      insgesamt gesendete oder empfangene
//                                       Bits
//  CAN_PARAM_LOCATION_INFO    char[250] Infotext zum Einbauort der Hardware
//  CAN_PARAM_HWBUS            long      Bus, an den die Hardware angeschlossen
//                                       ist
//  CAN_PARAM_HWDEVICE         long      PCI-Slot
//  CAN_PARAM_HWFUNCTION       long      PCI-Slot-function
//  CAN_PARAM_HWCONTROLLER     long      Nr. des Controllers auf der Hardware
//  CAN_PARAM_LISTEN_ONLY      long      SJA1000 in "Listen Only mode"?
//  CAN_PARAM_RCVERRFRAMES     long      Werden Error Frames als CAN-messages
//                                       empfangen?
//  CAN_PARAM_HW_DEVICENR      long      USB: DeviceNr
//  CAN_PARAM_HW_SERNR         long      USB: PEAK-Seriennr
//  CAN_PARAM_BUSLOAD          long      USB: Buslast
//  CAN_PARAM_USER_LOCATION_INFO
//                             char[250] USB: User-Info �ber Hardware-Benutzung
//  CAN_PARAM_FIRMWARE_MAJOR   long      USB: Auslesen der Firmware
//  CAN_PARAM_FIRMWARE_MINOR   long
//  CAN_PARAM_SDO_SUPPORT      long      USB: 1 = SDO-Mode moeglich
//  CAN_PARAM_ANALOG0..7       long      USB: A/D-Kanaele (nicht verwendet)
//
//  Erlaubte Parameter sind abh�ngig von der CAN-Hardware!
//
//  Moegliche Fehler: NOVXD ILLHW ILLPARAMTYPE

DWORD __stdcall CAN_GetHwParam(
        HCANHW hHw,
        WORD   wParam,
        void*  pBuff,
        WORD   wBuffLen);


///////////////////////////////////////////////////////////////////////////////
//  CAN_SetNetParam()
//  Setzt einen Net-Parameter auf einen bestimmten Wert.
//  'hNet ist ein gueltiges Netz-Handle
//  'wParam' ist CAN_PARAM_*
//  'dwValue' ist der zu setzende Wert des Parameters.
//  Erlaubte Parameter:
//  CAN_PARAM_DELAYED_MESSAGE_DISTRIBUTION
//                            long    1 = Msg erst ins Netz, nachdem HW
//                                        gesendet hat
//  CAN_PARAM_NET_MASTER      long    Client-Handle des Net-Masters
//  CAN_PARAM_SDO_NET_MASTER  long    Client-Handle des Net-Masters
//
//  Moegliche Fehler: NOVXD ILLNET ILLPARAMTYPE ILLPARAMVAL

DWORD __stdcall CAN_SetNetParam(
        HCANNET hNet,
        WORD    wParam,
        DWORD   dwValue);


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetNetParam()
//  Liest einen Netz-Parameter aus.
//  'hNet' ist ein gueltiges Netz-Handle
//  'wParam' ist CAN_PARAM_*
//  'pBuff' ist ein Zeiger auf einen Rueckgabepuffer
//  'wBuffLen' ist die Laenge des Rueckgabepuffers (nur fuer Rueckgabestrings)
//  Erlaubte Parameter:
//  CAN_PARAM_NAME             char[40]  Name des Netzes
//  CAN_PARAM_BAUDRATE         long      Baudrate, als 16Bit BTR0BTR1-Code
//  CAN_PARAM_MSGCNT           long      insgesamt transportierte Messages
//  CAN_PARAM_BITCNT           long      insgesamt transportierte Bits
//  CAN_PARAM_NETHW            long      Hardware-Handle des Netzes
//  CAN_PARAM_NETCLIENTS       char[MAX_HCANCLIENT+1]
//                                       Flag[i] <> 0: Client 'i' gehoert zum
//                                       Netz 'hNet'
//  CAN_PARAM_DELAYED_MESSAGE_DISTRIBUTION
//                             long      1 = Msg erst ins Netz, nachdem HW
//                                       gesendet hat.
//  CAN_PARAM_RCVERRFRAMES     long      1 = Error Frames empfangen, Hardware
//                                       enable
//  CAN_PARAM_NET_MASTER       long      Client-Handle des Net-Masters
//  CAN_PARAM_SDO_NET_MASTER   long      Client-Handle des SDO-Net-Masters
//
//  Moegliche Fehler: NOVXD ILLNET ILLPARAMTYPE ILLPARAMVAL

DWORD __stdcall CAN_GetNetParam(
        HCANNET hNet,
        WORD    wParam,
        void*   pBuff,
        WORD    wBuffLen);


///////////////////////////////////////////////////////////////////////////////
//  CAN_SetClientParam()
//  Setzt einen Client-Parameter auf einen bestimmten Wert.
//  'hClient' ist ein gueltiges Client-Handle
//  'wParam' ist CAN_PARAM_*
//  'dwValue' ist der zu setzende Wert des Parameters.
//  Erlaubte Parameter:
//  CAN_PARAM_ONRCV_EVENT_HANDLE long     Applikation-Handle des ONRCV-Events
//  CAN_PARAM_ONRCV_EVENT_PULSE  long     1 = PulseEvent, 0 = SetEvent
//  CAN_PARAM_SELF_RECEIVE       long     1 = Eigene Sende-Msgs empfangen
//  CAN_PARAM_RCVERRFRAMES       long     1 = Error Frames empfangen
//  CAN_PARAM_EXACT_11BIT_FILTER long     1 = Exakte Filterung fuer 11Bit-
//                                        Messages
//  CAN_PARAM_SDO_RECEIVE        long     1 = SDO_Status wird empfangen
//  CAN_PARAM_MARK_SELFRECEIVED_MSG_WITH_MSGTYPE
//                               long     0 = Altes SelfReceive-Verhalten
//                                        1 = MSGTYPE_SELFRECEIVE
//  CAN_PARAM_ACCFILTER_COUNT    long     2 = Zwei Akzeptanzfilter-Terme,
//                                        ansonsten nur einer (Default)
//
//  Moegliche Fehler: NOVXD ILLHW ILLPARAMTYPE ILLPARAMVAL

DWORD __stdcall CAN_SetClientParam(
        HCANCLIENT  hClient,
        WORD        wParam,
        DWORD       dwValue);


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetClientParam()
//  Liest einen Client-Parameter aus.
//  'hClient' ist ein gueltiges Client-Handle
//  'wParam' ist CAN_PARAM_*
//  'pBuff' ist ein Zeiger auf einen Rueckgabepuffer
//  'wBuffLen' ist die Laenge des Rueckgabepuffers (nur fuer Rueckgabestrings)
//  Erlaubte Parameter:
//  CAN_PARAM_NAME               char[40] Name des Clients
//  CAN_PARAM_ACCCODE_EXTENDED   long     29-Bit Acc-Filter-Werte
//  CAN_PARAM_ACCMASK_EXTENDED   long     (nur ID, Bits 28..0 relevant)
//  CAN_PARAM_ACCCODE_STD        long     11-Bit Acc-Filter-Werte
//  CAN_PARAM_ACCMASK_STD        long     (nur ID, Bits 10..0 relevant)
//  CAN_PARAM_RCVQUEUESIZE       long     Groesse der RcvQueue
//  CAN_PARAM_RCVQUEUEFILL       long     ungelesene Messages im RcvQueue
//  CAN_PARAM_XMTQUEUESIZE,
//  CAN_PARAM_DELAYXMTQUEUESIZE  long     Groesse der DelayXmtQueue
//  CAN_PARAM_XMTQUEUEFILL,
//  CAN_PARAM_DELAYXMTQUEUEFILL  long     ungesendete Messages im DelayXmtQueue
//  CAN_PARAM_RCVMSGCNT          long     insgesamt empfangene Messages
//  CAN_PARAM_RCVBITCNT          long     insgesamt empfangene Bits
//  CAN_PARAM_XMTMSGCNT          long     insgesamt gesendete Messages
//  CAN_PARAM_XMTBITCNT          long     insgesamt gesendete Bits
//  CAN_PARAM_MSGCNT             long     insgesamt gesendete oder empfangene
//                                        Messages
//  CAN_PARAM_BITCNT             long     insgesamt gesendete oder empfangene
//                                        Bits
//  CAN_PARAM_HWND               long     Fenster-Handle der Client-App.
//  CAN_PARAM_CLNETS             char[MAX_HCANNET+1]
//                                        Flag[i] <> 0: Netz 'i' gehoert zum
//                                        Client 'hClient'
//  CAN_PARAM_ONRCV_EVENT_HANDLE long     Applikation-Handle des ONRCV-Events
//  CAN_PARAM_ONRCV_EVENT_PULSE  long     1 = PulseEvent, 0 = SetEvent
//  CAN_PARAM_SELF_RECEIVE       long     1 = Empfaengt eigene Sende-Msgs
//  CAN_PARAM_RCVERRFRAMES       long     1 = Empfaengt Error Frames
//  CAN_PARAM_EXACT_11BIT_FILTER long     1 = Exakte Filterung fuer 11Bit-
//                                        Messages
//  CAN_PARAM_SDO_RECEIVE        long     1 = SDO_Status wird empfangen
//  CAN_PARAM_MARK_SELFRECEIVED_MSG_WITH_MSGTYPE
//                               long     0 = Altes SelfReceive-Verhalten
//                                        1 = MSGTYPE_SELFRECEIVE
//  CAN_PARAM_ACCFILTER_COUNT    long     2 = Zwei Akzeptanzfilter-Terme,
//                                        ansonsten nur einer (Default)
//
//  Moegliche Fehler: NOVXD ILLHW ILLPARAMTYPE ILLPARAMVAL

DWORD __stdcall CAN_GetClientParam(
        HCANCLIENT hClient,
        WORD       wParam,
        void*      pBuff,
        WORD       wBuffLen);



///////////////////////////////////////////////////////////////////////////////
//  CAN_VersionInfo()
//  Gibt einen Textstring mit Version- und Copyrightinfo
//  zurueck (max. 255 Zeichen).
//
//  Moegliche Fehler: NOVXD

DWORD __stdcall CAN_VersionInfo(LPSTR lpszTextBuff);


///////////////////////////////////////////////////////////////////////////////
//  CAN_SetDriverParam()
//  Setzt einen Treiber-Parameter auf einen bestimmten Wert.
//  'wParam' ist CAN_PARAM_*
//  'dwValue' ist der zu setzende Wert des Parameters.
//  Erlaubte Parameter:
//  CAN_PARAM_UNLOCKCODE        long    Code, um bestimmte Treiber-Fatures
//                                      freizuschalten (nicht verwendet)
//  CAN_PARAM_ISRTIMEOUT        long    Laufzeitgrenze in Mikrosekunden fuer ISR
//  CAN_PARAM_USBACTIVATEDELAY  long    USB: Wartepause in ms nach hw_activate()
//  CAN_PARAM_TIMERFIX          long    <> 0: aktiviert die Korrektur von
//                                      PerformanceCounter
//  CAN_PARAM_RCVQUEUESIZE      long    Groesse der Rcv-Puffer
//  CAN_PARAM_XMTQUEUESIZE      long    Groesse der Xmt-Puffer
//  CAN_PARAM_DELAYXMTQUEUESIZE long    Groesse der Xmt-Puffer
//
//  Moegliche Fehler: NOVXD ILLPARAMTYPE ILLPARAMVAL

DWORD __stdcall CAN_SetDriverParam(
        WORD  wParam,
        DWORD dwValue);


///////////////////////////////////////////////////////////////////////////////
//  CAN_GetDriverParam()
//  Liest einen Treiber-Parameter aus.
//  'wParam' ist CAN_PARAM_*
//  'pBuff' ist ein Zeiger auf einen Rueckgabepuffer
//  'wBuffLen' ist die Laenge des Rueckgabepuffers (nur fuer Rueckgabestrings)
//  Erlaubte Parameter:
//  CAN_PARAM_UNLOCKCODE        long      Code, um bestimmte Treiberfeatures
//                                        freizuschalten (nicht verwendet)
//  CAN_PARAM_ISRTIMEOUT        long      Laufzeitgrenze in Mikrosekunden fuer ISR
//  CAN_PARAM_DRIVERTYPE        long      System: 9x/NT/WDM ?
//  CAN_PARAM_RCVQUEUESIZE      long      Groesse der Rcv-Puffer
//  CAN_PARAM_XMTQUEUESIZE      long      Groesse der Xmt-Puffer
//  CAN_PARAM_DELAYXMTQUEUESIZE long      Groesse der Xmt-Puffer
//  CAN_PARAM_FCPU              long      CPU-Frequenz in kHz (erst 1 Sek. nach
//                                        CAN_Init(), nur NT/WDM)
//  CAN_PARAM_TIMERFIX          long      <> 0: die Korrektur von
//                                        PerformanceCounter ist aktiviert
//
//  Moegliche Fehler: NOVXD ILLPARAMTYPE ILLPARAMVAL

DWORD __stdcall CAN_GetDriverParam(
        WORD  wParam,
        void* pBuff,
        WORD  wBuffLen) ;


///////////////////////////////////////////////////////////////////////////////
//  CAN_RegisterMemory() - Non-paged Memory-Block allokieren
//  CAN_GetMemory()      - Memory-Handle in Zeiger umwandeln
//  CAN_RemoveMemory()   - Freigabe des Memory-Blocks
//
//  Moegliche Fehler: NOVXD RESOURCE

DWORD __stdcall CAN_RegisterMemory(
        DWORD dwSize,
        DWORD dwFlags,
        HCANMEM *hMem);

DWORD __stdcall CAN_GetMemory(
        HCANMEM hMem,
        PVOID *memptr);

DWORD __stdcall CAN_RemoveMemory(HCANMEM hMem);


int canapi2_init(void);
void canapi2_close(void);

#ifdef __cplusplus
}
#endif

#endif // __CANAPI2H__
