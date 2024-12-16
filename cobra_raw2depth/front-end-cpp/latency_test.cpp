#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <LumoLogger.h>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <vector>

static constexpr short STREAMING_PORT              { 12566 };
static constexpr short API_PORT                    { 80 };
static constexpr short USER_TAG_OFFSET             { 80 };

static constexpr int MAX_REQUEST_SIZE              { 1024 };
static constexpr int MAX_RESPONSE_SIZE             { 1024 };
static constexpr int MAX_PACKET_SIZE               { 1024 };
static constexpr int TCP_HEADER_W_MAGIC_SIZE       { 20 };
static constexpr int TCP_HEADER_SIZE               { 16 };
static constexpr int LENGTH_REMAINING_CORRECTION   { 4 };
static constexpr int TYPE_C_LENGTH                 { 549 };    // bad alignment
static constexpr int TYPE_D_LENGTH                 { 706 };    // bad alignment
static constexpr int WAIT_FOR_THREAD_SEC           { 2 };
static constexpr int USER_TAG                      { 10 };
static constexpr int MAX_USER_TAG                  { 99 };
static constexpr int MAX_TEST_OFFSET               { 549 + 16 };
static constexpr int TRIALS                        { 5 };
static constexpr long PACKET_MAGIC                 { 0x42434441 };
static constexpr int STRIPE_DSP_MODE               { 1 };
static constexpr int GRID_DSP_MODE                 { 0 };
static const char *IP_ADDR                         { "127.0.0.1" };
static constexpr int LATENCY_HISTORY_SIZE          { 10000 };
static constexpr int TARGET_FRAME_COUNT            { 10 };

static constexpr uint32_t NSECS_PER_SEC            { 1000000000U };
static constexpr uint32_t MAX_TIME_DIFFERENCE_NSEC {  500000000U };
static constexpr int TIME_OFFSET                   { 37 };
static constexpr int OFFSET_STEER_OFFSET           { 68 };
static constexpr int OFFSET_STARE_OFFSET           { 70 };
static constexpr int NS_FIELD_WIDTH                { 9 };

static constexpr int FAILURE_NONE                  { 0 };
static constexpr int FAILURE_IO                    { -1 };
static constexpr int FAILURE_BAD_PACKET_LENGTH     { -2 };

static const char *SCANPARAM_FORMAT =
    "POST /scan_parameters HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "User-Agent: latency_test/1.0.0\r\n"
    "Accept: */*\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 308\r\n"
    "Connection: close\r\n\r\n"
    "{\"interleave\":true,\"dsp_mode\":%1d,\"angle_range\":[[-45,45,1]],\"hdr_threshold\":4095,\"hdr_laser_power_percent\":5,\"hdr_inte_time_us\":1,\"user_tag\":[%2d],\"snr_threshold\":[1.25],\"nn_level\":[0],\"laser_power_percent\":[100],\"inte_time_us\":[15],\"binning\":[2],\"max_range_index\":[0],\"fps_multiple\":[1],\"frame_rate_hz\":[960]}\r\n";

// NOLINTBEGIN(hicpp-avoid-c-arrays) cleanest way to write test code
static const char START_FORMAT[] =
    "POST /start_scan HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "User-Agent: latency_test/1.0.0\r\n"
    "Accept: */*\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n\r\n";

static const char STOP_FORMAT[] =
    "POST /stop_scan HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "User-Agent: latency_test/1.0.0\r\n"
    "Accept: */*\r\n"
    "Content-Type: application/json\r\n"
    "Connection: close\r\n"
    "Content-Length: 0\r\n\r\n";
// NOLINTEND(hicpp-avoid-c-arrays) cleanest way to write test code

typedef struct {
    uint64_t tv_sec;
    uint32_t tv_nsec;
} ptp_time_t;

#define ZERO_TIME { 0, 0 }

struct thread_context {
    int result;
    const char *ip_addr;
    short r2d_port;
    int expected_tag;
    std::vector<ptp_time_t>& latency_history;
    std::vector<ptp_time_t>& absdiff_history;
    std::vector<ptp_time_t>& tbf_history;
    ptp_time_t last_pkt_time;
    ptp_time_t first_packet_in_frame_time;
    ptp_time_t min_photon_time;
    ptp_time_t max_tbf_time;
    ptp_time_t trial_start_time;
    int frame_count;
    int target_frame_count;
    bool expected_tag_found;
};

