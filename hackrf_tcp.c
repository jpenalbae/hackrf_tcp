/*
 * Copyright 2014 Jaime Peñalba <jpenalbae@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * hackrf_tcp uses a quick and dirty protocol.
 * Endianess is always network order (big endian).
 * 
 * 
 * All the packets have the following structure
 * ------------------------------------------
 * | 1 byte - TYPE | Variable length data   |
 * ------------------------------------------
 * 
 * 
 * Based on the type there are diferent packets:
 * 
 *   Hello packet (sent by the server once the client connects)
 *   ---------------------------------------------------------------
 *   | 0x00 | 1 byte - board id | variable length - version | 0x00 |
 *   ---------------------------------------------------------------
 * 
 *   Command request, basically calls to libkhackrf (sent by the client)
 *   ------------------------------------------------------------------
 *   | 0x01 | 1 byte - cmd type | 2 bytes - seq | 8 bytes - parameter |
 *   ------------------------------------------------------------------
 * 
 *   Command response (sent by the server)
 *   --------------------------------------------------------
 *   | 0x02 | 2 bytes - seq | 4 bytes (signed int) - result |
 *   --------------------------------------------------------
 * 
 *   Incomming I/Q data (sent by the server)
 *   -----------------------------------------
 *   | 0x03 | 4 byte - len |    I/Q data     |
 *   -----------------------------------------
 */


#include <libhackrf/hackrf.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <inttypes.h>
#include <endian.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <math.h>
#include <signal.h>


struct cmdreq {
    uint8_t type;
    uint8_t cmd;
    uint16_t seq;
    uint64_t param;
}__attribute__((packed));

struct cmdres {
    uint8_t type;
    uint16_t seq;
    int result;
}__attribute__((packed));

hackrf_device* device = NULL;
int client;
    

void usage(void)
{
	printf("hackrf_tcp, an I/Q spectrum server for HackRF\n\n"
		"Usage:\t[-a listen address]\n"
		"\t[-p listen port (default: 1234)]\n"
		"\t[-f frequency to tune to [Hz]]\n"
		"\t[-s samplerate in Hz (default: 8000000 Hz)]\n");
	exit(1);
}


void close_and_exit(int status)
{
    int result;
    
    result = hackrf_close(device);
	if (result != HACKRF_SUCCESS) {
		fprintf(stderr, "hackrf_close() failed: %s (%d)\n",
				hackrf_error_name(result), result);
		exit(EXIT_FAILURE);
	}

	hackrf_exit();
    
    exit(status);
}

int rx_callback(hackrf_transfer *transfer)
{
    //char buffer[302144];
    
    //printf("CALLBACK GOT %d bytes\n", transfer->valid_length);
    //buffer[0] = 0x03;
    //memcpy(&buffer[1], &(transfer->valid_length), 4);
    //memcpy(&buffer[5], transfer->buffer, transfer->valid_length);
    
   //printf("len %d\n", transfer->valid_length);
    write(client, transfer->buffer, transfer->valid_length);
    
    return 0;
}


void handle_cmds()
{
    int rbytes, wbytes, res;
    char buffer[255];
    struct cmdreq *cmd;
    struct cmdres response;
    
    response.type = 0x02;
    
    do {
        //rbytes = read(client, buffer, sizeof(buffer));
        rbytes = read(client, buffer, sizeof(struct cmdreq));
        cmd = (struct cmdreq *)buffer;
        
        printf("read %i bytes\n", rbytes);
        
        /* Init the response */
        response.seq = cmd->seq;
        response.result = 0;
        
        if (rbytes != 12)
            continue;
        
        /* Ignore packets wich dont match the cmd type */
        if (cmd->type != 0x01)
            continue;
        
        /* Convert to host endianess */
        cmd->param = be64toh(cmd->param);
        cmd->seq = be16toh(cmd->seq);
        
        switch (cmd->cmd) {
            case 0x01:
                printf("set freq %" PRIu64  "\n", cmd->param);
                res = hackrf_set_freq(device, cmd->param);
                break;
                
            case 0x02:
                printf("set sample rate %" PRIu64 "\n", cmd->param);
                hackrf_set_sample_rate(device, cmd->param);
                break;
                
            case 0x03:
                printf("enable amp %i\n", (uint8_t)cmd->param);
                res = hackrf_set_amp_enable(device, (uint8_t)cmd->param);
                break;
                
            case 0x04:
                printf("set lna gain %d db\n", (uint32_t)cmd->param);
                res = hackrf_set_lna_gain(device, (uint32_t)cmd->param);
                break;
                
            case 0x05:
                printf("set vga gain %d db\n", (uint32_t)cmd->param);
                res = hackrf_set_vga_gain(device, (uint32_t)cmd->param);
                break;
                
            case 0x06:
                printf("set baseband bandwidth %d\n", (uint32_t)cmd->param);
                res = hackrf_set_baseband_filter_bandwidth(
                        device, (uint32_t)cmd->param);
                break;
                    
            case 0x07:
                printf("Check if hackrf is streaming\n");
                res = hackrf_is_streaming(device);
                break;
                
            case 0x08:
                printf("RX Start\n");
                res = hackrf_start_rx(device, rx_callback, NULL);
                break;
            
            /* Unkown cmd or not implemented */
            default:
                res = 0xDEAD;
                break;
        }
        
        
        /* Send the response */
        response.result = htobe32(res);
        /* TODO: improve the ignore SIGPIPE method to avoid crashes */
        //signal(SIGPIPE, SIG_IGN);
        //wbytes = write(client, &response, sizeof(response));
        //signal(SIGPIPE, SIG_DFL);
        
        //if (wbytes != sizeof(response))
        //    break;
        
    } while (rbytes > 0);
    
    hackrf_stop_rx(device);
}


