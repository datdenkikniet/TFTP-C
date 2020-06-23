/*

    <one line to give the program's name and a brief idea of what it does.>
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

#include "../common/tftp.h"
#include <stdio.h>

void run_test();

void test_request(const char *test_name, const uint8_t *data, int data_length);

int main(){
    run_test();
}


void run_test(){
    uint8_t weird_packet[] = {
            0x00, 0x01, 0x73, 0x79, 0x73, 0x6c, 0x69, 0x6e, 0x75, 0x78, 0x2e, 0x65,
            0x66, 0x69, 0x36, 0x34, 0x00, 0x6f, 0x63, 0x74, 0x65, 0x74, 0x00, 0x74,
            0x73, 0x69, 0x7a, 0x65, 0x00, 0x30, 0x00, 0x62, 0x6c, 0x6b, 0x73, 0x69,
            0x7a, 0x65, 0x00, 0x31, 0x34, 0x36, 0x38, 0x00, 0x77, 0x69, 0x6e, 0x64,
            0x6f, 0x77, 0x73, 0x69, 0x7a, 0x65, 0x00, 0x34, 0x00, 0x00, 0x31, 0x34,
            0x30, 0x38, 0x00, 0x00, 0x30, 0x00, 0x62, 0x6c, 0x6b, 0x73, 0x69, 0x7a,
            0x65, 0x00, 0x31, 0x34, 0x30, 0x38, 0x00
    };
    test_request("Weird packet", weird_packet, sizeof(weird_packet));

    uint8_t illegal_opcode[] = {0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00};
    test_request("Illegal request", illegal_opcode, sizeof(illegal_opcode));

    uint8_t invalid_data[] = {0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};
    test_request("Invalid data", invalid_data, sizeof(invalid_data));
}

void test_request(const char *test_name, const uint8_t *data, const int data_length){
    tftp_packet_request request = {};
    int result = tftp_parse_packet_request(&request, data, data_length);
    printf("Test \"%s\" result: %d\n", test_name, result);
}

