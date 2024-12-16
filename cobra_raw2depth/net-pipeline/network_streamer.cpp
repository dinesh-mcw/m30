/**
 * @file network_streamer.cpp
 * @brief This file contains the implementations of the network streamer
 *        classes: NetworkStreamer, UDPStreamer, and TCPWrappedStreamer.
 *        The UDPStreamer is not supported at this time.
 *        
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved
 */

#include "network_streamer.hpp"
#include "pipeline_data.hpp"
#include "LumoLogger.h"
#include "MappingTable.h"

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define PROTO_MAGIC_0   'B'
#define PROTO_MAGIC_1   'C'
#define PROTO_MAGIC_2   'D'
#define PROTO_MAGIC_3   'A'
#define MAGIC_SIZE      4
#define PROTO_VER 1U
#define HEADER_VERSION_SHIFT 4U         // Version is the top 4 bits of version_type

struct GlobalHeader {
    uint8_t magic[MAGIC_SIZE];          // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
    uint8_t version_type;
    uint32_t deviceVersion;
    uint32_t seq;
    uint32_t deviceID;
    uint32_t reserved;
} __attribute__((packed));

struct Type1Header {
    uint8_t timestamp[TIMESTAMP_SIZE];  // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
    uint16_t tscale_lookAngle;
    uint8_t rsc_aoSeqFlags;
    uint32_t aolsstartSeq;
    uint32_t aolsendSeq;
    uint32_t aocsstartSeq;
    uint32_t reserved;
} __attribute__((packed));

enum class Type1AOSeqFlags {
    lastSceneBeginSequenceValid = 1,
    lastSceneEndSequenceValid = 2,
    currentSceneBeginSequenceValid = 4
};

struct Type1Return {
    uint16_t ret1Intensity;
    uint16_t ret1Range;
    uint16_t ret1Reflectivity_ret1Flags;
    uint16_t ret2Intensity;
    uint16_t ret2Range;
    uint16_t ret2Reflectivity_ret2Flags;
    uint16_t backgroundIntensity;
} __attribute__((packed));

enum class Type1ReturnFlags {
    presentAndValid = 1,
    clippedHigh = 2,
    clippedLow = 4
};

struct Type1Packet {
    GlobalHeader globalHeader;
    Type1Header t1h;
    Type1Return ret[MAX_CPI_PER_RETURN];    // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
} __attribute__((packed));

#define PROTO_TYPED_CODE 0xDU

struct TypeDHeader {
    uint8_t timestamp[TIMESTAMP_SIZE];      // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
    uint8_t tscale_aoSeqFlags;
    uint32_t aolsstartSeq;
    uint32_t aolsendSeq;
    uint32_t aocsstartSeq;
    uint32_t aocsendSeq;
    uint16_t completeSizeSteerDim;
    uint16_t completeSizeStareDim;
    uint16_t payloadSteerOrderOffset;
    uint16_t payloadStareOrderOffset;
    // Hack in SWDL variable vertical crop and bin for now
    uint16_t bs_SteerOffset;
    uint16_t bs_SteerStep;
    uint16_t bs_StareOffset;
    uint16_t bs_StareStep;
    uint16_t bs_UserTag;
} __attribute__((packed));

enum class TypeDAOSeqFlags {
    lastSceneBeginSequenceValid = 1,
    lastSceneEndSequenceValid = 2,
    currentSceneBeginSequenceValid = 4,
    currentSceneEndSequenceValid = 8
};

struct TypeDReturn {
    uint16_t intensity;
    uint16_t range;
    uint16_t background;
    uint16_t snr;
    uint8_t extraAnnotation;
    uint8_t retFlags;
} __attribute__((packed));

enum class TypeDReturnFlags {
    rangePresentAndValid = 1,
    itensityPresentAndValid = 2,
    backgroundPresentAndValid = 4,
    snrPresentAndValid = 8,
    extraTypeNNCount = 16
};

struct TypeDPacket {
    GlobalHeader globalHeader;
    TypeDHeader tDh;
    TypeDReturn ret[MAX_CPI_PER_RETURN];        // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
} __attribute__((packed));


// Type C Packet: Sensor Information Update
// Sequential "fill" of sensor parameters in (U, V) space. Likely happens at "beginning" 
// (open of lower-level communication channel, re-initialization, or periodic beacon for 
// interpretation of future reports).

// Most common usage is (U,V) --> (Theta, Phi) coordinate map. Could also provide intensity
// offset/scale values or noise-figures (likely internal-only, as would likely be compensated
// for up-front in raw-to-depth processing).

// Let's force this to be dense for now -- I'd rather not let customers upsample/interpolate
// themselves (way too error prone).

