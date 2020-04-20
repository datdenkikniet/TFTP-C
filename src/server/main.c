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
#import <unistd.h>
#include <errno.h>
#include "../common/tftp.h"

#define INITIAL_BUFSIZE 516

void sighandler(int);

void handle_read_request(tftp_transmission);


uint8_t recvBuffer[INITIAL_BUFSIZE];

int running = 1;

char path[512];

int main(int argc, char **argv) {
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    strcpy(path, argv[1]);
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
    int could_bind = bind(sockfd, (struct sockaddr *) &server, sockAddrSize);

    if (could_bind != 0) {
        printf("Could not bind to port\n");
        return 1;
    }


    while (running) {
        int rec = recvfrom(sockfd, recvBuffer, 514, 0, (struct sockaddr *) &client, &sockAddrSize);
        tftp_packet_request request_packet = {};
        int result = tftp_parse_packet_request(&request_packet, recvBuffer, rec);
        if (result) {
            printf("Received request from %s:%d, opcode: %d, filename: %s, mode: %s\n", inet_ntoa(client.sin_addr),
                   ntohs(client.sin_port), request_packet.opcode, request_packet.filename, request_packet.mode);
            tftp_transmission transmission;
            tftp_init_transmission(&transmission, request_packet.block_size);

            transmission.request = request_packet;
            transmission.client_addr_size = sizeof(client);
            transmission.client_addr = malloc(transmission.client_addr_size);
            transmission.original_socket = sockfd;
            memcpy(transmission.client_addr, &client, transmission.client_addr_size);
            if (request_packet.opcode == TFTP_OPCODE_READ_REQUEST) {
                handle_read_request(transmission);
            } else if (request_packet.opcode == TFTP_OPCODE_WRITE_REQUEST) {
                // handle_write_request(transmission);
            }
        }
    }
    return 0;
}


void sighandler(int signum) {
    printf("Stopping server...\n");
    running = 0;
}

void handle_read_request(tftp_transmission transmission) {

    int sockfd;
    struct sockaddr_in server;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct timeval timeout;
    timeout.tv_usec = 0;
    if (transmission.request.has_timeout) {
        timeout.tv_sec = transmission.request.timeout;
    } else {
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
    }
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(struct timeval));

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;

    // Converting arguments from host byte order to network byte order
    // Then binding address to socket using arguments
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(INADDR_ANY);

    int bound = bind(sockfd, (struct sockaddr *) &server, sizeof(struct sockaddr_in));
    if (bound != 0) {
        tftp_packet_error error;
        tftp_init_error(&error);
        tftp_set_error_message(&error, "Could not create new socket.");
        tftp_send_error(&transmission, &error, 1);
    }

    transmission.socket = sockfd;

    char actualPath[512];
    strcpy(actualPath, path);
    if (actualPath[strlen(actualPath) - 1] != '/') {
        strcat(actualPath, "/");
    }

    strcat(actualPath, transmission.request.filename);

    int file_descriptor = open(actualPath, O_RDONLY);
    if (file_descriptor < 0) {
        tftp_packet_error error;
        tftp_init_error(&error);
        if (errno == ENOENT) {
            error.error_code = TFTP_ERROR_ENOENT;
            tftp_set_error_message(&error, TFTP_ERROR_ENOENT_STRING);
        } else if (errno == EACCES) {
            error.error_code = TFTP_ERROR_ACCESS_VIOLATION;
            tftp_set_error_message(&error, TFTP_ERROR_ACCESS_VIOLATION_STRING);
        }
        tftp_send_error(&transmission, &error, 0);
    }

    tftp_packet_ack ack;
    tftp_init_ack(&ack);
    uint16_t block_num = 0;
    int oack_error = 0;
    if (tftp_request_has_options(transmission.request)) {
        tftp_packet_optionack optionack;
        tftp_init_oack(&optionack);
        optionack.has_block_size = transmission.request.has_block_size;
        optionack.block_size = transmission.request.block_size;
        optionack.has_timeout = transmission.request.has_timeout;
        optionack.timeout = transmission.request.timeout;
        optionack.has_window_size = transmission.request.has_window_size;
        optionack.window_size = transmission.request.window_size;
        tftp_send_oack(&transmission, optionack);
        int receive = tftp_receive_ack(&transmission, &ack);
        if (receive != TFTP_SUCCESS) {
            tftp_packet_error error;
            tftp_init_error(&error);
            tftp_set_error(&error, TFTP_ERROR_ILLEGAL_OP);
            oack_error = 1;
        }
        block_num++;
    }
    if (!oack_error) {
        tftp_packet_data data;
        tftp_init_data(&data);

        data.buffer = transmission.tx_buffer + 4;
        data.buffer_length = transmission.request.block_size;

        int retransmissions = 0;
        int is_retransmission = 0;
        int read_bytes = -1;

        do {
            if (!is_retransmission) {
                read_bytes = read(file_descriptor, data.buffer, data.buffer_length);
            }
            data.block_num = block_num;
            data.data_size = read_bytes;
            tftp_send_data(&transmission, &data, 0);

            int receive = tftp_receive_ack(&transmission, &ack);

            if (receive == TFTP_INVALID_OPCODE) {
                tftp_packet_error error;
                tftp_init_error(&error);
                tftp_set_error(&error, TFTP_ERROR_ILLEGAL_OP);
                tftp_send_error(&transmission, &error, 0);
                break;
            }

            if (receive == TFTP_RECV_FAILED) {
                if (retransmissions == 5) {
                    tftp_packet_error error;
                    tftp_init_error(&error);
                    tftp_set_error_message(&error, "Receive timed out.");
                    tftp_send_error(&transmission, &error, 0);
                    break;
                }
                retransmissions++;
                is_retransmission = 1;
                continue;
            }
            if (receive == TFTP_SUCCESS) {
                block_num++;
                is_retransmission = 0;
                retransmissions = 0;
            }
        } while (read_bytes == transmission.request.block_size);
    }
    close(sockfd);
    tftp_free_transmission(&transmission);
}

