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
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include "../common/tftp.h"

#define INITIAL_BUFSIZE 516

#define LOG_NONE 0
#define LOG_INFO 1
#define LOG_VERBOSE 2
#define LOG_DEBUG 3
#define LOG_TRACE 50

int LOG_LEVEL = LOG_DEBUG;
int TRACE = 0;

void sighandler(int);

void handle_read_request(tftp_transmission);

void log_message(int level, const char *format, ...);

uint8_t recv_buffer[INITIAL_BUFSIZE];

int running = 1;

char path[512];

int main(int argc, char **argv) {
    int port = 5555;
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    strcpy(path, argv[1]);
    int sock_fd;
    socklen_t sock_addr_size = sizeof(struct sockaddr_in);
    struct sockaddr_in server, client;

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;

    // Converting arguments from host byte order to network byte order
    // Then binding address to socket using arguments
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    int could_bind = bind(sock_fd, (struct sockaddr *) &server, sock_addr_size);

    if (could_bind != 0) {
        log_message(LOG_NONE, "Could not bind to port %d. Terminating\n", port);
        return 1;
    }

    log_message(LOG_INFO, "Started server on %s:%d.\n", inet_ntoa(server.sin_addr),
                ntohs(server.sin_port));

    tftp_transmission host_transmission = tftp_create_transmission(0);
    host_transmission.original_socket = sock_fd;
    while (running) {
        int rec = recvfrom(sock_fd, recv_buffer, 514, 0, (struct sockaddr *) &client, &sock_addr_size);
        tftp_packet_request request_packet = {};
        int result = tftp_parse_packet_request(&request_packet, recv_buffer, rec);
        if (result) {
            log_message(LOG_INFO, "Received request from %s:%d, opcode: %d, filename: %s, mode: %s\n",
                        inet_ntoa(client.sin_addr),
                        ntohs(client.sin_port), request_packet.opcode, request_packet.filename, request_packet.mode);
            if (tftp_request_has_options(request_packet)) {
                log_message(LOG_DEBUG, "Options:\n");
                if (request_packet.has_block_size) {
                    log_message(LOG_DEBUG, "Block size: %d\n", request_packet.block_size);
                }
                if (request_packet.has_window_size) {
                    log_message(LOG_DEBUG, "Window size: %d\n", request_packet.window_size);
                }
                if (request_packet.has_timeout) {
                    log_message(LOG_DEBUG, "Timeout: %d\n", request_packet.timeout);
                }
            }
            tftp_transmission transmission = tftp_create_transmission(request_packet.block_size);

            transmission.request = request_packet;
            transmission.client_addr_size = sizeof(client);
            transmission.client_addr = malloc(transmission.client_addr_size);
            transmission.original_socket = sock_fd;
            memcpy(transmission.client_addr, &client, transmission.client_addr_size);
            if (request_packet.opcode == TFTP_OPCODE_READ_REQUEST) {
                handle_read_request(transmission);
            } else if (request_packet.opcode == TFTP_OPCODE_WRITE_REQUEST) {
                // handle_write_request(transmission);
            } else {
                tftp_packet_error error = tftp_create_packet_error();
                tftp_set_error(&error, TFTP_ERROR_ILLEGAL_OP);
                tftp_send_error(&host_transmission, &error, 1);
                log_message(LOG_TRACE, "Sent error code %d, \"%.*s\"\n", error.error_code, error.error_message_length,
                            error.message);
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
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(INADDR_ANY);

    int bound = bind(sockfd, (struct sockaddr *) &server, sizeof(struct sockaddr_in));
    if (bound != 0) {
        log_message(LOG_VERBOSE, "Could not create return socket. Terminating transmission.\n");
        tftp_packet_error error = tftp_create_packet_error();
        tftp_set_error_message(&error, "Could not create new socket.");
        tftp_send_error(&transmission, &error, 1);
        log_message(LOG_TRACE, "Sent error code %d, \"%.*s\"\n", error.error_code, error.error_message_length,
                    error.message);
        tftp_stop_transmission(&transmission);
        return;
    }
    log_message(LOG_DEBUG, "Created new socket for transmission.\n");
    transmission.socket = sockfd;

    char actualPath[512];
    strcpy(actualPath, path);
    if (actualPath[strlen(actualPath) - 1] != '/') {
        strcat(actualPath, "/");
    }

    strcat(actualPath, transmission.request.filename);
    log_message(LOG_DEBUG, "Actual path of requested file: %s\n", actualPath);

    int file_descriptor = open(actualPath, O_RDONLY);
    if (file_descriptor < 0) {
        tftp_packet_error error = tftp_create_packet_error();
        if (errno == ENOENT) {
            log_message(LOG_VERBOSE, "Could not find file %s\n", actualPath);
            error.error_code = TFTP_ERROR_ENOENT;
            tftp_set_error_message(&error, TFTP_ERROR_ENOENT_STRING);
        } else if (errno == EACCES) {
            log_message(LOG_VERBOSE, "Permission denied for file %s\n", actualPath);
            error.error_code = TFTP_ERROR_ACCESS_VIOLATION;
            tftp_set_error_message(&error, TFTP_ERROR_ACCESS_VIOLATION_STRING);
        }
        tftp_send_error(&transmission, &error, 0);
        log_message(LOG_TRACE, "Sent error code %d, \"%.*s\"\n", error.error_code, error.error_message_length,
                    error.message);
        return;
    }

    tftp_packet_ack ack = tftp_create_packet_ack();
    tftp_packet_error recv_error = tftp_create_packet_error();

    if (tftp_request_has_options(transmission.request)) {
        tftp_packet_optionack optionack = tftp_create_packet_oack();
        optionack.has_block_size = transmission.request.has_block_size;
        optionack.block_size = transmission.request.block_size;
        optionack.has_timeout = transmission.request.has_timeout;
        optionack.timeout = transmission.request.timeout;
        optionack.has_window_size = transmission.request.has_window_size;
        optionack.window_size = transmission.request.window_size;
        tftp_send_oack(&transmission, optionack);
        log_message(LOG_TRACE, "Sent oack:\n");
        if (optionack.has_block_size) {
            log_message(LOG_TRACE, "Block size: %d\n", optionack.block_size);
        }
        if (optionack.has_window_size) {
            log_message(LOG_TRACE, "Window size: %d\n", optionack.window_size);
        }
        if (optionack.has_timeout) {
            log_message(LOG_TRACE, "Timeout: %d\n", optionack.timeout);
        }
        int receive = tftp_receive_ack(&transmission, &ack, &recv_error);
        if (receive == TFTP_OP_ERROR) {
            log_message(LOG_VERBOSE, "Received TFTP error. Error code %d, message \"%.*s\".\n", recv_error.error_code,
                        recv_error.error_message_length, recv_error.message);
            tftp_stop_transmission(&transmission);
            return;
        } else if (receive != TFTP_SUCCESS) {
            tftp_packet_error error = tftp_create_packet_error();
            tftp_set_error(&error, TFTP_ERROR_ILLEGAL_OP);
            tftp_stop_transmission(&transmission);
            return;
        }
    }

    tftp_packet_data data = tftp_create_packet_data();

    data.buffer = transmission.tx_buffer + 4;
    data.buffer_length = transmission.request.block_size;

    int retransmissions = 0;
    int is_retransmission = 0;
    int read_bytes = -1;
    uint16_t block_num = 1;
    long block_counter = 0;
    do {
        if (!is_retransmission) {
            read_bytes = read(file_descriptor, data.buffer, data.buffer_length);
        }
        data.block_num = block_num;
        data.data_size = read_bytes;
        tftp_send_data(&transmission, &data, 0);
        log_message(LOG_TRACE, "Send data block %d, size %d\n", block_num, read_bytes);

        int receive = tftp_receive_ack(&transmission, &ack, &recv_error);

        if (receive == TFTP_OP_ERROR) {
            log_message(LOG_VERBOSE, "Received TFTP error. Error code %d, message \"%.*s\".\n", recv_error.error_code,
                        recv_error.error_message_length, recv_error.message);
            break;
        }

        if (receive == TFTP_INVALID_OPCODE) {
            log_message(LOG_VERBOSE, "Received invalid opcode.\n");
            tftp_packet_error error = tftp_create_packet_error();
            tftp_set_error(&error, TFTP_ERROR_ILLEGAL_OP);
            tftp_send_error(&transmission, &error, 0);
            log_message(LOG_TRACE, "Sent error code %d, \"%.*s\"\n", error.error_code, error.error_message_length,
                        error.message);
            break;
        }

        if (receive == TFTP_RECV_FAILED) {
            if (retransmissions == 5) {
                log_message(LOG_VERBOSE, "Transmission timed out.\n");
                tftp_packet_error error = tftp_create_packet_error();
                tftp_set_error_message(&error, "Receive timed out.");
                tftp_send_error(&transmission, &error, 0);
                log_message(LOG_TRACE, "Sent error code %d, \"%.*s\"\n", error.error_code, error.error_message_length,
                            error.message);
                break;
            }
            retransmissions++;
            is_retransmission = 1;
            log_message(LOG_VERBOSE, "Transmission timed out %d out of 5 times.\n", retransmissions);
            continue;
        }
        if (receive == TFTP_SUCCESS) {
            if (ack.block_num == block_num) {
                log_message(LOG_TRACE, "Received ack %d.\n", block_num);
                block_num = ack.block_num + 1;
                block_counter += 1;
                is_retransmission = 0;
                retransmissions = 0;
            } else {
                retransmissions++;
                is_retransmission = 1;
                log_message(LOG_TRACE, "Received incorrect ACK.\n", retransmissions);
            }
        }
    } while (read_bytes == transmission.request.block_size);
    log_message(LOG_VERBOSE, "Successfully transferred file %s in %li blocks.\n", transmission.request.filename,
                block_counter);
    tftp_stop_transmission(&transmission);
}

void log_message(int level, const char *format, ...) {
    va_list argp;
    va_start(argp, format);
    int is_trace = (level == LOG_TRACE && TRACE);
    if (is_trace || LOG_LEVEL >= level) {
        vprintf(format, argp);
    }
    va_end(argp);
}