// Note that this very well may be some giagantic pseudo-packet (if running over something
// like TCP, in a capture-file, or whatever other octet stream you might dream up), but could
// as easily be split into multiple packets as long as we guarantee somehow (beaconing in
// the very worst case?) that the client gets it eventually.

// These parameters should apply to *all future reports* until replaced by another Type 4
// packet.

// Sequential "fill" of returns starting at (payloadStartU, payloadStartV), filled
// in "U-Major" order, sequentually in single-unit step. If that fill exceeds
// imageEndV, wrap to (0, payloadStartV + 1). 
// NOTE: Not yet implemented in "Type C". We'll send 64 values and zero-pad at the
//       end of line for sake of babystepping.

#define PROTO_TYPEC_CODE 0xCU
#define TYPE_C_POINTS_PER_PACKET_MAX 64

struct TypeCHeader {
    uint16_t imageEndU;
    uint16_t imageEndV;
    uint16_t payloadStartU;
    uint16_t payloadStartV;
    uint8_t  parameterType;
    uint8_t  reserved0;
    uint16_t reserved1;
    uint32_t reserved2;
} __attribute__((packed)); // Total size: 16 bytes. 

enum class TypeCParameterType {
                                        // Whenever in doubt, Big Endian
    none = 1,                           // Null/Unused
    coordinateMap32ASTheta32ASPhi = 2   // int32_t Theta in Arc Seconds, int32_t Phy in Arc Seconds
};

struct TypeCPayload {
    uint8_t data[1];                    // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
                                        // Placeholder -- actual may over-run and distort future member alignment
}  __attribute__((packed));

struct TypeCPayload_CM32T32P {
    int32_t theta;                      // Common specialization of above case -- two 32-bit signed integers
    int32_t phi;                        // specifying (theta, phi) for the (U, V) --> (theta, phi) table
}  __attribute__((packed));

struct TypeCPacket {
    GlobalHeader globalHeader; //18 Bytes
    TypeCHeader tCh;
    TypeCPayload data[1];               // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
                                        // Placeholder -- actual may over-run and distort future member alignment
} __attribute__((packed));

struct TypeCMappingTable {              // Common specialization of above case -- two 32-bit signed integers 
    GlobalHeader globalHeader; //18 Bytes
    TypeCHeader tCh;
    TypeCPayload_CM32T32P mappingTableEntry[TYPE_C_POINTS_PER_PACKET_MAX]; // NOLINT(hicpp-avoid-c-arrays) Use of a packed structure
                                        // Fixed size length in "Type C"
} __attribute__((packed));

// We'll need some sort of configuration interface to the system monitor
// framework. For now, assume that'll get wired up through the constructor.
NetworkStreamer::NetworkStreamer(uint32_t deviceVersion,
                                 uint32_t deviceID,
                                 PipelineOutputType outputType) :
    m_configLocked(false),
    m_payloadBufferSpace({}),
    m_deviceVersion(deviceVersion), 
    m_deviceID(deviceID),
    m_seq(0),
    m_lastSceneSeqsValid(false),
    m_thisSceneSeqsValid(false),
    m_lastSceneBeginSeq(0),
    m_lastSceneEndSeq(0),
    m_thisSceneBeginSeq(0),
    m_thisSceneLastSeq(0),
    m_dbg_maxFrames(0),
    m_dbg_maxFramesActive(false),
    m_dbg_maxFramesRemaining(0),
    m_outputType(outputType)
{
    switch (m_outputType)
    {
        case PipelineOutputType::ProcessedData: {
            m_verbosePrefix = std::string("ProcessedData::");
            break;
        }
        case PipelineOutputType::RawData: {
            m_verbosePrefix = std::string("RawData::");
            break;
        }
        default: {
            break;
        }
    };
}

// If the stream isn't running, allow deviceID updates (maybe later we learn from metadata)
// after network streamer is constructed, but before it actually talks to anything
bool NetworkStreamer::setDeviceID(uint32_t deviceID)
{
    if(m_configLocked)
    {
        return false;
    }

    m_deviceID = deviceID;
    return true;
}    

// This will always reset the counter for maxFrames
// This is only for debug, and hopefully temporary
// maxFrames < 0   =>  disable maximum frame count
// maxFrames == 0  =>  send no frames, but initial connection processing (metadata, 
//                     table still) still should occur
void NetworkStreamer::set_dbg_MaxFrames(int maxFrames)
{
     if(maxFrames >= 0)
     {
         m_dbg_maxFramesActive = true;
         m_dbg_maxFrames = maxFrames;
         m_dbg_maxFramesRemaining = maxFrames;
     }
     else
     {
         m_dbg_maxFramesActive = false;
         m_dbg_maxFrames = maxFrames;
         m_dbg_maxFramesRemaining = 0;
     }
 }