static void usage()
{
    std::cerr << "usage -- latency_test [-h <addr>] [-r <rawtodepth_port>] [-a <api_port>] [-u <user_tag>] [-n <trials>] [-d <dsp_mode>] [-f <frame_count>] [-?]\n" <<
                 "         addr            is the IP address of the NCB (default " << IP_ADDR << ")\n" <<
                 "         rawtodepth_port is the port number of the raw to depth output (default " << STREAMING_PORT << ")\n" <<
                 "         user_tag        must be between 0 and " << MAX_USER_TAG << " (default " << USER_TAG << ")\n" <<
                 "         trials          must be greater than 0 (default " << TRIALS << ")\n" <<
                 "         dsp_mode        0 for grid mode or nonzero for stripe mode (default)\n" <<
                 "         frame_count     number of frames per trial for latency statistics (default " << TARGET_FRAME_COUNT << ")\n";
}

static int open_stream(const char *ip_addr, short r2d_port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LLogErr("socket:errno=" << errno);
        return -1;
    }

    struct sockaddr_in addr {};

    addr.sin_family = AF_INET;
    int retVal = inet_pton(AF_INET, ip_addr, &addr.sin_addr.s_addr);
    if (retVal != 1) {
        std::cerr << "inet_pton:retVal=" << retVal;
        close(sock);
        return -1;
    }
    addr.sin_port = htons(r2d_port);

    if (connect(sock, (const sockaddr *)&addr, sizeof(addr)) < 0) {
        LLogErr("connect:errno=" << errno);
        close(sock);
        return -1;
    }

    return sock;
}

static int read_from_sock(int sock, std::array<uint8_t, MAX_PACKET_SIZE> &buf, int offset, ssize_t bytes)
{
    ssize_t index = 0;
    while (index < bytes) {
        ssize_t numRead = recv(sock, buf.data() + offset + index, bytes - index, 0);
        if (numRead < 0) {
            LLogErr("recv:errno=" << errno);
            return -1;
        }
        index += numRead;
    }
    return 0;
}

/**
 * @brief Synchronizes the TCP stream with the point cloud packets
 *
 * You enter this function after reading 20 bytes of data from the TCP socket and finding
 * that the TCP header and magic number, which should just fit in those 20 bytes are not
 * correct. The length field must contain either 459 (the length of a type C packet) or
 * 706 (the length of a type D packet), and the magic number field must contain 'BCDA'.
 * 
 * Now we must examine the stream one byte at a time to see if we can find 20 bytes of data
 * that conform to the specifications above. We do this by reading 20 more bytes into the
 * buffer so we now have 40 bytes. We then loop starting from position 1 and check if the
 * header located in the bytes at locations 1 to 20 inclusive conforms to the specifications
 * above. If not, we check the bytes at locations 2 to 21, all the way up to 20 to 39.
 * 
 * If we don't find a match, we move the last 20 bytes of the buffer to position 0 and
 * repeat the loop.
 *
 * If we find a match, we move the bytes from the first position of the matching header to
 * byte 39 of the buffer to the beginning so the header is at position 0. Then we carefully
 * calculate how many bytes of the packet are remaining and read those from the socket over
 * TCP. This means that when the function is finished, a good packet can be found in the
 * buffer.
 *
 * @param sock File descriptor for TCP socket from which we are receiving
 * @param buf  C++ array of characters to store the resulting packet
 * @return The length of the next packet. buf contains a good packet
 **/
