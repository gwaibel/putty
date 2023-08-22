/*
 * CAN back end (Windows-specific).
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "putty.h"
#include "canapi2.h"

typedef struct PCAN PCAN;
struct PCAN {
    Seat *seat;
    LogContext *logctx;
    HCANCLIENT client;
    HCANNET net;
    HCANHW hw;
    char *netname;
    int bitrate;
    DWORD32 rxid;
    DWORD32 txid;
    HANDLE rxthread;
    HANDLE rxevent;
    bool rxthread_running;
    Backend backend;
    DWORD xmtqueue_size;
};



/*
 * Clear the CAN bus-off condition.
 */
static void pcan_clear_busoff(PCAN *pcan)
{
    DWORD err;

    /* Try fast buson
     * reason: on PCAN-USB the regular ResetHardware takes a rather long time
     * -> simply set the buson again
     */
    err = CAN_SetHwParam(pcan->hw, CAN_PARAM_BUSON, 1);
    if (err != CAN_ERR_OK) {
        /* probably not available on this driver
         * -> try hard reset (will be fast enough on PCI cards)
         */
        CAN_ResetHardware(pcan->hw);
   }
   Sleep(100);  /* Give hardware some time to reset */
}

/*
 * Read a CAN message and check if it's for us.
 * Return true if a message has been read successfully.
 */
static bool pcan_read_message(PCAN *pcan, TCANMsg *msg)
{
    DWORD cerr;
    HCANNET net;
    TCANTimestamp ts;
    bool gotmsg = false;
    bool abort = false;

    while (!abort) {
        cerr = CAN_Read(pcan->client, msg, &net, &ts);
        if (cerr == CAN_ERR_OK) {
            // Got a message, check if it is for us
            if ((msg->MSGTYPE == MSGTYPE_STANDARD) || (msg->MSGTYPE == MSGTYPE_EXTENDED)) {
                // Check id
                DWORD32 id = msg->ID | ((msg->MSGTYPE == MSGTYPE_EXTENDED) ? 0x80000000u : 0u);

                if (id == pcan->rxid) {

                    gotmsg = true;
                    abort = true;
                }
            }
            else if (msg->MSGTYPE == MSGTYPE_STATUS) {
                // Handle busoff
                DWORD status = *((DWORD *)&msg->DATA[0]);
                if (status & CAN_ERR_BUSOFF)
                {
                    logeventf(pcan->logctx, "CAN bus-off, trying to restart...");
                    pcan_clear_busoff(pcan);
                }
            } else {
                // Unexpected message type, dump silently
            }
        } else if (cerr == CAN_ERR_QRCVEMPTY) {
            // No message available
            abort = true;
        } else {
            // Unexpected error
            abort = true;
        }
    }

    return gotmsg;
}

/*
 * Read all available CAN messages an forward the data to the frontend.
 * Function called from main thread, triggered by pcan->rxevent from pcan_rx_thread.
 */
static void pcan_rxevent_callback(void *vctx)
{
    PCAN *pcan = (PCAN*)vctx;
    TCANMsg msg;
    size_t rest;

    while (pcan_read_message(pcan, &msg)) {
        rest = seat_stdout(pcan->seat, &msg.DATA, msg.LEN);

        /* TODO: for now we don't expect that the frontend rejects our data */
        if (rest > 0)
            logeventf(pcan->logctx, "Uuups, frontend rejected received data (len=%u, rest=%u).", msg.LEN, rest);
    }
}

/*
 * Poll the CAN interface for incoming messages and inform main thread if some are available.
 */
static DWORD WINAPI pcan_rx_thread(void *param)
{
    PCAN *pcan = (PCAN*)param;
    DWORD err;
    DWORD fill;

    while(pcan->rxthread_running) {
        /* Inform main thread if a CAN message is available for reading */
        err = CAN_GetClientParam(pcan->client, CAN_PARAM_RCVQUEUEFILL, &fill, sizeof(fill));
        if (err == CAN_ERR_OK) {
            if (fill > 0) {
                SetEvent(pcan->rxevent);
            }
        }
        else
            assert(0);

        Sleep(10);
    }

    return 0;
}

