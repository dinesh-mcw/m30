/**
 * @file V4LSensorHeadThread.cpp
 *
 * @copyright Copyright (C) 2023-2024 Lumotive, Inc. All rights reserved.
 *
 * @brief This file provides the implementation of the V4LSensorHeadThread class,
 *        a subclass of the SensorHeadThread class.
 */

#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <linux/videodev2.h>
#include "frontend.h"
#include "LumoLogger.h"
#include "V4LSensorHeadThread.h"
#include "LumoAffinity.h"

typedef struct {
    int width;
    int height;
    int fps;
    int roiSize;
    int numRois;
    uint32_t v4l2_pix_fmt;
    const char *name;
} mode_info_t;

constexpr unsigned int NUM_MODES { 10 }; // keep this in sync with the array below!

static const std::array<mode_info_t, NUM_MODES> s_v4lFormatForMode = {{
    { 1280, 2881,  10, 11063040,  1, V4L2_PIX_FMT_BGR24, "DMFD_FF_BGR888"     },
    { 1280,  121, 637,   464640,  1, V4L2_PIX_FMT_BGR24, "DMFD_20_BGR888"     },
    { 1280,   41, 637,   157440,  1, V4L2_PIX_FMT_BGR24, "TA_20_BGR888"       },
    { 1280,  410,  64,   157440, 10, V4L2_PIX_FMT_BGR24, "TA_20_AG_10_BGR888" },
    { 1280,   49, 819,   188160,  1, V4L2_PIX_FMT_BGR24, "DMFD_8_BGR888"      },
    { 1280,   17, 819,    65280,  1, V4L2_PIX_FMT_BGR24, "TA_8_BGR888"        },
    { 1280,  170,  82,    65280, 10, V4L2_PIX_FMT_BGR24, "TA_8_AG_10_BGR888"  },
    { 1280,   37, 910,   142080,  1, V4L2_PIX_FMT_BGR24, "DMFD_6_BGR888"      },
    { 1280,   13, 910,    49920,  1, V4L2_PIX_FMT_BGR24, "TA_6_BGR888"        },
    { 1280,  130,  91,    49920, 10, V4L2_PIX_FMT_BGR24, "TA_6_AG_10_BGR888"  },
}};

#define NUM_FRAME_DROP_REPORTING_INTERVAL 10000 // How many frames received before reporting dropped frames
#define NUM_MODES (sizeof(s_v4lFormatForMode)/sizeof(s_v4lFormatForMode[0]))


/**
 * @brief Construct a V4LSensorHeadThread
 *
 * @param headNum The head number for this thread, only allowed to be 0 on NCB
 * @param devicePath The path of the video device used to receive MIPI
 * @param outPrefix The path prefix for the raw output files when raw streaming is enabled. Set to nullptr to disable raw streaming to files
 * @param outMaxRois The maximum number of ROIs that will be output in a session.
 * @param maxNetFrames The maximum number of output frames that will be sent over the network. Set to zero for unlimited frames
 * @param calFileName The name of the mapping table file. Set to nullptr to use the system control provided mapping table
 * @param pixmapFileName The name of the pixel map file. Set to nullptr to use the system control provided pixel map
 * @param basePort The TCP base port number for the point cloud data. The ports used will be basePort, basePort + 1, ... basePort + 7
 */
V4LSensorHeadThread::V4LSensorHeadThread(int headNum,
                                         std::string devicePath,
                                         const char *outPrefix,
                                         int outMaxRois,
                                         int maxNetFrames,
                                         const char *calFileName,
                                         const char *pixmapFileName,
                                         int basePort,
                                         std::shared_ptr<TimeSync> timeSync,
                                         unsigned int i2cAddress) :
    SensorHeadThread::SensorHeadThread(headNum, outPrefix, outMaxRois, calFileName, pixmapFileName, maxNetFrames, basePort),
    m_buffers({}),
    m_devicePath(devicePath),
    m_videoFd(-1),
    m_streaming(false),
    m_seqNum(0),
    m_frameCount(0),
    m_droppedFrames(0),
    m_dropEvents(0),
    m_roiSize(0),
    m_numRois(0),
    m_timeSync(timeSync),
    m_i2cAddress(i2cAddress),
    m_syncTimeOnNextSession(true),
    m_timeOffset(0),
    m_lastUserTags({ -1, -1, -1, -1, -1, -1, -1, -1 }) {
}

