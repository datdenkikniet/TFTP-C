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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "../common/tftp.h"

#define INITIAL_BUFSIZE 516

void sighandler(int);

void handle_read_request(tftp_transmission);

void handle_write_request(tftp_transmission);

uint8_t recvBuffer[INITIAL_BUFSIZE];

int running = 1;

char *base_path = "/home/jona/CLionProjects/tftpserver/files";

int main(int argc, char **argv) {
    signal(SIGINT, sighandler);
    int sockfd;
    socklen_t sockAddrSize = sizeof(struct sockaddr_in);
    struct sockaddr_in server, client;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;

    // Converting arguments from host byte order to network byte order
    // Then binding address to socket using arguments
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(5555);
    bind(sockfd, (struct sockaddr *) &server, sockAddrSize);


    while (running){
        int rec = recvfrom(sockfd, recvBuffer, 514, 0, (struct sockaddr *) &client, &sockAddrSize);
        tftp_packet_request request_packet = {};
        int result = tftp_parse_packet_request(&request_packet, recvBuffer, rec);
        if (result){
            printf("Received request from %s:%d, opcode: %d, filename: %s, mode: %s\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port), request_packet.opcode, request_packet.filename, request_packet.mode);
            tftp_transmission transmission;
            transmission.request = request_packet;
            transmission.sockaddr_size = sizeof(client);
            transmission.addr = malloc(transmission.sockaddr_size);
            memcpy(transmission.addr, &client, transmission.sockaddr_size);
            if (request_packet.opcode == TFTP_OPCODE_READ_REQUEST){
                handle_read_request(transmission);
            } else if (request_packet.opcode == TFTP_OPCODE_WRITE_REQUEST){
                handle_write_request(transmission);
            }
        }
    }
    return 0;
}

void sighandler(int signum){
    printf("Stopping server...\n");
}

void handle_read_request(tftp_transmission transmission) {
    int fd = open(transmission.request.filename, O_RDONLY);
    if (fd < -1){
        if (errno == ENOENT){

        }
    }
}
