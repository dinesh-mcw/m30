# frontend

This is the Linux executable that reads data from Video for Linux (V4L2) and sends it to the raw to depth pipeline.

## Command line options

| Option                     | Description |
|----------------------------|-------------|
| `-l, --local-port=PORT`    | Set the TCP port (default 1234) of the connection that controls the front end |
| `-b, --base-port=PORT`     | Set the TCP base port (default 12566) used to output point cloud data |
| `-m, --mock-prefix=PATH`   | Enable mocking and get mock data from files with the name `<PATH>dddd.bin` where `dddd` is a sequence number starting from `0000`. The front end will play the mock files in sequence until a break in the sequence is found and then repeat the sequence again starting from `0000`. If you enable mocking, you must also specify the calibration file path using `--cal-path` |
| `-t, --mock-delay=DELAY`   | When mocking is enabled, set the delay (in milliseconds) between the times ROIs are presented to Raw2Depth |
| `-c, --cal-path=PATH`      | Get sensor mapping table from the specified path instead of the files provided by the system config and control (SCC) code |
| `-n, --num-heads=NUM`      | Set the maximum number of heads to enable; for the NCB, the maximum number of heads is 1 |
| `-o, --output-prefix=PATH` | enable raw output streaming to files; the output file names are '`PATH_h_ss_dddd.bin`' where `h` is the head number (0-3), `ss` is the session number, and `dddd` is the ROI number |
| `-r, --output-rois=NUM`    | stop network streaming after NUM MIPI frames; set to 0 to disable network output |
| `-h, --help`               | Get help |

You can get the command line options by executing

`sudo frontend -h`

On the NCB, you run as root so you don't need to use the `sudo` command.
If you want to run the front end at the command line you should stop the service first:

`sudo systemctl stop frontend`

This action will allow your instance of the front end to access the video driver and the listening port.

## Installing

First make sure you've built using cmake. From the top level raw2depth directory type:

