/**
 * @file main.cpp
 * @brief This file provides the main() function and the the main thread functionality for the front end. The code here does the following:
 *        1. Sets up sensor head thread(s) to receive video frames from Video for Linux and send them to Raw to Depth
 *        2. Sets up a TCP server listening on the local port (defaults to 1234) to allow the Python code to control the front end
 *        3. Communicates between the TCP server and the sensor head threads
 *        4. Coordinates an orderly shutdown on a signal or when the sensor head threads fail or die
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved
 */

#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <getopt.h>
#include <string>
#include <sstream>
#include <thread>
#include <array>
#include <memory>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include "frontend.h"
#include "V4LSensorHeadThread.h"
#include "MockSensorHeadThread.h"
#include "LumoLogger.h"
#include "TimeSync.h"

constexpr unsigned int MAX_HEADS            { 1 };
constexpr unsigned int READ_TIMEOUT_MSEC    { 2000 };
constexpr unsigned int VIDEO_DEVICE         { 3 };
constexpr unsigned int BASE_FPGA_I2C_ADDR   { 0x10 };

#define LOGMASK_WITHOUT_DEBUG (LOG_MASK(LOG_WARNING) | LOG_MASK(LOG_INFO) | LOG_MASK(LOG_ERR))
#define LOGMASK_WITH_DEBUG (LOGMASK_WITHOUT_DEBUG | LOG_MASK(LOG_DEBUG))

static int s_numHeads = MAX_HEADS;
static std::array<std::shared_ptr<SensorHeadThread>, MAX_HEADS> s_shThreads {};
static int s_listenFd = -1;
static int s_signalFd = -1;
static int s_exitFd = -1;

/**
 * @brief Internal function to translate control bytes from socket to thread control bytes. See the README.md file for more details.
 *        Note that this function may synchronize the time
 *
 * @param controlByte Control byte from the control port socket
 *
 * @return control byte sent to the thread
 */
uint8_t translateControlByte(unsigned int controlByte)
{
    unsigned int retVal = THR_NOTIFY_NOTHING_HAPPENED;
    if ((controlByte & FEC_NOTIFY_START_STREAMING_MASK) == FEC_NOTIFY_START_STREAMING) { // start streaming
        retVal = THR_NOTIFY_START_STREAMING | ((controlByte & FEC_NOTIFY_FORMAT_MASK) >> FEC_NOTIFY_FORMAT_SHIFT); // b00FFFFHH -> b0000FFFF
    } else if ((controlByte & FEC_NOTIFY_STOP_STREAMING_MASK) == FEC_NOTIFY_STOP_STREAMING) {
        retVal = THR_NOTIFY_STOP_STREAMING;
    } else if ((controlByte & FEC_NOTIFY_RELOAD_CAL_DATA_MASK) == FEC_NOTIFY_RELOAD_CAL_DATA) {
        retVal = THR_NOTIFY_RELOAD_CAL_DATA;
    } else if ((controlByte & FEC_NOTIFY_START_RAW_STREAMING_MASK) == FEC_NOTIFY_START_RAW_STREAMING) {
        retVal = THR_NOTIFY_START_RAW_STREAMING;
    } else if ((controlByte & FEC_NOTIFY_SUSPEND_RAW_STREAMING_MASK) == FEC_NOTIFY_SUSPEND_RAW_STREAMING) {
        retVal = THR_NOTIFY_SUSPEND_RAW_STREAMING;
    } else if ((controlByte & FEC_NOTIFY_SYNC_TIME_MASK) == FEC_NOTIFY_SYNC_TIME) {
        unsigned int headNum = controlByte & FEC_NOTIFY_HEADNUM_MASK;
        if (headNum < s_numHeads) {
            s_shThreads[headNum]->syncTimeOnNextSession();
        }
        retVal = THR_NOTIFY_NOTHING_HAPPENED;
    } else {
        retVal = THR_NOTIFY_NOTHING_HAPPENED;
    }
    return static_cast<uint8_t>(retVal);
}