void NetworkStreamer::WorkOnSingleChunk(ReturnChunk *chunk)
{

    StartROISend();
    
    switch (m_outputType)
    {
        case PipelineOutputType::ProcessedData:
        {
            WorkOnCPIChunk(chunk);
            break;
        }
        case PipelineOutputType::RawData:
        {
            WorkOnROIChunk(chunk);
            break;
        }
        default:
        {
            LLogErr(m_verbosePrefix << "NetworkStreamer::WorkOnSingleChunk Undefined pipeline output type.");
            // Previous versions exited here; I am not sure it is necessary
            break;
        }
    };

    FinishROISend();
}

static inline void fillInGlobalHeader(GlobalHeader *globalHeader,
                                      unsigned int headerType,
                                      uint32_t deviceVersion,
                                      uint32_t deviceID,
                                      uint32_t seq)
{
    globalHeader->magic[0] = PROTO_MAGIC_0;
    globalHeader->magic[1] = PROTO_MAGIC_1;
    globalHeader->magic[2] = PROTO_MAGIC_2;
    globalHeader->magic[3] = PROTO_MAGIC_3;
    globalHeader->version_type = PROTO_VER << HEADER_VERSION_SHIFT | headerType; 
    globalHeader->deviceVersion = htonl(deviceVersion);
    globalHeader->seq = htonl(seq);
    globalHeader->deviceID = htonl(deviceID);
}

static inline void fillInTypeDReturnData(TypeDPacket *packet, CPIReturn *cpir)
{
    for(unsigned int channel = 0; channel < MAX_CPI_PER_RETURN; channel++)
    {
        TypeDReturn* tDr = &packet->ret[channel];
        
        // Only single return in Type 2 packets
        if(cpir->rangeValid.at(channel))
        {
            tDr->range = htons(cpir->range.at(channel));
            tDr->retFlags |= (uint8_t)TypeDReturnFlags::rangePresentAndValid;
        }
        if(cpir->intensityValid.at(channel))
        {
            tDr->intensity = htons(cpir->intensity.at(channel));
            tDr->retFlags |= (uint8_t)TypeDReturnFlags::itensityPresentAndValid;
        }
        if(cpir->backgroundValid.at(channel))
        {
            tDr->background = htons(cpir->background.at(channel));
            tDr->retFlags |= (uint8_t)TypeDReturnFlags::backgroundPresentAndValid;
        }
        if(cpir->snrValid.at(channel))
        {
            tDr->snr = htons(cpir->snr.at(channel));
            tDr->retFlags |= (uint8_t)TypeDReturnFlags::snrPresentAndValid;
        }            
        if(cpir->extraAnnotationType.at(channel) != 0)
        {
            tDr->extraAnnotation = cpir->extraAnnotation.at(channel);
            tDr->retFlags |= (unsigned int)cpir->extraAnnotationType.at(channel) << 4U;
        }
    }
}