static ssize_t synchronize_with_stream(int sock, std::array<uint8_t, MAX_PACKET_SIZE>& buf)
{
    ssize_t index;
    ssize_t length;
    ssize_t remaining;

    bool synchronized = false;
    while (!synchronized) {
        // read another block
        if (read_from_sock(sock, buf, TCP_HEADER_W_MAGIC_SIZE, TCP_HEADER_W_MAGIC_SIZE) < 0) {
            return -1;
        }

        // Now we have 2 * TCP_HEAD_W_MAGIC_SIZE bytes in buf.data()
        int offset = -1;
        for (int s_index = 1; s_index <= TCP_HEADER_W_MAGIC_SIZE && offset < 0; s_index++) {       // N.B. the limits appear to be off by 1 but they are not!
            // NOLINTBEGIN(readability-magic-numbers) Constructing numbers from bytes is most naturally expressed this way
            length = (buf[s_index] << 24LU) +
                     (buf[s_index + 1] << 16LU) +
                     (buf[s_index + 2] << 8LU) +
                     buf[s_index + 3];
            long magic = (buf[s_index + TCP_HEADER_SIZE] << 24LU) +
                         (buf[s_index + TCP_HEADER_SIZE + 1] << 16LU) +
                         (buf[s_index + TCP_HEADER_SIZE + 2] << 8LU) +
                         buf[s_index + TCP_HEADER_SIZE + 3];
            // NOLINTEND(readability-magic-numbers)
            if ((length == TYPE_C_LENGTH || length == TYPE_D_LENGTH) && magic == PACKET_MAGIC) {
                // found it; make note of the offset
                offset = s_index; // this will break out of the loop
            }
        }

        if (offset >= 0) {
            // now we move the data back so the length is at the beginning
            for (int m_index = offset; m_index < TCP_HEADER_W_MAGIC_SIZE * 2; m_index++) {
                buf[m_index - offset] = buf[m_index];
            }
            remaining = length - (TCP_HEADER_W_MAGIC_SIZE + LENGTH_REMAINING_CORRECTION - offset);

            // read the remaining stuff
            if (read_from_sock(sock, buf, TCP_HEADER_W_MAGIC_SIZE * 2 - offset, remaining) < 0) {
                return -1;
            }
            synchronized = true; // now we break from outer loop
        } else {
            // we didn't find it. Move the second half to the first half
            for (int m_index = 0; m_index < TCP_HEADER_W_MAGIC_SIZE; m_index++) {
                buf[m_index] = buf[m_index + TCP_HEADER_W_MAGIC_SIZE];
            }
        }
    }
    return length;
}

// Now with synchronization!
static int get_packet(int sock, std::array<uint8_t, MAX_PACKET_SIZE>& buf)
{
    // assume we are synchronized
    if (read_from_sock(sock, buf, 0, TCP_HEADER_W_MAGIC_SIZE) < 0) {
        return -1;
    }

    // check we are really synchronized
    ssize_t length = ntohl(*(int32_t *)buf.data());
    long magic = ntohl(*(int32_t *)(buf.data() + TCP_HEADER_SIZE));
    if ((length == TYPE_C_LENGTH || length == TYPE_D_LENGTH) && magic == PACKET_MAGIC) {
        // we are synchronized
        ssize_t remaining = length - LENGTH_REMAINING_CORRECTION;
        if (read_from_sock(sock, buf, TCP_HEADER_W_MAGIC_SIZE, remaining) < 0) {
            return -1;
        }
    } else {
        // we are not synchronized; try to synchronize
        length = synchronize_with_stream(sock, buf);
    }

    return (int)length;
}

// Compares two unsigned times and returns -1 if time1 < time2, 0 if time1 == time2, and 1 if time1 > time2
static int time_cmp(const ptp_time_t& time1, const ptp_time_t& time2)
{
    int ret = 0;

    // check coarse first
    if (time1.tv_sec > time2.tv_sec) {
        ret = 1;
    } else if (time1.tv_sec < time2.tv_sec) {
        ret = -1;
    } else {
        // need to check fine
        if (time1.tv_nsec > time2.tv_nsec) {
            ret = 1;
        } else if (time1.tv_nsec < time2.tv_nsec) {
            ret = -1;
        } else {
            ret = 0;
        }
    }
    return ret;
}