static int pcan_get_config(PCAN *pcan, Conf *conf)
{
    /* Get CAN configuration string and split into pieces (netname rxid txid, space separated)*/
    char *scfg = dupstr(conf_get_str(conf, CONF_pcan));
    char *snet = strtok(scfg, " ,;:");
    char *srxid = strtok(NULL, " ,;:");
    char *stxid = strtok(NULL, " ,;:");

    if (snet == NULL || srxid == NULL || stxid == NULL) {
        sfree(scfg);
        return -1;
    }

    pcan->netname = dupstr(snet);
    pcan->rxid = (DWORD32)strtoul(srxid, NULL, 0);
    pcan->txid = (DWORD32)strtoul(stxid, NULL, 0);
    sfree(scfg);

    pcan->bitrate = conf_get_int(conf, CONF_pcanbitrate);

    return 0;
}

static char* pcan_get_application_name(void)
{
    int len;
    int pos = 0;
    char name[1024];

    //get application filename to report to PEAK driver
    GetModuleFileNameA(GetModuleHandle(0), name, sizeof(name));
    //get only exe name (find last '\' and take everything from there)
    len = strlen(name);
    for (int i = len; i >= 0; i--)
    {
        if (name[i] == '\\')
        {
            pos = i + 1;
            break;
        }
    }
    //remove extension "."
    for (int i = len; i >= pos; i--)
    {
        if (name[i] == '.')
        {
            name[i] = '\0'; //terminate string
            break;
        }
    }

    return dupstr(&name[pos]);
}

static bool pcan_find_net(char *netname)
{
    char buf[MAX_DRIVERNAMELEN];
    bool found = false;

    for (int i = 1; i <= MAX_HCANNET; i++) {
        if (CAN_GetNetParam((HCANNET)i, CAN_PARAM_NAME, buf, sizeof(buf)) == CAN_ERR_OK)
        {
            if (strcmp(buf, netname) == 0) {
                found = true;
                break;
            }
        }
        /* there might be gaps in the index list -> don't break! */
    }

    return found;
}

static int pcan_calc_bitrate_reg_value(int bitrate)
{
    #define N_BITR              13
    #define PEAK2_CAN_BAUD_40   0x492F //T1 = 17; T2 =  3; SP = 85%; SJW = 2
    #define PEAK2_CAN_BAUD_200  0x815C //T1 = 14; T2 =  6; SP = 70%; SJW = 3
    #define PEAK2_CAN_BAUD_400  0x804D //T1 = 15; T2 =  5; SP = 75%; SJW = 3
    #define PEAK2_CAN_BAUD_800  0x0016 //T1 =  8; T2 =  2; SP = 80%; SJW = 1
    const struct {
        char text[5];
        int br;     //in kbit/s
        int btr;    //bit-timing register
    } bitrates[N_BITR] = {
            { "5",       5, CAN_BAUD_5K        },
            { "10",     10, CAN_BAUD_10K       },
            { "20",     20, CAN_BAUD_20K       },
            { "40",     40, PEAK2_CAN_BAUD_40  },
            { "50",     50, CAN_BAUD_50K       },
            { "100",   100, CAN_BAUD_100K      },
            { "125",   125, CAN_BAUD_125K      },
            { "200",   200, PEAK2_CAN_BAUD_200 },
            { "250",   250, CAN_BAUD_250K      },
            { "400",   400, PEAK2_CAN_BAUD_400 },
            { "500",   500, CAN_BAUD_500K      },
            { "800",   800, PEAK2_CAN_BAUD_800 },
            { "1000", 1000, CAN_BAUD_1M        }
        };

    for (int i = 0; i < N_BITR; i++) {
        if (bitrates[i].br == bitrate) {
            return bitrates[i].btr;
        }
    }

    return -1;
}