V4LSensorHeadThread::~V4LSensorHeadThread() = default;

#define NUM_ALLOWED_OPEN_FAILURES 10

/**
 * @brief Internal function to open the video device. This function also checks that the device supports
 *        all of the video modes supported by the front end
 * @returns 0 if the open succeeds, -1 if it fails; errors are logged
 */
int V4LSensorHeadThread::openDevice() {
    // open the video device
    int tries = 0;
    while (m_videoFd < 0 && tries < NUM_ALLOWED_OPEN_FAILURES) {
        m_videoFd = open(m_devicePath.c_str(), (unsigned int)O_RDWR | (unsigned int)O_NONBLOCK, 0);
        if (m_videoFd < 0) {
            LLogInfo("open:errno=" << errno << ",device=" << m_devicePath <<
                     ":failed to open video device; trying again");
            sleep(1);
            tries++;
        } else {
            LLogInfo("opened:device=" <<  m_devicePath << ",fd=" << m_videoFd <<
                     ":device opened successfully");
        }
    }

    bool failed = false;

    // check if the driver supports all of the video modes in our table
    for (int i = 0; i < NUM_MODES; i++) {
        struct v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (uninterruptedIoctl(m_videoFd, VIDIOC_G_FMT, &fmt) < 0) {
            LLogErr("get_initial_fmt:errno=" << errno <<
                    ":can't get initial video format; exiting session");
            return -1;
        }
        fmt.fmt.pix.width = s_v4lFormatForMode[i].width;
        fmt.fmt.pix.height = s_v4lFormatForMode[i].height;
        fmt.fmt.pix.pixelformat = s_v4lFormatForMode[i].v4l2_pix_fmt;
        LLogInfo("try_fmt:width=" << fmt.fmt.pix.width <<
                 ",height=" << fmt.fmt.pix.height <<
                 ",format=" << std::string((const char *)&fmt.fmt.pix.pixelformat, sizeof(fmt.fmt.pix.pixelformat)));

        if (uninterruptedIoctl(m_videoFd, VIDIOC_S_FMT, &fmt) < 0) {
            LLogErr("open_s_fmt:errno=" << errno <<
                     ":width=" << s_v4lFormatForMode[i].width <<
                     ",height=" << s_v4lFormatForMode[i].height <<
                     ",fd=" << m_videoFd << ":your kernel could be out of date; exiting");
            close(m_videoFd);
            m_videoFd = -1;
            return -1;
        }
        if (uninterruptedIoctl(m_videoFd, VIDIOC_G_FMT, &fmt) < 0) {
            LLogErr("open_g_fmt:errno=" << errno << ":can't read back video format; exiting session");
            return -1;
        }
        if (fmt.fmt.pix.width != s_v4lFormatForMode[i].width ||
            fmt.fmt.pix.height != s_v4lFormatForMode[i].height) {
            LLogErr("open_mismatch:expected_width=" << s_v4lFormatForMode[i].width <<
                    ",expected_height=" << s_v4lFormatForMode[i].height <<
                    ",actual_width=" << fmt.fmt.pix.width <<
                    ",actual_height=" << fmt.fmt.pix.height <<
                    ":your kernel could be out of date; exiting");
            failed = true;
        }
    }

    int retVal;

    if (failed) {
        close(m_videoFd);
        m_videoFd = -1;
        retVal = -1;
    } else {
        LLogInfo("open_modes_checked:numModes=" << NUM_MODES << ":all expected video modes present in driver");
        retVal = m_videoFd;
    }

    return retVal;
}

