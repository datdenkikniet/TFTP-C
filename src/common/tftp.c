/* 

    Provide an implementation for tftp.h
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

#include <stddef.h>
#include <stdlib.h>
#include "tftp.h"

int tftp_test_string(const char *possibleString, int max) {
    int stringEndIndex = 0;
    while (stringEndIndex < max
           && *(possibleString + stringEndIndex) != 0) {
        stringEndIndex++;
    }
    if (stringEndIndex == max) {
        return -1;
    }
    return stringEndIndex;
}

int tftp_init_packet(TftpPacket *packet, uint16_t opcode, uint8_t *buffer, int bufferSize) {
    packet->buffer = buffer;
    packet->content = buffer + 2;
    packet->bufferSize = bufferSize;
    packet->opcode = (uint16_t *) packet->buffer;
    packet->endianness = HOST;
    packet->length = 2;

    *packet->opcode = opcode;
    return 0;
}

int tftp_init_data(TftpPacket *packet, TftpPacketData *data) {
    data->packet = packet;
    *packet->opcode = TFTP_OPCODE_DATA;
    data->blockNum = (uint16_t *) packet->content;
    data->data = packet->content + 2;
    data->dataLength = 0;
    return 0;
}

int tftp_free(TftpPacket *packet) {
    free(packet->buffer);
    return 0;
}

int tftp_parse_packet(uint8_t *buffer, int bufferSize, int bytesRead, TftpPacket *packet) {
    if (bytesRead < 2) {
        return -1;
    } else {

        uint8_t ocLs = buffer[0];
        buffer[0] = buffer[1];
        buffer[1] = ocLs;

        packet->buffer = buffer;
        packet->bufferSize = bufferSize;
        packet->opcode = (uint16_t *) buffer;

        if (*packet->opcode < TFTP_OPCODE_MIN || *packet->opcode > TFTP_OPCODE_MAX) {
            return -2;
        }

        packet->length = bytesRead;
        packet->contentsLength = packet->length - 2;
        packet->content = buffer + 2;

        if (packet->contentsLength >= 2 &&
            (*packet->opcode == TFTP_OPCODE_DATA || *packet->opcode == TFTP_OPCODE_ACKNOWLEDGEMENT)) {
            uint8_t blkLs = packet->content[0];
            packet->content[0] = packet->content[1];
            packet->content[1] = blkLs;
        }
        packet->endianness = HOST;
        return 0;
    }
}

int tftp_parse_req(TftpPacket *packet, TftpPacketRequest *request) {
    if (packet->contentsLength < 2) {
        return -1;
    } else if (packet->endianness == NETWORK) {
        return -2;
    } else {
        request->packet = packet;
        request->filename = (char *) packet->content;
        int filenameEndIndex = tftp_test_string(request->filename, packet->contentsLength);
        if (filenameEndIndex == -1) {
            return -3;
        }
        request->mode = request->filename + filenameEndIndex + 1;
        int modeEndIndex = tftp_test_string(request->filename, packet->contentsLength);
        if (modeEndIndex == -1) {
            return -4;
        }
        return 0;
    }
}

int tftp_parse_rrq(TftpPacket *packet, TftpPacketReadRequest *readRequest) {
    return tftp_parse_req(packet, readRequest);
}

int tftp_parse_wrq(TftpPacket *packet, TftpPacketWriteRequest *writeRequest) {
    return tftp_parse_req(packet, writeRequest);
}

int tftp_parse_data(TftpPacket *packet, TftpPacketData *data) {
    if (packet->contentsLength < 2) {
        return -1;
    } else if (packet->endianness == NETWORK) {
        return -2;
    } else {
        data->packet = packet;
        data->blockNum = (uint16_t *) packet->content;
        if (packet->contentsLength > 4) {
            data->dataLength = packet->contentsLength - 2;
            data->data = packet->content + 2;
        } else {
            data->dataLength = 0;
            data->data = NULL;
        }
        return 0;
    }
}

int tftp_parse_ack(TftpPacket *packet, TftpPacketAck *ack) {
    if (packet->contentsLength != 2) {
        return -1;
    } else if (packet->endianness == NETWORK) {
        return -2;
    } else {
        ack->packet = packet;
        ack->blockNum = (uint16_t *) packet->content;
        return 0;
    }
}

int tftp_parse_error(TftpPacket *packet, TftpPacketError *error) {
    if (packet->contentsLength <= 0) {
        return -1;
    } else if (packet->endianness == NETWORK) {
        return -2;
    } else {
        error->errorCode = (uint16_t *) packet->content;
        error->message = (char *) packet->content + 2;
        int errMsgEndIndex = tftp_test_string(error->message, packet->contentsLength - 2);
        if (errMsgEndIndex == -1) {
            return -3;
        }
        return 0;
    }
}

/**
 * Re-align the integers in {@param packet} to be network-order
 * @param packet the packet to re-align
 * @return 0 on success
 */
int tftp_serialize(TftpPacket *packet, int dataLength) {
    if (!(packet->endianness == HOST)) {
        return -1;
    } else {
        uint16_t opcode = *packet->opcode;
        uint8_t opLs = packet->buffer[1];
        packet->buffer[1] = packet->buffer[0];
        packet->buffer[0] = opLs;

        packet->contentsLength = 0;

        if (opcode == TFTP_OPCODE_ACKNOWLEDGEMENT || opcode == TFTP_OPCODE_DATA || opcode == TFTP_OPCODE_ERROR) {
            uint8_t numLs = packet->content[1];
            packet->content[1] = packet->content[0];
            packet->content[0] = numLs;
            packet->contentsLength += 2;
            if (opcode == TFTP_OPCODE_DATA) {
                packet->contentsLength += dataLength;
            }
        }

        packet->length = 2 + packet->contentsLength;
        packet->endianness = NETWORK;
        return 0;
    }
}