int main(int argc, char** argv)
{
    int opt, result, optval, server;
    struct sockaddr_in local, remote;
    socklen_t rlen;
    uint8_t board_id = BOARD_ID_INVALID;
    char buffer[255], version[255 + 1];
    
    char* addr = "127.0.0.1";
    int port = 1234;
    
    while ((opt = getopt(argc, argv, "a:p:h")) != -1) {
        switch (opt) {
            case 'a':
                addr = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                usage();
                break;
        }
    }
    
    if (argc < optind)
		usage();
    
    
    /* Init the device and print some info */
    result = hackrf_init();
    if (result != HACKRF_SUCCESS) {
        fprintf(stderr, "hackrf_init() failed: %s (%d)\n",
				hackrf_error_name(result), result);
		return EXIT_FAILURE;
    }
    
    result = hackrf_open(&device);
	if (result != HACKRF_SUCCESS) {
		fprintf(stderr, "hackrf_open() failed: %s (%d)\n",
				hackrf_error_name(result), result);
		return EXIT_FAILURE;
	}
    
    result = hackrf_board_id_read(device, &board_id);
	if (result != HACKRF_SUCCESS) {
		fprintf(stderr, "hackrf_board_id_read() failed: %s (%d)\n",
				hackrf_error_name(result), result);
		return EXIT_FAILURE;
	}
	printf("Board ID Number: %d (%s)\n", board_id,
			hackrf_board_id_name(board_id));

	result = hackrf_version_string_read(device, &version[0], 255);
	if (result != HACKRF_SUCCESS) {
		fprintf(stderr, "hackrf_version_string_read() failed: %s (%d)\n",
				hackrf_error_name(result), result);
		return EXIT_FAILURE;
	}
	printf("Firmware Version: %s\n", version);
    
    
    /* Setup server addr & port */
    memset(&local,0,sizeof(local));
	 local.sin_family = AF_INET;
	 local.sin_port = htons(port);
	 local.sin_addr.s_addr = inet_addr(addr);
    
    while (1) {
        /* Create the server socket */
        server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == -1) {
            perror("socket");
            close_and_exit(EXIT_FAILURE);
        }

        optval = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
        setsockopt(server, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
        
        /* Bind and listen */
        if (bind(server, (struct sockaddr *)&local, sizeof(local))) {
            perror("bind");
            close_and_exit(EXIT_FAILURE);
        }

        if (listen(server, 0)) {
                perror("listen");
                close_and_exit(EXIT_FAILURE);
        }
        
        printf("Waiting for a new client...\n");
        printf("Use the device argument 'hackrf_tcp=%s:%d' in OsmoSDR "
               "to connect\n", addr, port);
        
        client = accept(server, (struct sockaddr *)&remote, &rlen);
        
        /* 
         * Note to this non sense open and close of the server socket.
         * 
         * Always close the server socket after we get a client as
         * HackRF only supports one user/listener, so we wont be accepting
         * any more connections till the current client disconnects.
         */
        close(server);
        
        if (client == -1) {
            perror("accept");
            continue;
        }
        
        printf("New connection from: %s\n", inet_ntoa(remote.sin_addr));
        
        /* Send hello packet */
        memset(buffer, 0, sizeof(buffer));
        buffer[0] = 0x00;
        buffer[1] = board_id;
        strncpy(&buffer[2], version, sizeof(buffer)-3);
        write(client, buffer, strlen(&buffer[2])+3);
        
        /* Wait for client cmds */
        handle_cmds();
        
        printf("End of client connection.\n");
        close(client);
    }
    
    

	return EXIT_SUCCESS;
}

