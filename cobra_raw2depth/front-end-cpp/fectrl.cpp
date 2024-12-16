
/**
 * @file fectrl.cpp
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.
 *
 * @brief This file contains the code for a command line TCP client for the
 * front end. It is able to send all of the commands that the front
 * end is capable of processing.
 */

#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include "frontend.h"
#include "LumoLogger.h"

#define HIGHEST_DEBUG_LEVEL 7  // Highest debug level
#define HIGHEST_HEAD        0  // Highest sensor head (0 on NCB)
#define HIGHEST_FORMAT      15 // Highest video format

/**
 * @brief Internal function that prints out usage information
 */
static void usage(void)
{
    std::cerr << "fectrl -h|--help" << std::endl;
    std::cerr << "       -d|--debug 0-7                    set the debug level (LOG_INFO is 0)" << std::endl;
    std::cerr << "       -s|--start 0-3 -f|--format <f>    start streaming" << std::endl;
    std::cerr << "       -e|--end 0-3                      end streaming" << std::endl;
    std::cerr << "       -r|--reload 0-3                   reload calibration data" << std::endl;
    std::cerr << "       -R|--raw 0-3                      start/unsuspend raw streaming" << std::endl;
    std::cerr << "       -S|--suspend 0-3                  suspend raw streaming" << std::endl;
    std::cerr << "       -t|--timesync 0-3                 synchronize time" << std::endl;
}

/**
 * @brief Internal function that determines if the chosen options are okay together
 *
 * @param start start streaming head number or -1 if -s was not specified on the command line
 * @param end end streaming head number or -1 if -e was not specified on the command line
 * @param debug debug level or -1 if -d was not specified on the command line
 * @param reload reload calibration table or -1 if -r was not specified on the command line
 * @param format streaming format index or -1 if -f was not specified on the command line
 * @param raw raw streaming head number or -1 if -R was not specified on the command line
 * @param suspend suspend head number or -1 if -S was not specified on the command line
 * @param timesync timesync head number or -1 if -t was not specified on the command line
 * @return true if the options are NOT legal together, false otherwise
 */
static bool are_bad_options(int start, int end, int debug, int reload, int format, int raw, int suspend, int tsync)
{
    bool retVal = false;
    int num_options_set = (start >= 0   ? 1 : 0) +
                          (end >= 0     ? 1 : 0) +
                          (debug >= 0   ? 1 : 0) +
                          (reload >= 0  ? 1 : 0) +
                          (raw >= 0     ? 1 : 0) +
                          (suspend >= 0 ? 1 : 0) +
                          (tsync >= 0   ? 1 : 0);

    if (num_options_set != 1) {
        retVal = true;
    }
    if (start > HIGHEST_HEAD ||
        end > HIGHEST_HEAD ||
        reload > HIGHEST_HEAD ||
        format > HIGHEST_FORMAT ||
        debug > HIGHEST_DEBUG_LEVEL ||
        raw > HIGHEST_HEAD ||
        suspend > HIGHEST_HEAD ||
        tsync > HIGHEST_HEAD) {
        retVal = true;
    }
    if (start >= 0 && format < 0) {
        retVal = true;
    }
    return retVal;
}

/**
 * @brief front end control main function
 * @param argc number of command line arguments
 * @param argv Array containing command line arguments as null terminated character arrays
 * @return Exit code for application; nonzero if there's an error condition
 */
int main(int argc, char *argv[])
{
    int opt;
    int debug = -1;
    int start = -1;
    int end = -1;
    int format = -1;
    int reload = -1;
    int raw = -1;
    int suspend = -1;
    int tsync = -1;
    int optIndex;
    uint8_t command;

    const std::array<struct option, 10> opts = {{
        { "help", no_argument, NULL, 'h' },
        { "debug", required_argument, NULL, 'd' },
        { "start", required_argument, NULL, 's' },
        { "format", required_argument, NULL, 'f' },
        { "end", required_argument, NULL, 'e' },
        { "reload", required_argument, NULL, 'r' },
        { "raw", required_argument, NULL, 'R' },
        { "suspend", required_argument, NULL, 'S' },
        { "timesync", required_argument, NULL, 't' },
        { NULL, 0, NULL, 0 }
    }};

    // NOLINTNEXTLINE(hicpp-vararg) calling Linux vararg API
    while ((opt = getopt_long(argc, argv, "hd:s:f:e:r:R:S:t:", (const struct option *)opts.data(), &optIndex)) != -1) {
        switch(opt) {
            case 'd' : debug = atoi(optarg);   break;
            case 's' : start = atoi(optarg);   break;
            case 'f' : format = atoi(optarg);  break;
            case 'e' : end = atoi(optarg);     break;
            case 'r' : reload = atoi(optarg);  break;
            case 'R' : raw = atoi(optarg);     break;
            case 'S' : suspend = atoi(optarg); break;
            case 't' : tsync = atoi(optarg);   break;
            case 'h' : {
                usage();
                exit(0);
            }
            default: {
                usage();
                exit(1);
                break;
            }
        }
    }

    if (are_bad_options(start, end, debug, reload, format, raw, suspend, tsync)) {
        usage();
        exit(1);
    }

    if (start >= 0) {
        command = FEC_NOTIFY_START_STREAMING | ((unsigned int)format << FEC_NOTIFY_FORMAT_SHIFT) | (unsigned int)start;
    } else if (end >= 0) {
        command = FEC_NOTIFY_STOP_STREAMING | (unsigned int)end;
    } else if (reload >= 0) {
        command = FEC_NOTIFY_RELOAD_CAL_DATA | (unsigned int)reload;
    } else if (debug >= 0) {
        command = FEC_NOTIFY_SET_DEBUG_LEVEL | (unsigned int)debug;
    } else if (raw >= 0) {
        command = FEC_NOTIFY_START_RAW_STREAMING | (unsigned int)raw;
    } else if (suspend >= 0) {
        command = FEC_NOTIFY_SUSPEND_RAW_STREAMING | (unsigned int)suspend;
    } else if (tsync >= 0) {
        command = FEC_NOTIFY_SYNC_TIME | (unsigned int)tsync;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "socket:errno=" << errno << std::endl;
        return 1;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(LISTEN_PORT);

    if (connect(sock, (const sockaddr *)&addr, sizeof(addr)) < 0) {
        std::cerr << "connect:errno=" << errno << std::endl;
        close(sock);
        return 1;
    }

    if (send(sock, &command, sizeof(command), 0) < 0) {
        std::cerr << "send:errno=" << errno << std::endl;
        close(sock);
        return 1;
    }

    uint8_t received;
    if (recv(sock, &received, sizeof(received), 0) < 0) {
        std::cerr << "recv:errno=" << errno << std::endl;
        close(sock);
        return 1;
    }

    if (received != command) {
        std::cerr << "received_mismatch:command=" << (int)command << ",received=" << (int)received << std::endl;
        close(sock);
        return 1;
    }
    close(sock);
    return 0;
}