void NetworkStreamer::WorkOnCPIChunk(ReturnChunk *chunk)
{
    // For now, generate advisory-only scene markers (which is safe, since
    // the are in fact advisory-only) assuming one receive chunk per scene
    // if we have no other information. I.e. set newScene to true at the 
    // beginning of an RC.

    // If "lastCpiInFrame" flag is set on a CPI, this indicates it's the last CPI
    // in a frame, and we should increment the frame counters on the next one.
    // I.e. set newScene to true on next iteration when emitFrame is set on
    // current CPI.
    bool newScene = true;

    // For each return in the chunk, generate a packet
    for(uint32_t rNum = 0; rNum < chunk->cpiReturnsUsed; rNum ++)
    {
        CPIReturn* cpir = chunk->cpiReturns.at(rNum);

        // If we want to suppress streaming of this CPI, make sure we skip
        // scene and sequence management so we don't confuse consumers about
        // missing data!
        if(cpir->metaSupressStream)
        {
            continue;
        }

        // Update calibration pointers coherently (want to do it in same thread)
        // as they're read from, regardless of if they're used at this moment or
        // later. If any are null -- a full update isn't available in this CpiReturn, 
        // and we shouldn't update our cached version.
        if ((cpir->calibrationX && cpir->calibrationY && cpir->calibrationTheta && cpir->calibrationPhi))
        {
            m_calibrationX = cpir->calibrationX;
            m_calibrationY = cpir->calibrationY;
            m_calibrationTheta = cpir->calibrationTheta;
            m_calibrationPhi = cpir->calibrationPhi;
        }

        if(cpir->prefixMetaDataUpdate)
        {
            UpdateClientMeta();
        }

        // Scene Management
        if(newScene)
        {
            // Last Scene <- This Scene
            m_lastSceneSeqsValid = m_thisSceneSeqsValid;
            m_lastSceneBeginSeq = m_thisSceneBeginSeq;
            m_lastSceneEndSeq = m_thisSceneLastSeq;

            // This Scene <- Fresh
            m_thisSceneSeqsValid = true;
            m_thisSceneBeginSeq = m_seq;

            // We've consumed the newScene flag
            newScene = false;
        }

        std::fill(m_payloadBufferSpace.begin(), m_payloadBufferSpace.end(), 0);
        auto* packet = (TypeDPacket *) m_payloadBufferSpace.data();

        // Global Header
        fillInGlobalHeader(&packet->globalHeader, PROTO_TYPED_CODE, m_deviceVersion, m_deviceID, m_seq);

        // Type D Header
        TypeDHeader* tDh = &packet->tDh;

        // Timestamp should already be in network order
        memcpy(&tDh->timestamp, cpir->timestamp.data(), cpir->timestamp.size());
        tDh->tscale_aoSeqFlags = ((uint8_t) cpir->tscale) << 4U;
        if(m_lastSceneSeqsValid)
        {
            tDh->tscale_aoSeqFlags |= (uint8_t)TypeDAOSeqFlags::lastSceneBeginSequenceValid;
            tDh->tscale_aoSeqFlags |= (uint8_t)TypeDAOSeqFlags::lastSceneEndSequenceValid;
            tDh->aolsstartSeq = htonl(m_lastSceneBeginSeq);
            tDh->aolsendSeq = htonl(m_lastSceneEndSeq);
        }
        if(m_thisSceneSeqsValid)
        {
            tDh->tscale_aoSeqFlags |= (uint8_t)
                TypeDAOSeqFlags::currentSceneBeginSequenceValid;
            tDh->aocsstartSeq = htonl(m_thisSceneBeginSeq);

            if (cpir->metaLastCpiInFrame)
            {
                tDh->tscale_aoSeqFlags |= (uint8_t)
                    TypeDAOSeqFlags::currentSceneEndSequenceValid;
                tDh->aocsendSeq = htonl(m_seq);
            }
        }
        tDh->completeSizeSteerDim = htons(cpir->completeSizeSteerDim);
        tDh->completeSizeStareDim = htons(cpir->completeSizeStareDim);
        tDh->payloadSteerOrderOffset = htons(cpir->startingSteerOrder);
        tDh->payloadStareOrderOffset = htons(cpir->startingStareOrder);
        tDh->bs_SteerOffset = htons(cpir->bs_SteerOffset);
        tDh->bs_SteerStep = htons(cpir->bs_SteerStep);
        tDh->bs_StareOffset = htons(cpir->bs_StareOffset);
        tDh->bs_StareStep = htons(cpir->bs_StareStep);
        tDh->bs_UserTag = htons(cpir->bs_UserTag);

        // Return Data
        fillInTypeDReturnData(packet, cpir);

        // Scene and Sequence Management
        m_thisSceneLastSeq = m_seq;
        m_seq++;
        if(cpir->metaLastCpiInFrame)
        {
            newScene = true;
        }

        // Fire the packet off
        this->NetworkSend((char *)packet, sizeof(TypeDPacket));
    }

}

void NetworkStreamer::WorkOnROIChunk(ReturnChunk *chunk)
{
    this->NetworkROISend(chunk->roiReturn->roi.data(), ROI_SIZE);
}

// Trivial Implementation -- No prep work
void NetworkStreamer::StartROISend()
{
}

// Trivial Implementation
void NetworkStreamer::FinishROISend()
{
    if(m_dbg_maxFramesActive && m_dbg_maxFramesRemaining >= 0)
    {
        m_dbg_maxFramesRemaining--;
    }
}

constexpr size_t MAPPING_TABLE_WIDTH    { IMAGE_WIDTH * 2U - 1U };
constexpr size_t MAPPING_TABLE_HEIGHT   { MAX_IMAGE_HEIGHT * 2U - 1U };