// Calculates the absolute difference between two unsigned times
static void time_absdiff(ptp_time_t& absdiff, const ptp_time_t& time1, const ptp_time_t& time2)
{
    absdiff.tv_sec = 0;
    absdiff.tv_nsec = 0;
    if (time1.tv_sec == time2.tv_sec) {
        if (time1.tv_nsec < time2.tv_nsec) {
            absdiff.tv_sec = 0;
            absdiff.tv_nsec = time2.tv_nsec - time1.tv_nsec;
        } else {
            absdiff.tv_sec = 0;
            absdiff.tv_nsec = time1.tv_nsec - time2.tv_nsec;
        }
    } else {
        if (time1.tv_sec > time2.tv_sec) {
            if (time1.tv_nsec < time2.tv_nsec) {
                // borrow
                absdiff.tv_sec = time1.tv_sec - time2.tv_sec - 1;
                absdiff.tv_nsec = NSECS_PER_SEC + time1.tv_nsec - time2.tv_nsec;
            } else {
                // no borrow
                absdiff.tv_sec = time1.tv_sec - time2.tv_sec;
                absdiff.tv_nsec = time1.tv_nsec - time2.tv_nsec;
            }
        } else {
            if (time1.tv_nsec > time2.tv_nsec) {
                // borrow
                absdiff.tv_sec = time2.tv_sec - time1.tv_sec - 1;
                absdiff.tv_nsec = NSECS_PER_SEC + time2.tv_nsec - time1.tv_nsec;
            } else {
                // no borrow
                absdiff.tv_sec = time2.tv_sec - time1.tv_sec;
                absdiff.tv_nsec = time2.tv_nsec - time1.tv_nsec;
            }
        }
    }
}

// Divides the absolute time by a positive constant
static void time_divn(ptp_time_t& quot, const ptp_time_t& dividend, uint32_t n)
{
    quot.tv_sec = dividend.tv_sec / n;
    uint64_t remainder_sec = dividend.tv_sec - quot.tv_sec * n;
    uint64_t low_dividend = ((uint64_t)remainder_sec * NSECS_PER_SEC) + dividend.tv_nsec;
    quot.tv_nsec = low_dividend / n;
}

// Calculates the sum of two unsigned times
static void time_add(ptp_time_t& sum, const ptp_time_t& time1, const ptp_time_t& time2)
{
    sum.tv_nsec = time1.tv_nsec + time2.tv_nsec;
    if (sum.tv_nsec >= NSECS_PER_SEC) {
        sum.tv_nsec -= NSECS_PER_SEC;
        sum.tv_sec = time1.tv_sec + time2.tv_sec + 1;
    } else {
        sum.tv_sec = time1.tv_sec + time2.tv_sec;
    }
}

#define TEST_CMP(_t1,_t2,_expected) { int val = time_cmp((_t1),(_t2)); if (val != (_expected)) { std::cerr << __LINE__ << ":compare:" << #_t1 << "," << #_t2 << ",actual=" << val << ",expected=" << (_expected) << std::endl; } }

#define TEST_OP2(_fn,_t1,_t2,_exp_sec,_exp_nsec) { ptp_time_t res_ptp = { 0, 0 }; (_fn)(res_ptp,(_t1),(_t2)); if (res_ptp.tv_sec != (_exp_sec) || res_ptp.tv_nsec != (_exp_nsec)) { std::cerr << __LINE__ << ":" << #_fn << ":" << #_t1 << "," << #_t2 << ",actual_sec=" << res_ptp.tv_sec << ",actual_nsec=" << res_ptp.tv_nsec << ",expected_sec=" << (_exp_sec) << ",expected_nsec=" << (_exp_nsec) << std::endl; } }

#define TEST_DIVN(_t1,_n,_exp_sec,_exp_nsec) { ptp_time_t res_ptp = { 0, 0 }; time_divn(res_ptp,(_t1),(_n)); if (res_ptp.tv_sec != (_exp_sec) || res_ptp.tv_nsec != (_exp_nsec)) { std::cerr << __LINE__ << ":time_divn:" << #_t1 << "," << (_n) << ",actual_sec=" << res_ptp.tv_sec << ",actual_nsec=" << res_ptp.tv_nsec << ",expected_sec=" << (_exp_sec) << ",expected_nsec=" << (_exp_nsec) << std::endl; } }

