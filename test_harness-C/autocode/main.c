// Autocode command line: -c -sm Simple1 ../Simple.mdxml
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <qep_port.h>
#include <qassert.h>
#include <log_event.h>

#include <Simple1.h>
#include <Simple1Impl.h>
#include <StatechartSignals.h>

Q_DEFINE_THIS_FILE;

#define MYQUEUESIZE 10       /* Queue size */
#define USER_ENTRY_SIZE 256  /* Number characters on inline prompt */
#define EVENT_SIZE 16        /* Array size for data of Medium Event */

/* Use QEvent as type of the small-size event poll */
typedef QEvent SmallEvent;
/* Define an event type with one piece of data for the medium-size event pool */
typedef struct MediumEvent {
    QEvent super;
    uint32_t data[EVENT_SIZE];
} MediumEvent;
#define MEDIUM_EVT_DATA_LIMIT EVENT_SIZE*sizeof(uint32_t)
/* Define a 256-byte event type for the large-size event pool */
typedef struct LargeEvent {
    QEvent super;
    uint32_t data[EVENT_SIZE*4];
} LargeEvent;
#define LARGE_EVT_DATA_LIMIT EVENT_SIZE*4*sizeof(uint32_t)

QSubscrList l_subscrSto[MAX_SIG]; /* publish-subscribe storage */
SmallEvent l_evPoolSm[MYQUEUESIZE];   /* small event pool: signal only */
MediumEvent l_evPoolMed[MYQUEUESIZE]; /* medium event pool: SIG+data */
LargeEvent l_evPoolLarge[2]; /* large event pool: lots of data */
const QEvent *test_queuestorage0[MYQUEUESIZE];

// Declare first State Machine Impls, and then the active objects
struct ImplObjects {
    Simple1Impl Simple1_impl;
} implObjs;

/**
 * Sets the given named guard attribute of given StateMachine name to
 * the designated True or False string value.
 */
void setGuardAttribute (const char *sm, const char *attr, const char *val) {
    printf("Got sm '%s', attr name '%s', and value '%s'\n", sm, attr, val);
    if (strcasecmp(sm, "Simple1") == 0) {
        AttributeMapper_set(&(implObjs.Simple1_impl), attr, AttributeMapper_strtobool(val));
    }
}

struct ActiveObjects {
    Simple1 Simple1;
} activeObjs;

/**
 * Stops and re-starts StateMachine active object of given name.
 */
void initSM (const char *sm) {
    if (strcasecmp(sm, "Simple1") == 0) {
        Simple1_reinit(&(activeObjs.Simple1));  // set StateMachine to 'initial' state
        QF_ACTIVE_INIT_((QHsm *)&(activeObjs.Simple1), (QEvent *)0);
    }
}


////////////////////////////////////////////////////////////////////////////////
// @fn receiveCmd()
// @brief Read the next command from stdin as well as from xmlrpc, if any
// @param cmdBuf char* for storing the read command
// @return None
////////////////////////////////////////////////////////////////////////////////
void receiveCmd(char *cmdBuf) {
#ifdef DEFINE_XMLRPC
    LogEvent_read(cmdBuf);
#else /* DEFINE_XMLRPC */
    scanf("%s", cmdBuf);
#endif /* DEFINE_XMLRPC */
}

