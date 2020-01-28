/* 

    Functions and structs that can be used for TFTP
    Copyright (C) 2020 Johannes Draaijer

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 
 */

#ifndef TFTPSERVER_PACKET_H
#define TFTPSERVER_PACKET_H

#include <stdint.h>

#define TFTP_OPCODE_READ_REQUEST 1
#define TFTP_OPCODE_WRITE_REQUEST 2
#define TFTP_OPCODE_DATA 3
#define TFTP_OPCODE_ACKNOWLEDGEMENT 4
#define TFTP_OPCODE_ERROR 5
#define TFTP_OPCODE_MIN TFTP_OPCODE_READ_REQUEST
#define TFTP_OPCODE_MAX TFTP_OPCODE_ERROR

enum TftpPacketEndianness {
    NETWORK = 1,
    HOST = 2
};

typedef struct {
    enum TftpPacketEndianness endianness;
    uint16_t *opcode;
    int length;

    int bufferSize;
    uint8_t *buffer;

    int contentsLength;
    uint8_t *content;
} TftpPacket;

typedef struct {
    TftpPacket *packet;
    char *filename;
    char *mode;
} TftpPacketRequest;

typedef TftpPacketRequest TftpPacketWriteRequest;
typedef TftpPacketRequest TftpPacketReadRequest;

typedef struct {
    TftpPacket *packet;
    uint16_t *blockNum;
    int dataLength;
    uint8_t *data;
} TftpPacketData;

typedef struct {
    TftpPacket *packet;
    uint16_t *blockNum;
} TftpPacketAck;

typedef struct {
    TftpPacket *packet;
    char *message;
} TftpPacketError;

int tftp_init_packet(TftpPacket *, uint16_t, uint8_t *, int);

int tftp_init_data(TftpPacket *packet, TftpPacketData *data);

int tftp_free(TftpPacket *);

int tftp_test_string(const char *possibleString, int);

int tftp_parse_packet(uint8_t *, int, int, TftpPacket *);

int tftp_parse_req(TftpPacket *, TftpPacketRequest *);

int tftp_parse_rrq(TftpPacket *, TftpPacketReadRequest *);

int tftp_parse_wrq(TftpPacket *, TftpPacketWriteRequest *);

int tftp_parse_data(TftpPacket *, TftpPacketData *);

int tftp_parse_ack(TftpPacket *, TftpPacketAck *);

int tftp_parse_error(TftpPacket *, TftpPacketError *);

int tftp_serialize(TftpPacket *, int);

#endif //TFTPSERVER_PACKET_H