/**
 * @brief Internal function that receives a control byte from the socket of the accepted connection and forwards it to the corresponding sensor head thread
 *
 * @param connectedFd File descriptor of the accepted connection socket
 */
void handleAccept(int connectedFd)
{
    uint8_t controlByte;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(connectedFd, &fds);
    struct timeval timeout {};
    timeout.tv_sec = READ_TIMEOUT_MSEC / MILLISECONDS_PER_SECOND;
    timeout.tv_usec = static_cast<unsigned long>(READ_TIMEOUT_MSEC % MILLISECONDS_PER_SECOND) * MICROSECONDS_PER_MILLISECOND;
    int ret = select(connectedFd + 1, &fds, NULL, NULL, &timeout);

    if (ret > 0) {
        ret = static_cast<int>(read(connectedFd, &controlByte, sizeof(controlByte)));
        if (ret < 0) {
            LLogErr("read:errno=" << errno);
        } else if (ret == 0) {
            LLogErr("read:no_data");
        } else {
            if ((controlByte & FEC_NOTIFY_SET_DEBUG_LEVEL_MASK) == FEC_NOTIFY_SET_DEBUG_LEVEL) {
                unsigned int level = LUMO_LOG_INFO + (controlByte & FEC_NOTIFY_DEBUG_LEVEL_MASK);
                LLogSetLogLevel(level);
                LLogInfo("set_log_level:level=" << level);
            } else {
                unsigned int headNum = controlByte & FEC_NOTIFY_HEADNUM_MASK;
                LLogInfo("accept_control:received_byte=" << (int)controlByte << ",headNum=" << headNum);
                if (headNum < s_numHeads) {
                    // The thread completes this operation before the control byte is echoed
                    uint8_t notifyByte = translateControlByte(controlByte);
                    s_shThreads.at(headNum)->handleControlByte(notifyByte);
                } else {
                    LLogErr("accept_bad_head:headNum=" << headNum << ",s_numHeads=" << s_numHeads);
                    controlByte = FEC_NOTIFY_ERROR;
                }
            }
            ssize_t res __attribute__((unused)) = write(connectedFd, &controlByte, sizeof(controlByte));
        }
    } else if (ret == 0) {
        LLogErr("accept_timeout:timeoutMs=" << READ_TIMEOUT_MSEC << ":ignoring command");
    } else {
        LLogErr("accept_select:errno=" << errno << ":select failure");
    }

    shutdown(connectedFd, SHUT_RDWR);
    close(connectedFd);
}

typedef void (*event_handler_t) (int fileDes);

struct fe_event {
    int fileDes;
    event_handler_t handler;
};

#define MAX_EVENTS 6 // four sensor heads, listenerFd, exitFd
static std::array<struct fe_event, MAX_EVENTS> s_events{};
static unsigned int s_numEvents = 0;

/**
 * @brief Internal function to handle a read on the listen file descriptor
 *
 * @param fileDes The listen file descriptor that has available read data
 */
static void handleListenEvent(int fileDes)
{
    int connectedFd = accept(fileDes, NULL, NULL);
    if (connectedFd < 0) {
        LLogErr("accept:errno=" << errno);
    } else {
        // handle the accept; this will close the connection when it finishes
        handleAccept(connectedFd);
    }
}

/**
 * @brief Internal function to handle a read on the signal file descriptor; tells the sensor head threads to exit
 *
 * @param fileDes The listen file descriptor that has available read data
 */
static void handleSignalEvent(int fileDes)
{
    uint8_t zero = 0;

    if (read(fileDes, &zero, sizeof(zero)) < 0) {
        LLogErr("read_signal_event:errno=" << errno);
    }

    for (int head = 0; head < s_numHeads; head++) {
        s_shThreads.at(head)->exitThread();
    }
}