/*
 * Called to set up the serial connection.
 *
 * Returns an error message, or NULL on success.
 *
 * Also places the canonical host name into `realhost'. It must be
 * freed by the caller.
 */
static char *pcan_init(const BackendVtable *vt, Seat *seat,
                         Backend **backend_handle, LogContext *logctx,
                         Conf *conf, const char *host, int port,
                         char **realhost, bool nodelay, bool keepalive)
{
    int ret;
    char *err;
    DWORD cerr;
    PCAN *pcan;
    int btr;
    char *clientname;

    /* No local authentication phase in this protocol */
    seat_set_trust_status(seat, false);

    pcan = snew(PCAN);
    memset(pcan, 0, sizeof(PCAN));
    pcan->seat = seat;
    pcan->backend.vt = vt;
    pcan->logctx = logctx;
    *backend_handle = &pcan->backend;

    /* Load Peak CAN driver DLL */
    ret = canapi2_init();
    if (ret != 0) {
        err = dupprintf("Could not open PEAK canapi2.dll (PEAK-CAN driver not installed?)");
        return err;
    }

    /* Get and parse configuration parameters into pcan struct */
    ret = pcan_get_config(pcan, conf);
    if (ret != 0) {
        err = dupprintf("Invalid configuration! (Use: 'netname rxid txid')");
        return err;
    }

    logeventf(pcan->logctx, "Opening CAN device %s", pcan->netname);
    logeventf(pcan->logctx, "RX-id=0x%x, TX-id=0x%x", pcan->rxid, pcan->txid);

    /* Register client at driver and remember the client handle */
    clientname = pcan_get_application_name();
    cerr = CAN_RegisterClient(clientname, 0u, &pcan->client);
    sfree(clientname);
    if (cerr != CAN_ERR_OK) {
        err = dupprintf("Could not register to PEAK CAN driver");
        return err;
    }

    /* Connect client to network and remember network handle */
    cerr = CAN_ERR_ILLNET;
    if (pcan_find_net(pcan->netname)) {
        cerr = CAN_ConnectToNet(pcan->client, pcan->netname, &pcan->net);
    }
    if (cerr != CAN_ERR_OK) {
        err = dupprintf("Could not register to CAN network '%s' (not existing?)", pcan->netname);
        return err;
    }

    /* Get CAN hardware handle for selected CAN network */
    CAN_GetNetParam(pcan->net, CAN_PARAM_NETHW, &pcan->hw, sizeof(pcan->hw));

    /* stwpeak2.dll does a CAN HW reset here. But why should we od this? */


    /* Set RX filter */
    cerr = CAN_SetClientParam(pcan->client, CAN_PARAM_EXACT_11BIT_FILTER, 1);
    if (cerr == CAN_ERR_OK) {
        TCANMsg msg = {
            .ID = pcan->rxid & 0x7FFFFFFFFu,
            .MSGTYPE = (pcan->rxid & 0x80000000u) ? MSGTYPE_EXTENDED : MSGTYPE_STANDARD,
        };
        cerr = CAN_RegisterMsg(pcan->client, pcan->net, &msg, &msg);
    }
    if (cerr != CAN_ERR_OK) {
        err = dupprintf("Could not set CAN RX filter");
        return err;
    }

    /* Set bitrate (leave as is of 0 is specified) */
    if (pcan->bitrate > 0) {
        cerr = CAN_ERR_ILLPARAMVAL;
        btr = pcan_calc_bitrate_reg_value(pcan->bitrate);
        if (btr > 0) {
            cerr = CAN_SetHwParam(pcan->hw, CAN_PARAM_BAUDRATE, btr);
        }
        if (cerr != CAN_ERR_OK) {
            err = dupprintf("Could not set CAN bitrate (%ikbps)", pcan->bitrate);
            return err;
        }
    }

    /*
     * Copied from stwpeak2.dll, don't know if actually required!
     * Activate performance timer fix if applicable.
     * ignore result: error if either no USB interface or old driver version (<2.46)
     */
    (void)CAN_SetClientParam(pcan->client, CAN_PARAM_TIMERFIX, 1);

    /*
     * Register RX event in PEAK driver => No, TODO:
     * cerr = CAN_SetClientParam(pcan->client, CAN_PARAM_ONRCV_EVENT_HANDLE, (DWORD)pv_RcvEvent);
     * The canapi2 API seems not to be 64-bit aware (sizeof(DWORD)=32bit, but sizeof(HANDLE)=64bit)
     * => We use polling mode for rx data
     *
     * Create RX event callback and RX thread
     */
    pcan->rxthread_running = true;
    pcan->rxevent = CreateEvent(NULL, false, false, NULL);
    add_handle_wait(pcan->rxevent, pcan_rxevent_callback, pcan);
    pcan->rxthread = CreateThread(NULL, 0, &pcan_rx_thread, pcan, 0, NULL);
    if ((pcan->rxevent == NULL) || (pcan->rxthread == NULL)) {
        err = dupprintf("Could not create RX thread or event");
        return err;
    }

    /* Get XMIT queue size */
    (void)CAN_GetClientParam(pcan->client, CAN_PARAM_XMTQUEUESIZE, &pcan->xmtqueue_size, sizeof(pcan->xmtqueue_size));

    *realhost = dupstr(pcan->netname);

    /*
     * Specials are always available.
     */
    seat_update_specials_menu(pcan->seat);

    return NULL;
}

