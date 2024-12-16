#!/usr/bin/env python3

import socket
import numpy as np
import time

class ImgSock :
# Change this IP address to point to the jetson image server
    PORT = 1234

    COLS = 640
    ROWS = 2880
    TAPS_PER_PIXEL = 3
    NUM_SUBFRAMES = 6
    BYTES_PER_TAP = 2
    FRAME_SIZE_TAPS = COLS * ROWS * TAPS_PER_PIXEL * NUM_SUBFRAMES + COLS * TAPS_PER_PIXEL
    FRAME_SIZE_BYTES = FRAME_SIZE_TAPS * BYTES_PER_TAP

    MAX_READ_SIZE = 4096

    s = 0

    def open(this, ip, mode: int) :
        this.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        this.s.connect((ip, this.PORT))
        bytes = bytearray(1)
        bytes[0] = mode
        this.s.send(bytes)

    def getBufferOfBytes(this, expected: int) :
        bytesReceived = 0
        readBytes = bytearray(expected)

        while bytesReceived < expected :
            bytesToRead = expected - bytesReceived;
            if bytesToRead > this.MAX_READ_SIZE :
                bytesToRead = this.MAX_READ_SIZE
            ba = this.s.recv(bytesToRead);
            numBytes = len(ba)
            readBytes[bytesReceived : bytesReceived + numBytes] = ba
            bytesReceived += numBytes

        return readBytes

    def getFrame(this) :
        ba = this.getBufferOfBytes(4)
        if len(ba) != 4 :
            print("bad expected length")
            quit()

        expected = int.from_bytes(ba, "big") # Network order

        if expected > this.FRAME_SIZE_BYTES :
            print("frame too big: actual={} max={}".format(expected, this.FRAME_SIZE_BYTES))
            return None

        ba = this.getBufferOfBytes(expected)

        return np.frombuffer(ba, np.uint16)

    def getAck(this) :
        print("waiting for ack")
        ba = this.s.recv(1)
        print("received ack")

    def sendMetadata(this, ba) :
        this.s.send(ba)
        ba = this.getBufferOfBytes(1);
        if ba[0] != 0 :
            print("received incorrect bytes back");

    def close(this) :
        this.s.shutdown(socket.SHUT_RDWR)
        this.s.close()

def checkimg(img) :
    img_nm = img[1920:]
    for i in range(0, len(img_nm)) :
        if img_nm[i] != i & 0xffff :
            print("mismatch at i={}".format(i, i & 0xffff, img_nm[i]))

if __name__ == '__main__' :
    addr = "192.168.1.15"
    frameDesc = 1
    imgsock = ImgSock()
    imgsock.open(addr, mode = 0);
    imgsock.close();
    imgsock.open(addr, mode = frameDesc);
    while True :
        bytes = bytearray(imgsock.COLS * imgsock.TAPS_PER_PIXEL * imgsock.BYTES_PER_TAP)
        imgsock.sendMetadata(bytes);
        if frameDesc < 64 :
            img = imgsock.getFrame()
            print("size={} img={}".format(len(img), img))
        else :
            imgsock.getAck()

    imgsock.close()