/**
 * @brief Internal function to close the video device
 */
void V4LSensorHeadThread::closeDevice() {
    if (m_videoFd >= 0) {
        close(m_videoFd);
        m_videoFd = -1;
    }
}

/**
 * @brief Internal function that issues an ioctl but restarts if an EINTR is received
 *
 * @param deviceFd File descriptor to which the ioctl is issued
 * @param request The ioctl request number
 * @param arg Pointer to the ioctl input output argument buffer
 *
 * @return 0 if the ioctl succeed, -1 if it fails; errors are logged
 */
int V4LSensorHeadThread::uninterruptedIoctl(int deviceFd, int request, void *arg) {
    int retVal;
    do {
        retVal = ioctl(deviceFd, request, arg); // NOLINT(hicpp-vararg) calling LINUX vararg API
    } while (retVal < 0 && errno == EINTR);
    return retVal;
}

/**
 * @brief Internal function that starts a Video for Linux streaming session
 *
 * @param mode The desired streaming video mode from the s_v4lFormatForMode table
 *
 * @return 0 if the session is started successfully, -1 if the session fails to start; errors are logged
 */
int V4LSensorHeadThread::startSession(uint8_t mode, uint8_t note) {
    // synchronize time if requested
    if (m_syncTimeOnNextSession && m_timeSync->initialized()) {
        m_timeOffset = m_timeSync->syncTime(m_i2cAddress);
        m_syncTimeOnNextSession = false;
        LLogInfo("sync_time:offset=" << m_timeOffset << ",head=" << m_headNum << ":time synchronized");
    }
    struct v4l2_format fmt{};

    ackControlByte(note);
    m_streaming = false;

    // tell the driver of the desired format
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix.width = s_v4lFormatForMode[mode].width;
    fmt.fmt.pix.height = s_v4lFormatForMode[mode].height;
    fmt.fmt.pix.pixelformat = s_v4lFormatForMode[mode].v4l2_pix_fmt;
    LLogDebug("start_requested_format:mode=" << s_v4lFormatForMode[mode].name <<
              ",width=" << fmt.fmt.pix.width <<
              ",height=" << fmt.fmt.pix.height <<
              ",pixfmt=" << std::string((const char *)&fmt.fmt.pix.pixelformat, sizeof(fmt.fmt.pix.pixelformat)));

    if (uninterruptedIoctl(m_videoFd, VIDIOC_S_FMT, &fmt) < 0) {
        LLogErr("start_set_fmt:errno=" << errno << ":can't set video format; exiting session");
        return -1;
    }

    if (uninterruptedIoctl(m_videoFd, VIDIOC_G_FMT, &fmt) < 0) {
        LLogErr("start_get_fmt:errno=" << errno << ":can't read back video format; exiting session");
        return -1;
    }

    struct v4l2_streamparm parms{};
    parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parms.parm.capture.timeperframe.numerator = 1;
    parms.parm.capture.timeperframe.denominator = s_v4lFormatForMode[mode].fps;

    if (uninterruptedIoctl(m_videoFd, VIDIOC_S_PARM, &parms) < 0) {
        LLogErr("start_set_parms:errno=" << errno << ":can't set fps; exiting session");
        return -1;
    }

    LLogDebug("start_actual_format:mode=" << s_v4lFormatForMode[mode].name <<
              ",width=" << fmt.fmt.pix.width <<
              ",height=" << fmt.fmt.pix.height <<
              ",pixfmt=" << std::string((const char *)&fmt.fmt.pix.pixelformat, sizeof(fmt.fmt.pix.pixelformat)));
                  
    // Initialize sendFrame information
    m_roiSize = s_v4lFormatForMode[mode].roiSize;
    m_numRois = s_v4lFormatForMode[mode].numRois;

    // now initialize mmap
    struct v4l2_requestbuffers reqBufs{};

    reqBufs.count = NUM_V4L_BUFFERS;
    reqBufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqBufs.memory = V4L2_MEMORY_MMAP;

    if (uninterruptedIoctl(m_videoFd, VIDIOC_REQBUFS, &reqBufs) < 0) {
        // unable to allocate the buffers in V4L
        // EINVAL means that the driver doesn't support MMAP
        LLogErr("req_bufs:errno=" << errno << ":failed to get V4L buffers; exiting session");
        endSession();
        return -1;
    }

    if (reqBufs.count < NUM_V4L_BUFFERS) {
        // failed to allocate the correct number of buffers
        LLogErr("req_bufs_count:count=" << reqBufs.count << ",expected=" << NUM_V4L_BUFFERS <<
                ":allocated incorrect number of buffers; exiting session");
        endSession();
        return -1;
    }

    // mmap the buffers
    for (int i = 0; i < NUM_V4L_BUFFERS; i++) {
        struct v4l2_buffer buf {};
        struct v4l2_plane plane {};

        buf.m.planes = &plane;
        buf.length = 1;

        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        plane.bytesused = m_roiSize * m_numRois;
        plane.length = m_roiSize * m_numRois;

        if (uninterruptedIoctl(m_videoFd, VIDIOC_QUERYBUF, &buf) < 0) {
            LLogErr("query_buf:errno=" << errno << ",i=" << i <<
                    ":failed to get buffer information; exiting session");
            endSession();
            return -1;
        }
        m_buffers.at(i).length = buf.m.planes->length;
        m_buffers.at(i).buffer = mmap(NULL,
                                      buf.m.planes->length,
                                      (unsigned int)PROT_READ | (unsigned int)PROT_WRITE,
                                      MAP_SHARED,
                                      m_videoFd,
                                      buf.m.planes->m.mem_offset);
        if (m_buffers.at(i).buffer == MAP_FAILED) {
            LLogErr("mmap:errno=" << errno << ":failed to mmap buffer; exiting session");
            endSession();
            return -1;
        }
        if (uninterruptedIoctl(m_videoFd, VIDIOC_QBUF, &buf) < 0) {
            LLogErr("queue_buf:errno=" << errno << ":failed to queue buffers");
            endSession();
            return -1;
        }
    }

    // initialize frame counters
    m_seqNum = -1;
    m_droppedFrames = 0;
    m_dropEvents = 0;
    m_frameCount = 0;

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (uninterruptedIoctl(m_videoFd, VIDIOC_STREAMON, &type) < 0) {
        LLogErr("streamon:errno=" << errno << ":failed to start streaming:shutting down session");
        endSession();
        return -1;
    }
    m_streaming = true;
    return 0;
}