void NetworkStreamer::UpdateClientMeta()
{
    // Send mapping table
    if(!(m_calibrationX && m_calibrationY && m_calibrationTheta && m_calibrationPhi))
    {
        return;
    }
    StartROISend();
    auto *calibrationTheta = m_calibrationTheta.get();
    auto *calibrationPhi = m_calibrationPhi.get();
    assert(calibrationTheta->size() == calibrationPhi->size());
    size_t len = calibrationTheta->size();
    int mappingTableIndex = 0;
    
    // Send out one Type C packet with 64 points per line (zero padding end)
    // Note that these sizes *should* be stored in the mapping table?
    // TODO: Add sizes to mapping table -- they shouldn't be fixed here
    size_t width = MAPPING_TABLE_WIDTH;
    size_t height = MAPPING_TABLE_HEIGHT;
    assert(len == width * height);

    for (size_t payloadV = 0; payloadV < height; payloadV++)
    {
        for (size_t payloadU = 0; payloadU < width; payloadU += TYPE_C_POINTS_PER_PACKET_MAX)
        {
            memset(m_payloadBufferSpace.data(), 0, m_payloadBufferSpace.size());
            auto *packet = (TypeCMappingTable *)m_payloadBufferSpace.data();

            // Global Header
            fillInGlobalHeader(&packet->globalHeader, PROTO_TYPEC_CODE, m_deviceVersion, m_deviceID, m_seq);
            m_seq++;

            // Type C Header
            TypeCHeader *tCh = &packet->tCh;
            tCh->imageEndU = htons(width - 1);
            tCh->imageEndV = htons(height - 1);
            tCh->payloadStartU = htons(payloadU);
            tCh->payloadStartV = htons(payloadV); 
            tCh->parameterType = (uint8_t) TypeCParameterType::coordinateMap32ASTheta32ASPhi;

            // Fill in points
            for(int mapPktIndex = 0; mapPktIndex < TYPE_C_POINTS_PER_PACKET_MAX; mapPktIndex++)
            {
                // Incomplete packing for now (i.e. we're padding end-of-lines) -- no wrapping
                if((mapPktIndex + payloadU) >= width)
                {
                    continue;
                } 
                // to a signed host to network conversion for azimuth/elevation
                uint32_t theta = htonl(*reinterpret_cast<uint32_t*>(&(*calibrationTheta)[mappingTableIndex]));
                uint32_t phi = htonl(*reinterpret_cast<uint32_t*>(&(*calibrationPhi)[mappingTableIndex]));
                packet->mappingTableEntry[mapPktIndex].theta = *(reinterpret_cast<int32_t*>(&theta));
                packet->mappingTableEntry[mapPktIndex].phi = *(reinterpret_cast<int32_t*>(&phi));
                mappingTableIndex++;
            }

            // Pull trigger
            this->NetworkSend((char *)packet, sizeof(TypeCMappingTable));
        }
    }

    FinishROISend();
}

UDPStreamer::UDPStreamer(
    uint32_t deviceVersion,
    uint32_t deviceID,
    const char* targetHost,
    uint16_t targetPort,
    PipelineOutputType outputType
) : NetworkStreamer (deviceVersion, deviceID, outputType),
    m_targetAddr ({})
{
    struct hostent *targetHe;

    // Note that we are not running threads yet -- non-reentrant syscalls
    // are okay during setup, and keeps us more POSIX compliant.
    targetHe = gethostbyname(targetHost);
    if(targetHe == nullptr)
    {
        LLogErr("NetworkStreamer::NetworkStreamer gethostbyname:h_err=" << hstrerror(h_errno));
        exit(1);
    }

    m_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(m_fd < 0)
    {
        LLogErr("NetworkStreamer::NetworkStreamer socket, errno=" << errno);
        exit(1);
    }

    // For now, assume broadcast permission required
    const int SO_BROADCAST_true = 1;
    if (setsockopt(m_fd, SOL_SOCKET, SO_BROADCAST, &SO_BROADCAST_true, sizeof(SO_BROADCAST_true)) < 0)
    {
        LLogErr("NetworkStreamer::NetworkStreamer setsockopt, errno=" << errno);
        exit(1);
    }

    m_targetAddr.sin_family = AF_INET; 
    m_targetAddr.sin_port = htons(targetPort);
    m_targetAddr.sin_addr = *((struct in_addr *)targetHe->h_addr);
    memset((void *)m_targetAddr.sin_zero, 0, sizeof m_targetAddr.sin_zero);
}

void UDPStreamer::NetworkSend(char *buffer, size_t len)
{
    // After we fire the first packet, don't accept config changes
    m_configLocked = true;

    // Fire off a UDP packet to prescribed target
    if (sendto(m_fd, buffer, sizeof(Type1Packet), 0, (struct sockaddr *)&m_targetAddr, sizeof(m_targetAddr)) < 0)
    {
        perror("NetworkStreamer::WorkOnSingleChunk sendto");
    }
}

struct FramingHeader {
    uint32_t len;
    uint32_t flags;
    uint32_t flagDependentVal1;
    uint32_t flagDependentVal2;
} __attribute__((packed));

