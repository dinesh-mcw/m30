/**
 * @file mddump.cpp
 *
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved.
 *
 * @brief This file is some sample code that reads a mock file containing ROI
 * (region of interest ) data and prints out the metadata for that ROI
 * in a human readable format.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <climits>
#include <sstream>
#include "LumoLogger.h"
#include "RtdMetadata.h"

#define DATA_SIZE 3840

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "usage mddump <file_prefix>\n";
        exit(1);
    }

    int num = 0;
    std::stringstream path;
    std::array<char, DATA_SIZE> data {};
    int ret = 0;

    while (true) {
        path << argv[1] << std::setw(4) << std::setfill('0') << num << ".bin";
        struct stat statBuf {};
        if (stat(path.str().c_str(), &statBuf) < 0) {
            if (num == 0) {
                std::cerr << "can't stat " << path.str() << " errno=" << errno << "\n";
                ret = 1;
            }
            break;
        }

        int fileDes = open(path.str().c_str(), O_RDONLY); // NOLINT(hicpp-vararg) open() is variadic
        if (fileDes < 0) {
            std::cerr << "can't open " << path.str() << ", errno=" << errno << "\n";
            exit(1);
        }

        int pos = 0;
        while (pos < data.size()) {
            int bytesRead = static_cast<int>(read(fileDes, data.data() + pos, data.size() - pos));
            if (bytesRead < 0) {
                std::cerr << "can't read " << path.str() << ", errno=" << errno << "\n";
                close(fileDes);
                exit(1);
            }
            pos += bytesRead;
        }
        close(fileDes);
        auto metadata = RtdMetadata((const uint16_t *)data.data(), data.size());
        std::cout << "file:" << path.str() << "\n";
        metadata.printMetadata();
        num++;
    }
    return ret;
}