/**
 * @brief Internal function that ends the current session also used to clean up if starting the session failed
 */
void V4LSensorHeadThread::endSession() {
    if (m_streaming) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (uninterruptedIoctl(m_videoFd, VIDIOC_STREAMOFF, &type) < 0) {
            LLogErr("streamoff:errno=" << errno << ":failed to stop streaming");
        }
        m_streaming = false;
        reportDroppedRois();
    }

    for (int i = 0; i < NUM_V4L_BUFFERS; i++) {
        if (m_buffers.at(i).buffer != nullptr && munmap(m_buffers.at(i).buffer, m_buffers.at(i).length) < 0) {
            LLogErr("munmap:errno=" << errno << ":failed to unmap buffer");
        }
        m_buffers.at(i).buffer = nullptr;
        m_buffers.at(i).length = 0;
    }

    // free all buffers
    struct v4l2_requestbuffers reqBufs{};

    reqBufs.count = 0;
    reqBufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqBufs.memory = V4L2_MEMORY_MMAP;

    if (uninterruptedIoctl(m_videoFd, VIDIOC_REQBUFS, &reqBufs) < 0) {
        LLogErr("reqbufs_free:errno=" << errno << ":failed to free buffers");
    }
}

/**
 * @brief Internal function that reports the number of dropped ROIs to the log
 */