/**
 * @brief Internal function to handle an unsolicited (no control byte was sent) control byte from the sensor head thread indicating that the sensor head thread has terminated
 *
 * @param fileDes The sensor head trigger file descriptor that has available read data
 */
static void handleThreadEvent(int fileDes)
{
    // read the notification byte
    uint8_t zero;
    if (read(fileDes, &zero, sizeof(zero)) < 0) {
        LLogErr("read_shutdown_notification:errno=" << errno);
    }

    // clear the file descriptor associated with this head
    for (int head = 0; head < s_numHeads; head++) {
        if (s_shThreads.at(head)->getTrigFd() == fileDes) {
            s_shThreads.at(head)->markThreadAsStopped();
        }
    }
}

/**
 * @brief Internal function that returns true of all sensor head threads have exited
 */
static bool allThreadsExited(void)
{
    bool allExited = true;
    for (int head = 0; head < s_numHeads; head++) {
        if (!s_shThreads.at(head)->threadStopped()) {
            allExited = false;
            break;
        }
    }
    return allExited;
}

/**
 * @brief Internal function that executes the event loop until all of the sensor head threads have exited
 */
static void eventLoop(void)
{
    int retVal = 0;
    fd_set fdset;

    while (!allThreadsExited()) {
        // set up the file descriptor set
        int maxFd = 0;
        FD_ZERO(&fdset);
        for (int eventIdx = 0; eventIdx < s_numEvents; eventIdx++) {
            int fileDes = s_events[eventIdx].fileDes;
            FD_SET(fileDes, &fdset);
            if (fileDes > maxFd) {
                maxFd = fileDes;
            }
        }

        // select
        retVal = select(maxFd + 1, &fdset, NULL, NULL, NULL);

        if (retVal < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }

        // handle events
        if (retVal > 0) {
            for (int eventIdx = 0; eventIdx < s_numEvents; eventIdx++) {
                if (FD_ISSET(s_events[eventIdx].fileDes, &fdset)) {
                    auto *event = &s_events[eventIdx];
                    (event->handler) (event->fileDes);
                }
            }
        }
    }
}

/**
 * @brief Internal function that prints the usage information for the front end app
 *
 * @param error Flag: true if the usage message is printed due to an error; in this case the message is printed to stderr instead of stdout
 */
static void usage(bool error)
{
    (error ? std::cerr : std::cout) << "usage -- frontend [OPTIONS]" << std::endl;
    (error ? std::cerr : std::cout) << 
"  -l, --local-port=PORT      set the TCP port (default 1234) of the connection\n"
"                               that controls the front end\n"
"  -b, --base-port=PORT       set the TCP base port (default 12566) used to\n"
"                               output point cloud data\n"
"  -m, --mock-prefix=PATH     enable mocking and get mock data from files with\n"
"                               the name '<PATH>dddd.bin' where dddd is a\n"
"                               sequence number starting from 0000; the front\n"
"                               end will play the mock files in sequence until\n"
"                               a break in the sequence is found and then\n"
"                               repeat the sequence again starting from 0000.\n"
"                               If you enable mocking, you must also specify\n"
"                               the calibration file path using --cal-path and\n"
"                               the pixel mask file path using --pixmap-path\n"
"  -t, --mock-delay=DELAY     when mocking is enabled, set the delay (in\n"
"                               milliseconds) between the times ROIs are\n"
"                               presented to Raw2Depth\n"
"  -c, --cal-path=PATH        get sensor mapping table from the specified path\n"
"                               instead of the file provided by the system\n"
"                               control code\n"
"  -p, --pixmap-path=PATH     get pixel map from the specified path instead of\n"
"                               the file provided by the system control code\n"
"  -n, --num-heads=NUM        set the maximum number of heads to enable; for\n"
"                               the NCB, the maximum number of heads is 1\n"
"  -o, --output-prefix=PATH   enable raw output streaming to files; the\n"
"                               output file names are 'PATH_h_ss_dddd.bin'\n"
"                               where h is the head number (0-3), ss is the\n"
"                               session number, and dddd is the ROI number\n"
"  -r, --output-rois=NUM      when raw output streaming is enabled, set the\n"
"                               maximum number of ROIs that will be output\n"
"                               in a single session; defaults to 91 of omitted\n"
"  --max-net-frames=NUM       stop network streaming after NUM frames; set\n"
"                               to 0 to disable network output\n"
"  -s, --start-mode=MODE      set the startup time synchronization mode to\n"
"                               MODE. Ignored if mocking is eanbled.\n"
"                               Possible values are none (default), ptp,\n"
"                               and pps\n"
"  -h, --help                 print this help message\n";
    exit(error ? 1 : 0);
}