enum class FHFlags {
    paddingOnly = 1,
    echoValValid = 2,
    echoValNew = 4,
    skipStats = 8
};

// For now, just testing TCP performance. Probably want to listen/accept in a more
// sane way (separate thread?) if this works out
// Also, if this works out:
//      Should we stream to *both* UDP and TCP, or swap on-the-fly if we get a TCP connection?
//      Should we send beacon broadcast/multicast frames (even if not the full stream) such that
//           a GUI might auto-populate guessed addresses? The beacon might not punch through
//           firewalls however, so we might not want to depend on this.
TCPWrappedStreamer::TCPWrappedStreamer(uint32_t deviceVersion,
                                       uint32_t deviceID,
                                       uint16_t tcpPort,
                                       ssize_t minSockBuffer,
                                       PipelineOutputType outputType) :
    NetworkStreamer(deviceVersion, deviceID, outputType),
    m_listenfd(-1),
    m_clientfd(-1),
    m_reqSockBuffer(minSockBuffer),
    m_servAddr({}),
    m_clientAddr({}),
    m_clientAddrLen(0),
    m_tcpBufferSpace({}),
    m_tcpROIBufferSpace({})
{
    // Grab a socket
    m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_listenfd < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::TCPWrappedStreamer socket, errno=" << errno);
        exit(1);
    }

    // Reuse it
    const int REUSEADDR_true = 1;
    if(setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &REUSEADDR_true, sizeof(REUSEADDR_true)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::TCPWrappedStreamer setsockopt, errno=" << errno);
        exit(1);
    }

    // Nonblocking accept
    if(fcntl(m_listenfd, F_SETFL, O_NONBLOCK) < 0) // NOLINT(hicpp-vararg) Must call C API
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::TCPWrappedStreamer fcntl, errno=" << errno);
        exit(1);
    }

    // Bind
    m_servAddr.sin_family = AF_INET;
    m_servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_servAddr.sin_port = htons(tcpPort);
    
    if (bind (m_listenfd, (struct sockaddr *) &m_servAddr, sizeof(m_servAddr)) < 0)
    {
        LLogErr(m_verbosePrefix << "NetworkStreamer::NetworkStreamer bind, errno=" << errno);
        exit(1);
    }

    // Listen
    if (listen(m_listenfd, TCP_SERVE_BACKLOG) < 0)
    {
        LLogErr(m_verbosePrefix << "NetworkStreamer::NetworkStreamer listen, errno=" << errno);
        exit(1);
    }
}

TCPWrappedStreamer::TCPWrappedStreamer(uint32_t deviceVersion,
                                       uint32_t deviceID,
                                       uint16_t tcpPort,
                                       PipelineOutputType outputType)
    : TCPWrappedStreamer(deviceVersion, deviceID, tcpPort, 0, outputType)
{
}

// Future ideas here:
//      - Read Linux/kernel-specific TCP performance counters to estimate RTT
//           and maybe report as metadata?