int test_time_math(void)
{
    // NOLINTBEGIN(readability-magic-numbers)
    ptp_time_t time5_5 = { 5000ULL, 500000000UL };      // 5000.5 sec
    ptp_time_t time5_6 = { 5000ULL, 600000000UL };      // 5000.6 sec
    ptp_time_t time5_6_2 = { 5000ULL, 600000000UL };    // 5000.6 sec copy
    ptp_time_t time4_6 = { 4000ULL, 600000000UL };      // 4000.6 sec
    ptp_time_t time4_4 = { 4000ULL, 400000000UL };      // 4000.4 sec

    // compare
    TEST_CMP(time5_5, time5_5, 0);
    TEST_CMP(time5_6, time5_6_2, 0);
    TEST_CMP(time5_6_2, time5_6, 0);
    TEST_CMP(time5_5, time5_6, -1);
    TEST_CMP(time5_6, time5_5, 1);
    TEST_CMP(time5_5, time4_6, 1);
    TEST_CMP(time4_6, time5_5, -1);
    TEST_CMP(time5_6, time4_6, 1);
    TEST_CMP(time4_6, time5_6, -1);

    // absdiff
    TEST_OP2(time_absdiff, time5_6, time4_6, 1000ULL, 0L);
    TEST_OP2(time_absdiff, time4_6, time5_6, 1000ULL, 0L);
    TEST_OP2(time_absdiff, time5_5, time4_4, 1000ULL, 100000000L); // no borrow
    TEST_OP2(time_absdiff, time5_5, time4_6, 999ULL, 900000000L); // borrow
    TEST_OP2(time_absdiff, time4_6, time5_5, 999ULL, 900000000L); // borrow
    TEST_OP2(time_absdiff, time5_5, time5_6, 0ULL, 100000000L); // fine subtract
    TEST_OP2(time_absdiff, time5_6, time5_5, 0ULL, 100000000L); // fine subtract
    TEST_OP2(time_absdiff, time5_6, time5_6_2, 0ULL, 0L); // equal
    TEST_OP2(time_absdiff, time5_6_2, time5_6, 0ULL, 0L); // equal

    // add
    TEST_OP2(time_add, time5_5, time5_5, 10001ULL, 0L);
    TEST_OP2(time_add, time5_6, time5_6, 10001ULL, 200000000L);
    TEST_OP2(time_add, time5_6, time5_6_2, 10001ULL, 200000000L);
    TEST_OP2(time_add, time5_5, time4_6, 9001ULL, 100000000L);
    TEST_OP2(time_add, time5_5, time4_4, 9000ULL, 900000000L); // no carry
    TEST_OP2(time_add, time4_4, time5_5, 9000ULL, 900000000L); // no carry
    TEST_OP2(time_add, time5_5, time5_6, 10001ULL, 100000000L);
    TEST_OP2(time_add, time5_6, time5_5, 10001ULL, 100000000L);
    TEST_OP2(time_add, time4_6, time5_6, 9001ULL, 200000000L);
    TEST_OP2(time_add, time5_6, time4_6, 9001ULL, 200000000L);

    // divn
    TEST_DIVN(time5_5, 2UL, 2500ULL, 250000000L); // no remainder
    TEST_DIVN(time5_6, 1000UL, 5ULL, 600000L);    // no remainder
    TEST_DIVN(time5_5, 15UL, 333ULL, 366666666L); // remainder
    TEST_DIVN(time5_6, 1003UL, 4ULL, 985643070L);
    // NOLINTEND(readability-magic-numbers)

    return 0;
}

static void get_now(ptp_time_t &now)
{
    struct timespec current_time = ZERO_TIME;
    if (clock_gettime(CLOCK_REALTIME, &current_time) < 0) {
        LLogErr("clock_gettime:errno=" << errno);
    } else {
        now.tv_sec = current_time.tv_sec;
        now.tv_nsec = current_time.tv_nsec;
    }
}

void get_packet_time(std::array<uint8_t, MAX_PACKET_SIZE> &data, ptp_time_t& pkt_time)
{
    // measure the current time as soon as we can
    // determine the time photon time from packet
    // NOLINTBEGIN(readability-magic-numbers) Endian conversion is best expressed with literals
    pkt_time.tv_sec = (((uint64_t)data[TIME_OFFSET]) << 40U) +
                      (((uint64_t)data[TIME_OFFSET + 1]) << 32U) +
                      (((uint64_t)data[TIME_OFFSET + 2]) << 24U) +
                      (((uint64_t)data[TIME_OFFSET + 3]) << 16U) +
                      (((uint64_t)data[TIME_OFFSET + 4]) << 8U) +
                      ((uint64_t)data[TIME_OFFSET + 5]);
    pkt_time.tv_nsec = (data[TIME_OFFSET + 6] << 24U) +
                       (data[TIME_OFFSET + 7] << 16U) +
                       (data[TIME_OFFSET + 8] << 8U) +
                       data[TIME_OFFSET + 9];
    // NOLINTEND(readability-magic-numbers)
}