/**
 * @brief Internal function to add an event to the list of handled events
 *
 * @param fileDes File descriptor for the event
 * @param handler A function that gets executed when the file descriptor has read data available
 */
static void addEvent(int fileDes, event_handler_t handler)
{
    if (handler == nullptr) {
        LLogErr("handler_null");
        return;
    }

    if (s_numEvents < MAX_EVENTS) {
        s_events[s_numEvents].fileDes = fileDes;
        s_events[s_numEvents].handler = handler;
        s_numEvents++;
    }
}

/**
 * @brief Internl function to create listener socket and bind it to the 0.0.0.0 with the specified port
 *
 * @param listenPort The IP port to which the listener is bound; defaults to 1234
 */
static int setUpListener(int listenPort)
{
    const int oneOpt = 1;

    s_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listenFd < 0) {
        LLogErr("listen_socket:errno=" << errno << ":can't create listen socket; shutting down");
        return -1;
    }

    if (setsockopt(s_listenFd, SOL_SOCKET, SO_REUSEADDR, &oneOpt, sizeof(oneOpt)) < 0) {
        LLogErr("listen_setsockopt:errno=" << errno << ":can't set sockopt; shutting down");
        shutdown(s_listenFd, SHUT_RDWR);
        close(s_listenFd);
        s_listenFd = -1;
        return -1;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(listenPort);
    if (bind(s_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LLogErr("listen_bind:errno=" << errno << ":can't bind listen socket; shutting down");
        shutdown(s_listenFd, SHUT_RDWR);
        close(s_listenFd);
        s_listenFd = -1;
        return -1;
    }

    if (listen(s_listenFd, 1) < 0) {
        LLogErr("listen_listen:errno=" << errno << ":listen failed; shutting down");
        shutdown(s_listenFd, SHUT_RDWR);
        close(s_listenFd);
        s_listenFd = -1;
        return -1;
    }
    addEvent(s_listenFd, handleListenEvent);
    return 0;
}

static void handleSignal(int signal)
{
    uint8_t zero = 0;
    if (signal == SIGTERM || signal == SIGINT) {
        if (write(s_signalFd, &zero, sizeof(zero)) < 0) {
            LLogErr("signal_write:errno=" << errno);
        }
    }
}

/**
 * @brief Internal function to set up the signals as well as the socket pair used by the signal handler to tell the main thread to shut down
 */
static void setUpSignals(void)
{
    std::array<int, 2> socks{};
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, socks.data()) < 0) {
        LLogErr("signal_socket_pair:errno=" << errno);
        s_signalFd = -1;
        s_exitFd = -1;
    } else {
        s_signalFd = socks[0];
        s_exitFd = socks[1];
        addEvent(s_exitFd, handleSignalEvent);
        LLogDebug("s_signalFd=" << s_signalFd << ",s_exitFd=" << s_exitFd);
    }

    struct sigaction action{};
    memset(&action, 0, sizeof(action));
    action.sa_handler = handleSignal;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGPIPE, &action, NULL);
}

/**
 * @brief Internal function to convert the startup mode command line option to a startup_mode_enum_t
 */