static void pcan_terminate(PCAN *pcan)
{
    pcan->rxthread_running = false;
    if (pcan->rxthread != NULL) {
        WaitForSingleObject(pcan->rxthread, INFINITE);
        pcan->rxthread = NULL;
    }
    CloseHandle(pcan->rxevent);

    CAN_DisconnectFromNet(pcan->client, pcan->net);
    CAN_RemoveClient(pcan->client);
}

static void pcan_free(Backend *be)
{
    PCAN *pcan = container_of(be, PCAN, backend);

    logeventf(pcan->logctx, "Closing CAN device %s", pcan->netname);

    /*
     * TODO:
     * pcan_free is not called when closing the session. Thus the CAN client is not cleaned up, which ist ugly.
     * Why is pcan_free not called by the frontend?
     */

    pcan_terminate(pcan);
    expire_timer_context(pcan); // required??
    if (pcan->netname != NULL)
        sfree(pcan->netname);
    sfree(pcan);
    canapi2_close();
}

static void pcan_reconfig(Backend *be, Conf *conf)
{
    PCAN *pcan = container_of(be, PCAN, backend);

    /*
     * TODO: What shall we do here??
     */
    logeventf(pcan->logctx, "Uuups, reconfig called, but what shall we do here?");
}


/*
 * Send a CAN message.
 * This function only accepts up to 8 byte of data
 */
static int pcan_send_msg(PCAN *pcan, const char *buf, size_t len)
{
    DWORD err;
    TCANMsg msg;
    TCANTimestamp send_time = { 0 };

    assert(len <= 8);

    msg.MSGTYPE = (pcan->txid & 0x80000000u) ? MSGTYPE_EXTENDED : MSGTYPE_STANDARD;
    msg.ID = (pcan->txid & ~0x80000000u);
    msg.LEN = len;
    memcpy(msg.DATA, buf, len);

    err = CAN_Write(pcan->client, pcan->net, &msg, &send_time);

    return (err == CAN_ERR_OK) ? 0 : -1;
}

/*
 * Called to send data down the connection.
 */