static inline int user_tag(std::array<uint8_t, MAX_PACKET_SIZE> &data)
{
    return (data[USER_TAG_OFFSET] << 8U) + data[USER_TAG_OFFSET + 1];     // NOLINT(readability-magic-numbers) Clearest expression of endian conversion
}

static inline bool is_first_packet_in_frame(std::array<uint8_t, MAX_PACKET_SIZE> &data)
{
    int offsetSteer = (data[OFFSET_STEER_OFFSET] << 8U) + data[OFFSET_STEER_OFFSET + 1]; // NOLINT(readability-magic-numbers) Clearest expression of endian conversion
    int offsetStare = (data[OFFSET_STARE_OFFSET] << 8U) + data[OFFSET_STARE_OFFSET + 1]; // NOLINT(readability-magic-numbers) Clearest expression of endian conversion
    return offsetSteer == 0 && offsetStare == 0;
}

static void print_time(const char *heading, ptp_time_t& ptp_time)
{
    std::cout << heading << ptp_time.tv_sec << "." << std::setw(NS_FIELD_WIDTH) << std::setfill('0') << ptp_time.tv_nsec << std::endl;
}

static bool handle_type_d_packet(std::array<uint8_t, MAX_PACKET_SIZE> &data, struct thread_context *context)
{
    ptp_time_t pkt_time = ZERO_TIME;
    ptp_time_t now = ZERO_TIME;

    get_now(now);
    get_packet_time(data, pkt_time);

    if (user_tag(data) == context->expected_tag) {
        // check if we should turn measure the api latency
        if (!context->expected_tag_found) {
            // stop the timer as soon as possible
            ptp_time_t api_time = ZERO_TIME;
            time_absdiff(api_time, now, context->trial_start_time);
            context->latency_history.push_back(api_time);
            LLogInfo("expected_user_tag_found::tag=" << context->expected_tag);
            context->expected_tag_found = true;
        }

        // handle first packet in frame
        if (is_first_packet_in_frame(data)) {
            // first frame?
            if (context->frame_count < 0) {
                // don't have enough data to calculate latencies
                context->frame_count = 0;
            } else {
                // calculate photon to frame latency
                ptp_time_t latency = ZERO_TIME;
                time_absdiff(latency, context->first_packet_in_frame_time, context->min_photon_time);
                context->absdiff_history.push_back(latency);
                context->frame_count ++;
            }
            context->min_photon_time = pkt_time;
            context->first_packet_in_frame_time = now;

            if (context->frame_count >= context->target_frame_count) {
                // save the max time between frames
                context->tbf_history.push_back(context->max_tbf_time);
                return true; // exit the thread
            }
        }

        // maintain the earliest photon time for this frame
        if (time_cmp(pkt_time, context->min_photon_time) < 0) {
            context->min_photon_time = pkt_time;
        }
    }

    if (time_cmp(context->last_pkt_time, ZERO_TIME) > 0) {
        ptp_time_t time_between_frames = ZERO_TIME;
        time_absdiff(time_between_frames, now, context->last_pkt_time);
        if (time_cmp(time_between_frames, context->max_tbf_time) > 0) {
            context->max_tbf_time = time_between_frames;
        }
    }
    context->last_pkt_time = now;

    return false;
}

static void *thread_run(void *context_in)
{
    auto *context = (struct thread_context *)context_in;
    std::array<uint8_t, MAX_PACKET_SIZE> data {};

    int ssock = open_stream(context->ip_addr, context->r2d_port);
    if (ssock < 0) {
        context->result = FAILURE_IO;
        return context;
    }
    int test_offset = 0;
    while (true) {
        int length = get_packet(ssock, data);
        if (length < 0) {
            context->result = FAILURE_IO;
            break;
        }
        if (length == TYPE_D_LENGTH) {
            if (handle_type_d_packet(data, context)) {
                break;
            }
        } else if (length == TYPE_C_LENGTH) {
            // here we do a test of the synchronizer
            if (test_offset < MAX_TEST_OFFSET) {
                if (read_from_sock(ssock, data, 0, test_offset) < 0) {
                    context->result = FAILURE_IO;
                    break;
                }
                test_offset++;
            }
        } else {
            // This should never happen because of the design of the synchronizer
            LLogErr("unknown_length:length=" << length);
            context->result = FAILURE_BAD_PACKET_LENGTH;
            break;
        }
    }

    shutdown(ssock, SHUT_RDWR);
    close(ssock);
    return context;
}