void V4LSensorHeadThread::reportDroppedRois() {
    if (m_dropEvents != 0) {
        LLogWarning("roi_frame_drops:dropped=" << m_droppedFrames <<
                    ",events=" << m_dropEvents <<
                    ",received=" << m_frameCount <<
                    ":frame(s) were dropped");
    }
    m_frameCount = 0;
    m_droppedFrames = 0;
    m_dropEvents = 0;
}

/**
 * @brief Internal function that analyzes a MIPI frame to see if an ROI has been dropped or not
 *        This function also adjust the timestamps in each MIPI frame to UTC
 *
 * @param mipiFrameData A buffer of MIPI data; the caller must verify that the size of the data is large enough to hold the metadata
 */
void V4LSensorHeadThread::lookForDroppedRoisAndAdjustTime(uint8_t *mipiFrameData) {

    for (unsigned int roi = 0; roi < m_numRois; roi++) {
        // look for dropped frames
        auto metadata = RtdMetadata((const uint16_t *)mipiFrameData, m_roiSize);
        int seq = (int)metadata.getRoiCounter();

        // There are two ignored cases:
        // 1. Stream start, when m_seqNum is initialized to -1
        // 2. Sequence number wraparound when seq resets to 0
        if (m_seqNum != seq) {
            if (seq > 0 && m_seqNum >= 0) {
                if (seq <= m_seqNum) {
                    LLogInfo("frame_drop_weird_sequence:seq=" << seq << ",m_seqNum=" << m_seqNum);
                    m_droppedFrames++;
                } else {
                    m_droppedFrames += seq - m_seqNum;
                }
                m_dropEvents++;
                LLogDebug("frame_drop:seq=" << seq << ",m_seqNum=" << m_seqNum);
            }
            m_seqNum = seq;
        }
        m_seqNum++;
        m_frameCount++;
        if (m_frameCount >= NUM_FRAME_DROP_REPORTING_INTERVAL) {
            reportDroppedRois();
        }

        // Report on any changed user tags
        for (unsigned int fov = 0; fov < FOV_STREAMS_PER_HEAD; fov++) {
            int userTag = (int)metadata.getUserTag(fov);
            if (userTag != m_lastUserTags[fov]) {
                if (m_lastUserTags[fov] > 0) {
                    LLogInfo("user_tag_changed:oldTag=" << m_lastUserTags[fov] << ",newTag=" << userTag);
                }
                m_lastUserTags[fov] = userTag;
            }
        }

        // This is where we adjust the timestamp
        RtdMetadata::adjustTimestamp(mipiFrameData, m_timeOffset);
        mipiFrameData += m_roiSize;
    }
}


/**
 * @brief Internal function to receive a frame of MIPI data and send it to raw to depth; called when the video file descriptor has data is available for reading
 */
void V4LSensorHeadThread::retrieveAndSendMipiFrame() {
    struct v4l2_buffer buf {};
    struct v4l2_plane plane {};
    buf.m.planes = &plane;
    buf.length = 1;

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (uninterruptedIoctl(m_videoFd, VIDIOC_DQBUF, &buf) < 0) {
        if (errno != EAGAIN) {
            LLogErr("dequeue:errno=" << errno << ":failed to dequeue buffer");
        }
        return;
    }

    uint32_t index = buf.index;
    if (index >= NUM_V4L_BUFFERS) {
        LLogErr("buf_num:index=" << index << ",expected_max=" << NUM_V4L_BUFFERS);
        return;
    }

    if ((buf.flags & V4L2_BUF_FLAG_ERROR) == 0) {
        auto *ptr = (uint8_t *)m_buffers[index].buffer; // C-style for maximum speed and no error checking
        uint32_t length = m_buffers[index].length;

        // make sure buffer has the size we expect
        if (length >= m_roiSize * m_numRois) {
            lookForDroppedRoisAndAdjustTime(ptr);
            sendMipiFrame(ptr, m_roiSize, m_numRois);
        } else {
            LLogErr("buffer_too_small:length=" << length << ",roiSize=" << m_roiSize << ",numRois=" << m_numRois);
        }
    }
    if (uninterruptedIoctl(m_videoFd, VIDIOC_QBUF, &buf) < 0) {
        LLogErr("queueback:errno=" << errno << ":failed to queue buffer back");
    }
}