static void pcan_send(Backend *be, const char *buf, size_t len)
{
    PCAN *pcan = container_of(be, PCAN, backend);
    int err;
    size_t now;
    size_t rest = len;
    size_t sent = 0u;

    while (rest > 0u) {
        now = (rest > 8u) ? 8u : rest;
        err = pcan_send_msg(pcan, &buf[sent], now);
        if (err)
            break;

        sent += now;
        rest -= now;
    }

    if (err) {
        logeventf(pcan->logctx, "Uuups, transmit buffer overflow (only sent %u from %u bytes)", sent, len);
        /* Dump TX error silently.
         * TODO: store data to a tx queue and retry sending from a txthread
         */
    }
}

/*
 * Called to query the current sendability status.
 */
static size_t pcan_sendbuffer(Backend *be)
{
    PCAN *pcan = container_of(be, PCAN, backend);
    DWORD err;
    DWORD xmtqueue_fill;
    size_t ret;

    err = CAN_Status(pcan->hw);
    if (err == CAN_ERR_ILLHW) {
        /* illegal HW handle => this is propably a vcan interface. Assume it is always ok */
        err = CAN_ERR_OK;
    }

    if (err & CAN_ERR_BUSOFF) {
        logeventf(pcan->logctx, "CAN bus-off, trying to restart...");
        pcan_clear_busoff(pcan);
        ret = 0;
    }
    else if (err != CAN_ERR_OK) {
        ret = 0;
    }
    else {
        (void)CAN_GetClientParam(pcan->client, CAN_PARAM_XMTQUEUEFILL, &xmtqueue_fill, sizeof(xmtqueue_fill));
        ret = (pcan->xmtqueue_size - xmtqueue_fill) * 8u;
    }

    return ret;
}

/*
 * Called to set the size of the window
 */
static void pcan_size(Backend *be, int width, int height)
{
    /* Do nothing! */
    return;
}

/*
 * Send special codes.
 */
static void pcan_special(Backend *be, SessionSpecialCode code, int arg)
{
    return;
}

/*
 * Return a list of the special codes that make sense in this
 * protocol.
 */
static const SessionSpecial *pcan_get_specials(Backend *be)
{
    static const SessionSpecial specials[] = {
        /* e.g: {"Break", SS_BRK}, */
        {NULL, SS_EXITMENU}
    };
    return specials;
}

static bool pcan_connected(Backend *be)
{
    return true;                       /* always connected */
}

static bool pcan_sendok(Backend *be)
{
    size_t buf = pcan_sendbuffer(be);

    return (buf > 0) ? true : false;
}

static void pcan_unthrottle(Backend *be, size_t backlog)
{
}

static bool pcan_ldisc(Backend *be, int option)
{
    /*
     * Local editing and local echo are off by default.
     */
    return false;
}

static void pcan_provide_ldisc(Backend *be, Ldisc *ldisc)
{
    /* This is a stub. */
}

static int pcan_exitcode(Backend *be)
{
    /* Exit codes are a meaningless concept with this backend */
    return -1;
}

/*
 * cfg_info for CAN does nothing at all.
 */
static int pcan_cfg_info(Backend *be)
{
    return 0;
}

const BackendVtable pcan_backend = {
    .init = pcan_init,
    .free = pcan_free,
    .reconfig = pcan_reconfig,
    .send = pcan_send,
    .sendbuffer = pcan_sendbuffer,
    .size = pcan_size,
    .special = pcan_special,
    .get_specials = pcan_get_specials,
    .connected = pcan_connected,
    .exitcode = pcan_exitcode,
    .sendok = pcan_sendok,
    .ldisc_option_state = pcan_ldisc,
    .provide_ldisc = pcan_provide_ldisc,
    .unthrottle = pcan_unthrottle,
    .cfg_info = pcan_cfg_info,
    .id = "PCAN",
    .displayname_tc = "PCAN",
    .displayname_lc = "PCAN",
    .protocol = PROT_PCAN,
    .flags = 0,
};