void runQF() {
    char cmdBuf[USER_ENTRY_SIZE];
    MediumEvent *event;
    char *word;

    for (;;) {
        // Get the incoming string command from the dmsTerminal or the GUI
        receiveCmd(cmdBuf);
        printf("Got command: %s!\n", cmdBuf);

        // Attempt to parse the command string for IMPL or ACTIVE calls
        char tmpBuf[USER_ENTRY_SIZE];
        char smName[USER_ENTRY_SIZE];
        char attrName[USER_ENTRY_SIZE];
        char valStr[USER_ENTRY_SIZE];
        // scan for IMPL['SM'].set("attr", True/False), allowing blanks
        int cnt = sscanf(strcpy(tmpBuf, cmdBuf),
                "IMPL[%*['\"]%[^'\"]%*['\"]].set(%*['\"]%[^'\"]%*['\"],%[^)])",
                smName, attrName, valStr);
        if (cnt > 0) {  // found an IMPL attribute setter!
            setGuardAttribute(smName, attrName, valStr);
            continue;
        }
        // next, attempt scan for ACTIVE['SM'].onStart(ACTIVE['SM'].top)
        cnt = sscanf(strcpy(tmpBuf, cmdBuf),
                "ACTIVE[%*['\"]%[^'\"]%*['\"]].onStart(ACTIVE[%*['\"]%*[^'\"]%*['\"]].top)",
                smName);
        if (cnt > 0) {
            initSM(smName);
            continue;
        }

        word = strtok(cmdBuf, " ");
        if (strcasecmp(word, "quit") == 0) {  // quit the program
            QF_stop();
            return;
        }

        // We assume the first word contains the signal that is to be published,
        // and the remaining words are data to be used to populate the event.
        int signal = strtoul(word, NULL, 10);
        if (signal == DURING) {
            QF_tick();
        } else {
            event = Q_NEW(MediumEvent, signal);
            // Loop through the remaining words and populate the event
            int i = 0;
            do {
                word = strtok(NULL, " ");
                if (word) {
                    Q_ASSERT(i<EVENT_SIZE);
                    event->data[i] = strtoul(word, NULL, 16);
                } else {  // set data to zeros
                    event->data[i] = 0;
                }
                i = i + 1;
            } while (word);
            // let's zero out the remaining data spaces to avoid bad data
            for (; i < EVENT_SIZE; ++i) {
                event->data[i] = 0;
            }
            printf("Publishing event with signal %d...\n", ((QEvent *)event)->sig);
            QF_publish((QEvent *)event);
        }
        // hand control to QF for one round of publish
        QF_run();
    }

    return;
}


/*.................................................................*/
void Q_onAssert(char const Q_ROM * const Q_ROM_VAR file, int line) {
    fprintf(stderr, "Assertion failed in %s, line %d!\n", file, line);
    QF_stop();
}

void QF_onStartup(void) {  /* qvanilla: nothing to do */  }

void QF_onCleanup(void) {
    printf("\nBye bye!\n");
    LogEvent_clean();
}
/*.................................................................*/


int main(int argc, char *argv[]) {
    int xmlrpcPort = LogEvent_defaultPort();

#ifdef DEFINE_XMLRPC
    if (argc > 2) {  // look for port
        if (0 == strcmp("-p", argv[1])) {
            xmlrpcPort = atoi(argv[2]);
        }
    }

#endif /* DEFINE_XMLRPC */
    // initialize logging, including any GUI connection
    LogEvent_init(xmlrpcPort);

    printf("Quantum Framework Test\nQEP %s\nQF  %s, QF/Linux port %s\n",
            QEP_getVersion(),
            QF_getVersion(), QF_getPortVersion());

    QF_init();  // initialize framework
    // initialize publish-subscribe
    QF_psInit(l_subscrSto, Q_DIM(l_subscrSto));
    // initialize all 3 event pools, small, medium, and large
    QF_poolInit(l_evPoolSm, sizeof(l_evPoolSm), sizeof(l_evPoolSm[0]));
    QF_poolInit(l_evPoolMed, sizeof(l_evPoolMed), sizeof(l_evPoolMed[0]));
    QF_poolInit(l_evPoolLarge, sizeof(l_evPoolLarge), sizeof(l_evPoolLarge[0]));

    // instantiate and start the State Machines
    Simple1Impl_Constructor(&implObjs.Simple1_impl);  // initialize impl obj
    Simple1_Constructor(&(activeObjs.Simple1), "Simple1", &implObjs.Simple1_impl, (QActive *) 0 /*no parent*/);
    QActive_start((QActive *) &(activeObjs.Simple1), 1, test_queuestorage0, Q_DIM(test_queuestorage0), (void *)0, 0, (QEvent *)0);

    // execute interactive loop
    runQF();

    return 0;
}