```
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

Then install.

`sudo make install`

## Design overview

The front end provides four basic functions:
1. Configures the Video for Linux subsystem to receive video frames in the proper format
2. Gets the raw frames from Video for Linux and passes them to Raw2Depth
3. Provides a socket based interface to control its functions
4. Provides the main() function and command line interface for Raw2Depth

## Features

### Support mock files
If mock files are specified on the command line the front end sends mock data from files to Raw2Depth. You can specify a delay in milliseconds between frames to slow down the frame rate to realistic values. The front end remains in mock mode until it exits.

## Save video data as mock files
You tell the front end to save video data to disk in the same format as the mock files. This way you can capture real data and use it for reproducible testing later.

### Provide a control interface
The control interface allows the python system control code to control the front end. From the control interface you can:
- Start video streaming in the specified format on the specified head (only 1 head supported on NCB)
- Stop video streaming on the specified head
- Start raw streaming on the specified head (not currently working)
- Suspend raw streaming on the specified head (not currently working)
- Set the debug level (0 = no debug messages, level can have a value up to 7, higher levels have lower priority)

## Control Port

The System Config and Control Python code controls the front end using the control TCP socket. A client connects to the local-port (1234 by default) and sends a single byte command. This command can have the following bit formats (where C is the command, F is the format, and H is the head number):

| Bit format | Meaning                                     | Translation to thread command |
|------------|---------------------------------------------|-------------------------------|
| 00FFFFHH   | Start streaming with format FFFF on head HH | 0000FFFF or 0001FFFF          |
| 010000HH   | Stop streaming on head HH                   | 11110000                      |
| 100000HH   | Reload calibration data on head HH          | 11110001                      |
| 01001DDD   | Set the debug level to DD                   | N/A                           |
| 010100HH   | Reserved for future use                     | 11110100                      |
| 010101HH   | Reserved for future use                     | 11110101                      |
| 010110HH   | Resynchronize with 1PPS on head HH          | N/A                           |

The formats are defined in the Features section.

The head numbers are:

| Head ID | Channel |
|---------|---------|
|      00 | A       |
|      01 | B       |
|      10 | C       |
|      11 | D       |

On NCB, only head A is supported.

## Threads

There are up to six threads in the front end (only two on NCB):
- main thread - handles TCP communication with client and Linux signals
- time synchronization thread - sets up the OS for time synchronization
- sensor head 0-3 - handle gathering of data through device or mock file and sending to Raw2Depth

On the NCB, there is only a single sensor head so there are only two threads. The communication is between main thread and sensor head threads only. When multiple heads are supported, sensor head threads do not communicate with each other.

### Thread communication
Communication between the main thread and a sensor head thead is done by sending single bytes through a socket pair. The single byte eliminates framing. The socket pair allows us to use `select()` to get fully asynchronous operation. The socket pairs for the sensor head threads are created by the SensorHeadThread constructor. The two sockets are labelled as shown in the following table:

| Socket file descriptor name | Informal name           | Used by            |
|-----------------------------|-------------------------|--------------------|
| `m_trigFd`                  | Trigger file descriptor | main thread        |
| `m_waitFd`                  | Wait file descriptor    | sensor head thread |

These terms are chosen because the sensor head thread is passively waiting for commands triggered from the main thread.

### `select()` setup

The main thread loop is based on the Linux `select()` function. It is watching the following file descriptors:
| File descriptor name    | Description                                               |
|-------------------------|-----------------------------------------------------------|
| `s_signalFd`            | Signal file descriptor to allow the asynchronous signal handler to abort the main loop |
| `s_listenFd`            | The TCP socket listening on the local-port (default 1234) |
| trigger file descriptor | The main thread listens to one of these per sensor head thread. An unrequested byte from the trigger file descriptor indicates that the thread has shut down |

The sensor head thread `select()` function is watching the following file descriptors:
| Member name | Description |
|-------------|-------------|
| `m_waitFd`  | The sensor head thread is waiting for commands from the main thread that are sent through this socket |
| `m_videoFd` | The video driver file descriptor that triggers when a new MIPI frame is available |

### Sensor head thread commands
Commands that are received through TCP from Python are translated into thread commands. These are different from the control commands and must be translated. The `translateControlByte()` function in main.cpp performs this translation. The thread command bit formats are as follows:

| Bit format | Meaning |
|------------|---------|
| 0000FFFF   | Start streaming with format FFFF |
| 0001FFFF   | Start streaming with format FFFF and reload cal data |
| 0010XXXX   | Free commands with parameters |
| ...        | |
| 1110XXXX   | |
| 11110000   | Stop streaming |
| 11110001   | Reload calibration data |
| 11110010   | Exit thread |
| 11110011   | Nothing happened |
| 11110100   | Reserved for future use |
| 11110101   | Reserved for future use |
| 11110110   | Free commands without parameters |
| ...        | |
| 11111110   | |
| 11111111   | Error |

## Flows

### Start up flow
1. The `main()` function evaluates the command line arguments commands
2. The `main()` function calls `setUpListener()` to create, bind, and listen to the listening socket.
3. The `main()` function calls `setUpSignals()` to create a signal handler for SIGINT, SIGTERM, and SIGPIPE and socket pair for the signal handler to communicate with the main thread
4. The `main()` function creates a V4LSensorHeadThread object for each sensor head and stores a shared pointer to the object in the `s_shThreads` array. This array is declared static so that other functions in the file can access it.
5. The constructor of the sensor head thread object creates the socket pair used to communicate between the main thread and the sensor head thread.
6. The `main()` function creates a thread object for each sensor head and stores a shared pointer to the object in the `threads` array. The thread takes a weak reference to the V4LSensorHeadThread object so it call the `run()` method in the object.
7. As a result of creating each sensor head thread, the `run()` method is executed in each thread.
8. The `main()` function calls the `eventLoop` function, which sets up a file descriptor set for the `select()` function and the calls `select()`, which blocks until there is data available to read on one of the file descriptors in the file descriptor set.

### Command handling flow
1. The main thread is waiting for the `select()` call in the `eventLoop()` function.
2. The `select()` call exits when the application receives a request to connect through the listening socket.
2. The `eventLoop()` function calls the `handleListenEvent()` event handler.
3. The `handleListenEvent()` function accepts the listening socket, creating the accepted socket, and calls `handleAccept()`.
4. The `handleAccept()` function calls receives the command (1 byte) from the accepted socket.
5. The `handleAccept()` function calls `translateControlByte()` to translate the 1-byte command to another 1-byte thread command and determines the thread to send it to
6. The `handleAccept()` function calls the `handleControlByte()` function, that sends the translated command to the trigger file descriptor for the specified thread
7. The sensor head thread has been sitting in the `select()` call of the `run()` function. The `select()` call exits and upon seeing that the wait file descriptor has read data, the `run()` function calls `receiveNotification()` to get the control byte from the wait file descriptor.
8. The sensor head thread `run()` function calls `handleNotification()` to execute the command
9. The sensor head thread `handleNotification()` function executes the command and sends the 1 byte thread command back to its wait file descriptor
10. The `handleAccept()` function running in the main thread receives the 1 byte reply from the trigger file descriptor and verifies it matches the command it sent
11. The `handleAccept()` function running in the main thread shuts down and closes the accepted socket and returns to the `eventLoop()` main loop.

### Shutdown flow
Shutdown only occurs under one of the following situations:
- All of the sensor head threads crash
- One of SIGINT (ctrl-C) or SIGTERM (kill) occur. This allows us to cleanly shutdown using ctrl-C, killall, or systemd

The sequence of events on signals is as follows:

1. Signal occurs and is handled by `handleSignal()`
2. `handleSignal()` sends a byte to the signal file descriptor, which is paired with the exit file descriptor
3. The main loop `select()` sees the byte on the exit file descriptor and executes `handleSignalEvent()`.
4. `handleSignalEvent()` sends an exit command byte (0b11110010 or 0xf2) to all of the sensor head threads
5. The `select()` in each of the sensor head thread main loops exit, thus causing the threads to die
6. As each thread dies, it sends a byte back to the main thread. Because this is not sent in response to a command, the main loop executes the `handleThreadEvent()` function with the thread's trigger file descriptor as its argument
7. `handleThreadEvent()` clears the file descriptor in the `s_threadFds` array
8. Because the `select()` function fired in the main loop, the main loop checks if all of the file descriptors in the `s_threadFds` array have been cleared, if so, the main loop exits
9. The main thread sends the exit command byte (0b11110010 or 0xf2) to all of the sensor head threads. This is just a precaution
10. The main thread joins all of the sensor head threads, putting them in the detached state
11. The main thread sets the `s_shThreads` array member for the thread to `nullptr` to free the `SensorHeadThread` object
12. The main function exits letting the threads array go out of scope, cleaning up the threads

## Format of frame data (and mock data)

The data is in the form of MIPI frames. Each frame consists of lines.  Each line is `640 pixels * 3 taps per pixel * sizeof(uint16_t) = 3880 bytes`

The LIDAR system groups lines into _regions of interest_ (ROIs). A ROI can comprise an entire MIPI frame or part of it.

Every ROI is accompanied by and starts with one line of metadata. Every ROI consists of multiple sub ROIs. An ROI can break down into either 6 sub ROIs (known as DMFD) or 2 sub ROIs (known as Tap Accumulated). Each sub ROI can be 20 lines, 8 lines or 6 lines.

Therefore ROIs can have the following number of lines:

| sub ROI length | number of sub ROIs | number of metadata lines | total lines in ROI and MIPI frame |
|----------------|--------------------|--------------------------|-----------------------------------|
| 20             | 6                  | 1                        | 121                               |
| 20             | 2                  | 1                        | 41                                |
| 8              | 6                  | 1                        | 49                                |
| 8              | 2                  | 1                        | 17                                |
| 6              | 6                  | 1                        | 37                                |
| 6              | 2                  | 1                        | 13                                |

We can also aggregate multiple ROIs in a single MIPI frame. This feature allows us to reduce the interrupt rate that the compute module needs to handle. It can also help to reduce any per MIPI frame overhead the compute system may have. The driver and front end supports formats with with the tap accumulated ROIs aggregated 10 at a time:

|sub ROI length | number of sub ROIs | number of metadata lines | total number of lines in MIPI frame |
|---------------|--------------------|--------------------------|-------------------------------------|
| 20            | 2                  | 1                        | 41 * 10 = 410                       |
| 8             | 2                  | 1                        | 17 * 10 = 170                       |
| 6             | 2                  | 1                        | 13 * 10 = 130                       |

Additionally the front end can support the fully raw output from the sensor, which is 480 lines in DMFD format. The full set of MIPI frame formats supported by the front end matches exactly the formats supported by the m30 kernel driver.

| Format ID | Pixel Format | Aggregation | Subframes | Lines | Metadata | Description |
|-----------|--------------|-------------|-----------|-------|----------|-------------|
|         0 | 1280 x 2881  | 1           | 6         | 2880  | 1        | DMFD Full frame |
|         1 | 1280 x 121   | 1           | 6         | 20    | 1        | DMFD 20 line ROI |
|         2 | 1280 x 41    | 1           | 2         | 20    | 1        | Tap Accum 20 line ROI |
|         3 | 1280 x 410   | 10          | 2         | 20    | 1        | Tap Accum 20 line ROI with 10 aggregation |
|         4 | 1280 x 49    | 1           | 6         | 8     | 1        | DMFD 8 line ROI |
|         5 | 1280 x 17    | 1           | 2         | 8     | 1        | Tap Accum 8 line ROI |
|         6 | 1280 x 170   | 10          | 2         | 8     | 1        | Tap Accum 8 line ROI with 10 aggregation |
|         7 | 1280 x 37    | 1           | 6         | 6     | 1        | DMFD 6 line ROI |
|         8 | 1280 x 13    | 1           | 2         | 6     | 1        | Tap Accum 6 line ROI |
|         9 | 1280 x 130   | 10          | 2         | 6     | 1        | Tap Accum 6 line ROI with 10 aggregation |

We can define up to six additional formats as necessary.

## Classes

### SensorHeadThread
The `SensorHeadThread` class is an abstract (although not purely abstract) superclass to the concrete V4LSensorHeadThread and MockSensorHead subclasses. The superclass provides the following functionality:
- Creating the RawToFovs and CobraNetPipelineWrapper objects that encapsulate the Raw2Depth code.
- Communicating with the main thread via the thread's socket pair
- Saving the frame to disk if the --output-rois command line option is specified
- Breaking down aggregated MIPI frames into individual ROIs
- Sending individual ROIs to Raw2Depth
### MockSensorHeadThread
The `MockSensorHeadThread` class reads mock files and sends their contents to the superclass. Each mock file contains data for a single ROI.
A sequence of mock files have file names that end in `dddd.bin` where `d` is a decimal digit. The mock code first reads from `<path_prefix>0000.bin`, then `<path_prefix>0001.bin`, and keeps incrementing until it encounters a file name that doesn't exist. Then it goes back to `<path_prefix>0000.bin` again. Of course if the `<path_prefix>0000.bin` file doesn't exist, a fatal error occurs and the thread aborts.

In the mock sensor head thread no video devices are opened and all data sent to Raw2Depth comes from mock files. The control port is ignored except to shut down the thread on signal.
### V4LSensorHeadThread
The V4LSensorHeadThread is responsible for starting Video for Linux streaming in a specified format at the request of the main thread. It also shuts down the streaming. It receives raw frames from Video for Linux in the form of a pointer and size, which it duly passes on to Raw2Detph. It also detects dropped MIPI frames using the ROI counter in the ROI metadata and adjusts the timestamps received in the MIPI metadata to UTC.
### TimeSync
The Timesync class is responsible for tasks related to time synchronization
1. Start up a thread that initializes the OS to enable time synchronization based on either PTP or an external 1PPS signal
2. Synchronize the FPGA timestamps with UTC
