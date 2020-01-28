/*

    Main file that can be executed, and manages connections
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

#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <zconf.h>
#include "../common/tftp.h"

uint8_t recvBuffer[516];
uint8_t sendBuffer[516];

int main(int argc, char **argv) {
    int sockfd;
    socklen_t sockSize = sizeof(struct sockaddr_in);
    struct sockaddr_in server, client;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;

    // Converting arguments from host byte order to network byte order
    // Then binding address to socket using arguments
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(5555);
    bind(sockfd, (struct sockaddr *) &server, sockSize);

    int fd;

    int rec = recvfrom(sockfd, recvBuffer, 514, 0, (struct sockaddr *) &client, &sockSize);
    TftpPacket pack = {};
    tftp_parse_packet(recvBuffer, 514, rec, &pack);

    if (*pack.opcode == TFTP_OPCODE_READ_REQUEST) {
        TftpPacketReadRequest req = {};
        tftp_parse_rrq(&pack, &req);

        fd = open(req.filename, O_RDONLY);

        TftpPacket packet = {};
        tftp_init_packet(&packet, TFTP_OPCODE_DATA, sendBuffer, 516);

        TftpPacketData data = {};
        tftp_init_data(&packet, &data);

        int readBytes = read(fd, data.data, 512);
        data.dataLength = readBytes;
        *data.blockNum = 1;

        tftp_serialize(&packet, data.dataLength);

        printf("Read bytes: %d, Endianness: %d, Contents length: %d, Length: %d, Opcode: %04X\nContents: %.*s\n",
               readBytes, packet.endianness, packet.contentsLength, packet.length, *packet.opcode, data.dataLength,
               data.data);

        int sent = sendto(sockfd, packet.buffer, packet.length, 0, (const struct sockaddr *) &client, sockSize);

        printf("Sent: %d\n", sent);

    }

    return 0;
}