/**
 * @brief Internal function to execute a command received from the main thread; called after the run() has read data from the wait file descriptor
 *
 * @return 0 if successful, -1 if not; errors are logged
 */
int V4LSensorHeadThread::handleNotification(uint8_t note) {
    switch(note) {
        case THR_NOTIFY_NOTHING_HAPPENED : // this should never happen because of the select
            ackControlByte(note);
            break;
        case THR_NOTIFY_EXIT_THREAD :
            LLogInfo(std::string("notify_exit_thread"));
            return -1;
        default:
        {
            if ((note & THR_NOTIFY_COMMAND_MASK) == THR_NOTIFY_START_STREAMING ||
                (note & THR_NOTIFY_COMMAND_MASK) == THR_NOTIFY_START_STREAMING_WITH_RELOAD) {
                // start streaming
                uint8_t format = note & THR_NOTIFY_PARAM_MASK;
                LLogInfo("start_streaming:dev=" << m_devicePath << ",format=" << (int)format);
                if (m_streaming) {
                    LLogInfo("start_currently_streaming:dev=" << m_devicePath <<
                             ",m_streaming=" << m_streaming <<
                             ":ending current session");
                    endSession();
                }
                if (format < NUM_MODES) {
                    // startSession sends the ack
                    if (startSession(format, note) < 0) {
                        LLogErr("v4l_new_session_fail:errno=" << errno);
                    }
                } else {
                    LLogErr("v4l_new_session_mode:format=" << (int)format << ",max=" << NUM_MODES);
                    ackControlByte(note);
                }
            } else if (note == THR_NOTIFY_STOP_STREAMING) {
                LLogInfo("stop_streaming:dev=" << m_devicePath);
                if (m_streaming) {
                    LLogInfo("end_currently_streaming:dev=" << m_devicePath <<
                             ",m_streaming=" << m_streaming <<
                             ":ending current session");
                    endSession();
                }
                ackControlByte(note);
            } else {
                ackControlByte(note);
            }
            break;
        }
    }
    return 0;
}

/**
 * @brief The MIPI video sensor head thread main loop
 */
void V4LSensorHeadThread::run() {
    LumoAffinity::setAffinity(LumoAffinity::A72_1);

    if (openDevice() < 0) {
        SensorHeadThread::notifyShutdown();
        return;
    }

    while(true) {
        // set up select
        fd_set fds;
        int waitFd = SensorHeadThread::getWaitFd();
        FD_ZERO(&fds);
        FD_SET(waitFd, &fds);
        int maxFd = waitFd;
        if (m_streaming) {
            FD_SET(m_videoFd, &fds);
            if (maxFd < m_videoFd) {
                maxFd = m_videoFd;
            }
        }

        // wait for something to happen
        int ret = select(maxFd + 1, &fds, NULL, NULL, NULL); // no timeout
        if (ret < 0) {
            if (errno == EINTR) {
                continue; // back to the well if interrupted
            }
            LLogErr("select:errno=" << errno << ":exiting thread");
            SensorHeadThread::notifyShutdown();
            break;
        }

        if (m_streaming && FD_ISSET(m_videoFd, &fds)) {
            retrieveAndSendMipiFrame();
        }

        if (FD_ISSET(waitFd, &fds)) {
            uint8_t note = receiveNotification();
            if (handleNotification(note) < 0) {
                SensorHeadThread::notifyShutdown();
                break;
            }
        }
    }
}


void V4LSensorHeadThread::syncTimeOnNextSession() {
    LLogInfo("time will be synchronized for sensor head " << m_headNum);
    m_syncTimeOnNextSession = true;
}