static int rest_send_request(const char *ip_addr, const short api_port, const char *req, int req_len, std::array<char, MAX_RESPONSE_SIZE>& resp)
{
    // create the socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LLogErr("socket:errno=" << errno);
        return -1;
    }

    // set the connect address
    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    int retVal = inet_pton(AF_INET, ip_addr, &addr.sin_addr.s_addr);
    if (retVal != 1) {
        LLogErr("inet_pton:ip_addr=" << ip_addr << ",retVal=" << retVal);
        close(sock);
        return -1;
    }
    addr.sin_port = htons(api_port);

    if (connect(sock, (const sockaddr *)&addr, sizeof(addr)) < 0) {
        LLogErr("connect:errno=" << errno);
        close(sock);
        return -1;
    }

    // send the entire packet
    ssize_t index = 0;
    while (index < req_len) {
        ssize_t bytesSent = send(sock, req, req_len - index, 0);
        if (bytesSent < 0) {
            LLogErr("send:errno=" << errno);
            close(sock);
            return -1;
        }
        index += bytesSent;
    }

    // get response
    index = 0;
    while (index < resp.size()) {
        ssize_t bytesReceived = recv(sock, resp.data() + index, resp.size() - index, 0);
        if (bytesReceived < 0) {
            LLogErr("recv:errno=" << errno);
            close(sock);
            return -1;
        }
        if (bytesReceived == 0) {
            break;
        }
        index += bytesReceived;
    }

    close(sock);
    return 0;
}

static int time_cmp_qsort(const void *time1, const void *time2) {
    return time_cmp(*(ptp_time_t*)time1, *(ptp_time_t*)time2);
}

static void print_stats(const char *name, std::vector<ptp_time_t> stat_data)
{
    std::cout << "statistics for " << name << std::endl;

    int hSize = (int)stat_data.size();
    if (hSize > 0) {
        qsort(stat_data.data(), hSize, sizeof(ptp_time_t), time_cmp_qsort);
        print_time("min=", stat_data[0]); // NOLINT(readability-container-data-pointer) This is more readable than stat_data.data()
        print_time("max=", stat_data[hSize - 1]);

        // calculate mean
        ptp_time_t sum = ZERO_TIME;
        for (unsigned int index = 0; index < hSize; index++) {
            time_add(sum, sum, stat_data[index]);
        }
        ptp_time_t result = ZERO_TIME;
        time_divn(result, sum, hSize);
        print_time("mean=", result);

        // calculate median
        if (hSize % 2 == 1) {
            print_time("median=", stat_data[hSize / 2]);
        } else {
            ptp_time_t sum;
            ptp_time_t result;
            time_add(sum, stat_data[hSize / 2], stat_data[hSize / 2 - 1]);
            time_divn(result, sum, 2);
            print_time("median=", result);
        }
    }

    std::cout << std::endl;
}