// Start sending stream of ROIs
void TCPWrappedStreamer::StartROISend()
{
    NetworkStreamer::StartROISend();

    // Check for new connections (new ones should preempt old ones, since we
    // only handle one client -- multiples might need a "reflector"-style
    // proxy that can handle bufering frameskip-per-client.

    AcceptNewConnection();

    // Is anyone there? If not, nothing to handle
    if (m_clientfd < 0)
    {
        return;
    }

    // Cork till entire ROI is submitted to take advantage of larger packets
    // when available
    const int TCP_CORK_true = 1;
    if(setsockopt(m_clientfd, IPPROTO_TCP, TCP_CORK, &TCP_CORK_true, sizeof (TCP_CORK_true)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::NetworkSend setsockopt TCP_CORK, errno=" << errno);
    }
}

void TCPWrappedStreamer::FinishROISend()
{
    NetworkStreamer::FinishROISend();
    
    // Uncork -- we're done with ROI
    if (m_clientfd >= 0)
    {
        const int zero = 0;
        if (setsockopt(m_clientfd, IPPROTO_TCP, TCP_CORK, &zero, sizeof(zero)) < 0)
        {
            LLogErr(m_verbosePrefix << "TCPWrappedStreamer::NetworkSend setsockopt TCP_CORK, errno=" << errno);
        }
    }
}

void TCPWrappedStreamer::NetworkROISend(char *roi, size_t len)
{
    bool sendFailed = false;
    char *buf;

    if (m_outputType != PipelineOutputType::RawData)
    {
        LLogErr("TCPWrappedStreamer::NetworkROISend Network");
        return;
    }

    if (len > RAWDATA_PAYLOAD_MAX_SIZE)
    {
        LLogErr("TCPWrappedStreamer::NetworkROISend Payload Too Large");
        return;
    }

    // Is anyone there? If not, nowhere to send -- do nothing and try again later
    if (m_clientfd < 0)
    {
        return;
    }

    // TODO: Scatter-gather send if these memcpys have too much perf impact

    // Slap on our frame header
    memset(m_tcpROIBufferSpace.data(), 0, m_tcpROIBufferSpace.size());
    auto *framingHeader = (FramingHeader *)m_tcpROIBufferSpace.data();
    framingHeader->len = htonl(len);

    // Copy over the original payload
    memcpy(m_tcpROIBufferSpace.data() + sizeof(FramingHeader), roi, len);
    len += sizeof(FramingHeader);
    buf = m_tcpROIBufferSpace.data();

    while (len > 0)
    {
        ssize_t bytesRead = send(m_clientfd, buf, len, 0);
        if (bytesRead < 0)
        {
            sendFailed = true;
            break;
        }
        len -= bytesRead;
        buf += bytesRead;
    }

    // Did we fail?
    if (sendFailed)
    {
        if ((errno == ECONNRESET) || (errno == EPIPE))
        {
            CloseConnection();
        }
        else
        {
            LLogErr("TCPWrappedStreamer::NetworkROISend send, errno=" << errno);
        }
    }
}

void TCPWrappedStreamer::NetworkSend(char *buffer, size_t len)
{
    bool sendFailed = false;
    char *buf;

    if (m_outputType != PipelineOutputType::ProcessedData)
    {
        LLogErr("TCPWrappedStreamer::NetworkSend Can't use NetworkSend with something else than ProcessedData");
        return;
    }

    if (len > PROCESSEDDATA_PAYLOAD_MAX_SIZE)
    {
        LLogErr("TCPWrappedStreamer::NetworkSend Payload Too Large");
        return;
    }

    // Is anyone there? If not, nowhere to send -- do nothing and try again later
    if (m_clientfd < 0)
    {
        return;
    }

    // TODO: Scatter-gather send if these memcpys have too much perf impact

    // Slap on our frame header
    memset(m_tcpBufferSpace.data(), 0, m_tcpBufferSpace.size());
    auto *framingHeader = (FramingHeader *)m_tcpBufferSpace.data();
    framingHeader->len = htonl(len);

    // Copy over the original payload
    memcpy(m_tcpBufferSpace.data() + sizeof(FramingHeader), buffer, len);
    len += sizeof(FramingHeader);
    buf = m_tcpBufferSpace.data();


    while (len > 0)
    {
        ssize_t bytesRead = send(m_clientfd, buf, len, 0);
        if (bytesRead < 0)
        {
            sendFailed = true;
            break;
        }
        len -= bytesRead;
        buf += bytesRead;
    }

    // Did we fail?
    if (sendFailed)
    {
        if ((errno == ECONNRESET) || (errno == EPIPE))
        {
            CloseConnection();
        }
        else
        {
            LLogErr("TCPWrappedStreamer::NetworkSend send, errno=" << errno);
        }
    }
}


bool TCPWrappedStreamer::AcceptNewConnection()
{
    int option = 0; // we will reuse this integer to set socket options

    // Look for a new connection
    struct sockaddr_in clientAddr {};
    socklen_t clientAddrLen = sizeof(clientAddr);
    int clientfd = -1;

    clientfd = accept(m_listenfd, (struct sockaddr *)&clientAddr, &clientAddrLen);
    if (clientfd < 0)
    {
        if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
        {
            LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection accept:errno=" << errno);
        }
        return false; // in all cases leave the listening socket open
    }

    // We got something new -- close existing connection (if present)
    if(m_clientfd >= 0)
    {
        CloseConnection();
    }

    // Lock config changes while connection is open
    m_configLocked = true;

    // Keep the new connection
    m_clientfd = clientfd;
    memcpy(&m_clientAddr, &clientAddr, sizeof(clientAddr));
    m_clientAddrLen = clientAddrLen;

    std::array<char, INET_ADDRSTRLEN> clientAddrString {};
    memset(clientAddrString.data(), 0, clientAddrString.size());

    LLogInfo(m_verbosePrefix << "TCP: Got connection.");

    if(inet_ntop(AF_INET, (void *) &m_clientAddr.sin_addr, clientAddrString.data(), clientAddrString.size()) != nullptr)
    {
        LLogInfo(m_verbosePrefix << "TCP: to " << clientAddrString.data());
    }
    else
    {
        LLogInfo(m_verbosePrefix << "TCP: to --.");
    }

    // Might want to flush the tube with a large padding-only send to get TCP
    // going at a reasonable-speed

    // Or other Linux/kernel-specific TCP/socket/qdisc vodoo
    
    // Disable Nagle's Algorithm
    // NOTE: Later on, TCP_CORK will override this during normal operation. Turning
    //       off Nagle as default during non-corked operation.
    option = 1;
    if(setsockopt(m_clientfd, IPPROTO_TCP, TCP_NODELAY, &option, sizeof (option)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection setsockopt TCP_NODELAY, errno=" << errno);
    }

    // Set a minimum send buffer size, if so requested.
    // (We'll later use this when checking for sufficient space for a wrapped UDP payload --
    //  it'll make no sense to wait later on for buffer space to open up if we'll never get
    //  the amount we're waiting for).
    // Note that Linux will likely get *more* buffer space than we request.
    if (m_reqSockBuffer != 0)
    {
        option = (int)m_reqSockBuffer;
        if (setsockopt(m_clientfd, SOL_SOCKET, SO_SNDBUF, &option, sizeof(option)) < 0)
        {
            LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection setsockopt SO_SNDBUF");
        }
    }

    socklen_t length = sizeof(option);
    if (getsockopt(m_clientfd, SOL_SOCKET, SO_SNDBUF, &option, &length) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection getsockopt SO_SNDBUF");
    }
    LLogDebug(m_verbosePrefix << "TCP: send buffer size requested: " << m_reqSockBuffer << " actual: " << option);
    if (option < m_reqSockBuffer)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection: Could not set minimum send buffer size as requested");
    }

    // Lets fail fast on hung connections, as we're only accepting a single connection
    // for now, and we don't want re-connect attempts to fail.

    // Enable TCP KEEPALIVE, TCP_KEEPIDLE = 1 second, TCP_KEEPINTVL = 11 seconds (1, 11 are
    // relatively prime), TCP_KEEPCNT = 3 probes. Effectively ~34 seconds of timeout
    // if connection idle.
    constexpr int KEEPALIVE_TIME_SEC { 1 };
    option = KEEPALIVE_TIME_SEC;
    if (setsockopt(m_clientfd, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection setsockopt SO_KEEPALIVE");
    }
    constexpr int KEEPIDLE_TIME_SEC { 1 };
    option = KEEPIDLE_TIME_SEC;
    if (setsockopt(m_clientfd, IPPROTO_TCP, TCP_KEEPIDLE, &option, sizeof(option)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection setsockopt TCP_KEEPIDLE");
    }
    constexpr int KEEPINTVL_TIME_SEC { 11 };
    option = KEEPINTVL_TIME_SEC;
    if (setsockopt(m_clientfd, IPPROTO_TCP, TCP_KEEPINTVL, &option, sizeof(option)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection setsockopt TCP_KEEPINTVL");
    }
    constexpr int KEEPCOUNT { 3 };
    option = KEEPCOUNT;
    if (setsockopt(m_clientfd, IPPROTO_TCP, TCP_KEEPCNT, &option, sizeof(option)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection setsockopt TCP_KEEPCNT");
    }

    // Set TCP_USER_TIMEOUT = 30 seconds. Timeout after ~30 seconds when there is
    // outstanding unacknowledged send data. Of same order as idle timeout above
    // by design.
    constexpr int USER_TIMEOUT_MS { 30000 };
    option = USER_TIMEOUT_MS;
    if (setsockopt(m_clientfd, IPPROTO_TCP, TCP_USER_TIMEOUT, &option, sizeof(option)) < 0)
    {
        LLogErr(m_verbosePrefix << "TCPWrappedStreamer::AcceptNewConnection setsockopt TCP_USER_TIMEOUT");
    }

    if (m_outputType == PipelineOutputType::ProcessedData)
    {
        UpdateClientMeta();
    }

    // Reset frame limits
    set_dbg_MaxFrames(m_dbg_maxFrames);

    return true;
}


void TCPWrappedStreamer::CloseConnection()
{
    std::array<char, INET_ADDRSTRLEN> clientAddrString {};
    memset(clientAddrString.data(), 0, clientAddrString.size());

    LLogInfo(m_verbosePrefix << "TCP: Closing connection");

    if(inet_ntop(AF_INET, (void *) &m_clientAddr.sin_addr, clientAddrString.data(), clientAddrString.size()) != nullptr)
    {
        LLogInfo(m_verbosePrefix << "TCP: to " << clientAddrString.data() << ".");
    }
    else
    {
        LLogInfo(m_verbosePrefix << "TCP: to ---.");
    }

    close(m_clientfd);
    m_clientfd = -1;

    // Allow config changes until new connection established
    m_configLocked = false;
}
