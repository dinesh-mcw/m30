#ifndef NETWORK_STREAMER_HPP
#define NETWORK_STREAMER_HPP

/**
 * @file network_streamer.hpp
 * @brief This file contains the definitions for the NetworkStreamer,
 *        UDPStreamer, and TCPWrappedStreamer classes.
 *        2. UDPStreamer, a subclass of NetworkStreamer that sends data
 *           over UDP
 *        3. TCPWrappedStreamer, a subclass of NetworkStreamer that sends
 *           data over TCP
 *        The UDPStreamer is not supported at this time.
 *        
 * @copyright Copyright (C) 2023 Lumotive, Inc. All rights reserved
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <random>
#include <memory>
#include "pipeline_modules.hpp"

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#endif

#define RAWDATA_PAYLOAD_MAX_SIZE        ROI_SIZE
#define PROCESSEDDATA_PAYLOAD_MAX_SIZE  1472
#define FRAMEING_HEADER_SIZE 16 
#define TCP_SERVE_BACKLOG 20
#define NUM_CHANNELS_PER_TYPE2_PACKET 64

using namespace LidarPipeline;

/**
 *  @brief NetworkStreamer is a pipeline module and therefore a subclass of the
 *         PipelineModule class. It is designed to send depth data over the network
 *         in Lumotive's packet format.
 */
class NetworkStreamer : public PipelineModule {
    public:
        bool setDeviceID(uint32_t deviceID);
        void set_dbg_MaxFrames(int maxFrames);
    protected:
        NetworkStreamer(
            uint32_t deviceVersion,
            uint32_t deviceID,
            PipelineOutputType outputType);
        void WorkOnSingleChunk(ReturnChunk *chunk) override;
        virtual void StartROISend();
        virtual void FinishROISend();
        bool m_configLocked;
        virtual void UpdateClientMeta();
        void net_perror(const char * className, const char * netOp, const char * msg);
    private:
        virtual void NetworkSend(char* buffer, size_t len) = 0;
        virtual void NetworkROISend(char* roi, size_t len) = 0;
        void WorkOnCPIChunk(ReturnChunk *chunk);
        void WorkOnROIChunk(ReturnChunk *chunk);
        std::array<char, PROCESSEDDATA_PAYLOAD_MAX_SIZE> m_payloadBufferSpace;
        uint32_t m_deviceVersion;
        uint32_t m_deviceID;
        uint32_t m_seq;
        bool m_lastSceneSeqsValid;
        bool m_thisSceneSeqsValid;
        uint32_t m_lastSceneBeginSeq;
        uint32_t m_lastSceneEndSeq;
        uint32_t m_thisSceneBeginSeq;
        uint32_t m_thisSceneLastSeq;
        std::shared_ptr<std::vector<int32_t>> m_calibrationX;
        std::shared_ptr<std::vector<int32_t>> m_calibrationY;
        std::shared_ptr<std::vector<int32_t>> m_calibrationTheta;
        std::shared_ptr<std::vector<int32_t>> m_calibrationPhi;
    protected:
        int m_dbg_maxFrames;
        bool m_dbg_maxFramesActive;
        uint64_t m_dbg_maxFramesRemaining;
        PipelineOutputType m_outputType;
        std::string m_verbosePrefix;
};

/**
 *  @brief UDPStreamer is a NetworkStreamer used to send data over UDP. It is not currently
 *         supported.
 */
class UDPStreamer : public NetworkStreamer {
    public:
        UDPStreamer(
            uint32_t deviceVersion,
            uint32_t deviceID,
            const char* targetHost,
            uint16_t targetPort,
            PipelineOutputType outputType);
    private:
        void NetworkSend(char* buffer, size_t len) override;
        int m_fd;
        struct sockaddr_in m_targetAddr;
};

/**
 *  @brief TCPWrappedStreamer is a NetworkStreamer used to send data over TCP.
 */
class TCPWrappedStreamer : public NetworkStreamer {
    public:
        TCPWrappedStreamer(
            uint32_t deviceVersion,
            uint32_t deviceID,
            uint16_t tcpPort,
            PipelineOutputType outputType);
        TCPWrappedStreamer(
            uint32_t deviceVersion,
            uint32_t deviceID,
            uint16_t tcpPort,
            ssize_t minSockBuffer,
            PipelineOutputType outputType);
    void StartROISend() override;
    void FinishROISend() override;
    private:
        void NetworkSend(char* buffer, size_t len) override;
        void NetworkROISend(char* roi, size_t len) override;
        bool AcceptNewConnection();
        void CloseConnection();
        int m_listenfd;
        int m_clientfd;
        ssize_t m_reqSockBuffer;
        struct sockaddr_in m_servAddr;
        struct sockaddr_in m_clientAddr;
        socklen_t m_clientAddrLen;
        std::array<char, PROCESSEDDATA_PAYLOAD_MAX_SIZE + FRAMEING_HEADER_SIZE> m_tcpBufferSpace;
        std::array<char, RAWDATA_PAYLOAD_MAX_SIZE + FRAMEING_HEADER_SIZE> m_tcpROIBufferSpace;
};

#endif