int test_one_time(const char *ip_addr, short r2d_port, short api_port,
                  int trials, int target_frame_count,
                  short user_tag,
                  int dsp_mode,
                  std::vector<ptp_time_t>& latency_history,
                  std::vector<ptp_time_t>& absdiff_history,
                  std::vector<ptp_time_t>& tbf_history)
{
    std::array<char, MAX_REQUEST_SIZE> req {};
    std::array<char, MAX_RESPONSE_SIZE> resp {};

    // first send scan parameters
    int len = snprintf(req.data(), req.size(), SCANPARAM_FORMAT, dsp_mode, user_tag); // NOLINT(hicpp-vararg) Simplest way to create formatted string with the correct length
    if (rest_send_request(ip_addr, api_port, req.data(), len, resp) < 0) {
        return FAILURE_IO;
    }

    // send start
    if (rest_send_request(ip_addr, api_port, (const char *)START_FORMAT, sizeof(START_FORMAT) - 1, resp) < 0) {
        return FAILURE_IO;
    }

    sleep(WAIT_FOR_THREAD_SEC);

    // start a thread to receive the data
    pthread_t thread {};
    struct thread_context context {
        .result = FAILURE_NONE,
        .ip_addr = ip_addr,
        .r2d_port = r2d_port,
        .expected_tag = (user_tag + 1) % (MAX_USER_TAG + 1),
        .latency_history = latency_history,
        .absdiff_history = absdiff_history,
        .tbf_history = tbf_history,
        .last_pkt_time = ZERO_TIME,
        .max_tbf_time = ZERO_TIME,
        .frame_count = -1,
        .target_frame_count = target_frame_count,
        .expected_tag_found = false,
    };

    int err = pthread_create(&thread, NULL, thread_run, &context);
    if (err != 0) {
        std::cerr << "pthread_create:err=" << err << std::endl;
        return FAILURE_IO;
    }

    // wait for thread to connect
    sleep(WAIT_FOR_THREAD_SEC);

    // NOLINTNEXTLINE(hicpp-vararg) Simplest way to create formatted string with correct length
    len = snprintf(req.data(), req.size(), (const char *)SCANPARAM_FORMAT, dsp_mode, context.expected_tag);

    LLogInfo("sending API request");
    // grab the current time
    get_now(context.trial_start_time);

    // send scan parameters with updated user tag
    if (rest_send_request(ip_addr, api_port, req.data(), len, resp) < 0) {
        std::cerr << "send_req_with_updated_tag:errno=" << errno << std::endl;
        return FAILURE_IO;
        pthread_cancel(thread);
        pthread_join(thread, NULL);
        return FAILURE_IO;
    }

    LLogInfo("received_reply");

    // Wait for thread to finish gathering data
    pthread_join(thread, NULL);
    if (context.result != FAILURE_NONE) {
        LLogErr("thread_return_val:result=" << context.result);
        return context.result;
    }
    rest_send_request(ip_addr, api_port, (const char *)STOP_FORMAT, sizeof(STOP_FORMAT) - 1, resp);

    return FAILURE_NONE;
}

int main(int argc, char *argv[])
{
    const char *ip_addr = IP_ADDR;
    short r2d_port = STREAMING_PORT;
    short api_port = API_PORT;
    short user_tag = USER_TAG;
    int trials = TRIALS;
    int dsp_mode = STRIPE_DSP_MODE;
    int target_frame_count = TARGET_FRAME_COUNT;
    int opt;

    while ((opt = getopt(argc, argv, "h:r:a:u:n:d:")) != -1) {
        switch(opt) {
        case 'h' :
            ip_addr = optarg;
            break;
        case 'r' :
            r2d_port = (short)atoi(optarg);
            break;
        case 'a' :
            api_port = (short)atoi(optarg);
            break;
        case 'u' :
            user_tag = (short)atoi(optarg);
            if (user_tag < 0 || user_tag > MAX_USER_TAG) {
                usage();
                return 1;
            }
            break;
        case 'n' :
            trials = atoi(optarg);
            if (trials < 0) {
                usage();
                return 1;
            }
            break;
        case 'd' :
            if (atoi(optarg) == 0) {
                dsp_mode = GRID_DSP_MODE;
            }
            break;
        case 'f' :
            target_frame_count = atoi(optarg);
            if (target_frame_count < 1) {
                 target_frame_count = TARGET_FRAME_COUNT;
            }
            break;
        default:
            usage();
            return 1;
        }
    }

    // test the time math functions
    if (test_time_math() < 0) {
        std::cerr << "time math tests failed" << std::endl;
        return 1;
    }

    std::vector<ptp_time_t> latency_history;
    latency_history.reserve(trials);

    std::vector<ptp_time_t> absdiff_history;
    absdiff_history.reserve((int)(target_frame_count * trials));

    std::vector<ptp_time_t> tbf_history;
    tbf_history.reserve(trials);

    for (int trial = 0; trial < trials; trial++) {
        int retVal = test_one_time(ip_addr, r2d_port, api_port,
                                   trials, target_frame_count,
                                   user_tag,
                                   dsp_mode,
                                   latency_history,
                                   absdiff_history,
                                   tbf_history);
        if (retVal < 0) {
            std::cerr << "failed:code=" << retVal << std::endl;
            return 1;
        }
    }

    print_stats("api latency", latency_history);
    print_stats("photon to packet latency", absdiff_history);
    print_stats("time between frames latency", tbf_history);

    return 0;
}