static startup_mode_enum_t start_mode_for_string(const char *mode)
{
    startup_mode_enum_t ret = STARTUP_MODE_NO_TIMESYNC;
    if (strcmp(mode, "none") == 0) {
        ret = STARTUP_MODE_NO_TIMESYNC;
    } else if (strcmp(mode, "ptp") == 0) {
        ret = STARTUP_MODE_PTP_TIMESYNC;
    } else if (strcmp(mode, "pps") == 0) {
        ret = STARTUP_MODE_PPS_TIMESYNC;
    } else {
        LLogErr("unknown startup mode %s; defaulting to no time synchronization");
        ret = STARTUP_MODE_NO_TIMESYNC;
    }
    return ret;
}

#define DEFAULT_MAX_ROIS 91
#define VIDEO_DEVICE_NAME_SIZE 20
#define EVENT_LOOP_ITERATION_TIME 1000

/**
 * @brief Main function of front end application
 *
 * @param argc Number of command line arguments
 * @param argv Array containing command line arguments as null terminated character arrays
 *
 * @return Exit code for application; nonzero if there's an error condition
 */
int main(int argc, char *argv[])
{
    int opt;
    int optionIndex;
    const char *mockPrefix = nullptr;
    int port = LISTEN_PORT;
    int basePort = -1;
    int mockRoiDelay = -1;
    const char *outPrefix = nullptr;
    int outMaxRois = -1;
    std::array<std::shared_ptr<std::thread>, MAX_HEADS> threads;
    int maxNetFrames = -1;
    const char *calFileName = nullptr;
    const char *pixmapFileName = nullptr;
    startup_mode_enum_t startMode = STARTUP_MODE_NO_TIMESYNC;

    constexpr unsigned int NUM_OPTIONS {13}; // Keep this in sync with the array below
    const std::array<struct option, NUM_OPTIONS> longOptions = {{
        { "local-port",     required_argument, nullptr, 'l' },
        { "base-port",      required_argument, nullptr, 'b' },
        { "mock-prefix",    required_argument, nullptr, 'm' },
        { "mock-delay",     required_argument, nullptr, 't' },
        { "cal-path",       required_argument, nullptr, 'c' },
        { "pixmap-path",    required_argument, nullptr, 'p' },
        { "num-heads",      required_argument, nullptr, 'n' },
        { "output-prefix",  required_argument, nullptr, 'o' },
        { "output-rois",    required_argument, nullptr, 'r' },
        { "max-net-frames", required_argument, nullptr, 'f' },
        { "start-mode",     required_argument, nullptr, 's' },
        { "help",           no_argument,       nullptr, 'h' },
        { nullptr,          0,                 nullptr, 0   }
    }};

    LLogDebug("debug_mode::debug mode is enabled");

    // NOLINTNEXTLINE(hicpp-vararg) calling Linux vararg API
    while ((opt = getopt_long(argc, argv, "l:b:m:t:c:p:n:o:r:f:s:h", (const struct option *)longOptions.data(), &optionIndex)) != -1) {
        switch(opt) {
        case 'l' : port = atoi(optarg); break;
        case 'b' : basePort = atoi(optarg); break;
        case 'm' : mockPrefix = optarg; break;
        case 't' : mockRoiDelay = atoi(optarg); break;
        case 'n' :
            s_numHeads = atoi(optarg);
            if (s_numHeads > MAX_HEADS || s_numHeads < 0) {
                usage(true);
            }
            break;
        case 'o' :
            outPrefix = optarg;
            break;
        case 'r' :
            outMaxRois = atoi(optarg);
            break;
        case 'h' :
            usage(false);
            break;
        case 'f' :
            maxNetFrames = atoi(optarg);
            break;
        case 'c' :
            calFileName = optarg;
            break;
        case 'p' :
            pixmapFileName = optarg;
            break;
        case 's' :
            startMode = start_mode_for_string(optarg);
            break;
        default :
            usage(true);
            break;
        }
    }

    if (outPrefix != nullptr && outMaxRois < 0) {
        outMaxRois = DEFAULT_MAX_ROIS;
    }

    // if mocking, must specify the calibration file
    if (mockPrefix != nullptr && (calFileName == nullptr || pixmapFileName == nullptr)) {
        std::cerr << "you must specify both the calibration file (--cal-path) and the pixel map file (--pixmap-path) if you use the --mock-prefix option\n";
        usage(true);
    }

    // make sure the calFileName is not NULL because we will create a std::string with it
    if (calFileName == nullptr) {
        calFileName = "";
    }

    // make sure the pixmapFileName is not NULL because we will create a std::string with it
    if (pixmapFileName == nullptr) {
        pixmapFileName = "";
    }

    // Log the command line arguments
    LLogInfo("s_numHeads=" << s_numHeads);
    LLogInfo("port=" << port);
    LLogInfo("basePort=" << basePort);
    LLogInfo("mockPrefix=\"" << (mockPrefix != nullptr ? mockPrefix : "<none>") << "\"");
    LLogInfo("mockRoiDelay=" << mockRoiDelay);
    LLogInfo("calFileName=\"" << (calFileName != nullptr ? calFileName : "<none>") << "\"");
    LLogInfo("pixmapFileName=\"" << (pixmapFileName != nullptr ? pixmapFileName : "<none>") << "\"");
    LLogInfo("outPrefix=\"" << (outPrefix != nullptr ? outPrefix : "<none>") << "\"");
    LLogInfo("outMaxRois=" << outMaxRois);
    LLogInfo("maxNetFrames=" << maxNetFrames);
    LLogInfo("startMode=" << startMode);

    if (setUpListener(port) < 0) {
        return 1;
    }

    setUpSignals();

    std::shared_ptr<TimeSync> timeSyncP = nullptr;

    // create and run the threads
    if (mockPrefix != nullptr) { // mocking API
        for (int head = 0; head < s_numHeads; head++) {
            auto mock = std::make_shared<MockSensorHeadThread>(head,
                                                               mockPrefix,
                                                               mockRoiDelay,
                                                               outPrefix,
                                                               outMaxRois,
                                                               maxNetFrames,
                                                               calFileName,
                                                               pixmapFileName,
                                                               basePort);
            s_shThreads.at(head) = mock;
            threads.at(head) = std::make_shared<std::thread>(MockSensorHeadThread::selfRun, mock.get());
            addEvent(s_shThreads.at(head)->getTrigFd(), handleThreadEvent);
        }
    } else {
        timeSyncP = std::make_shared<TimeSync>(startMode);

        for (int head = 0; head < s_numHeads; head++) {
            std::stringstream name;
            name << "/dev/video" << std::setw(1) << VIDEO_DEVICE;
            auto v4l = std::make_shared<V4LSensorHeadThread>(head,
                                                             name.str(),
                                                             outPrefix,
                                                             outMaxRois,
                                                             maxNetFrames,
                                                             calFileName,
                                                             pixmapFileName,
                                                             basePort,
                                                             timeSyncP,
                                                             BASE_FPGA_I2C_ADDR + 2 * head);
            s_shThreads.at(head) = v4l;
            threads.at(head) = std::make_shared<std::thread>(V4LSensorHeadThread::selfRun, v4l.get());
            addEvent(s_shThreads.at(head)->getTrigFd(), handleThreadEvent);
        }
    } 

    // main loop exits when all threads exit
    eventLoop();

    LLogInfo("joining_threads");
    for (int head = 0; head < s_numHeads; head++) {
        s_shThreads.at(head)->exitThread();
        threads.at(head)->join();
        s_shThreads.at(head) = nullptr; // free the memory associated with the shared pointer
    }

    shutdown(s_listenFd, SHUT_RDWR);
    close(s_listenFd);
    LLogInfo("exiting");
    return 0;
}
