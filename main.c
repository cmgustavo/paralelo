/***********************************************************************
 *            paralelo.c
 *
 *         Jun 20 2006
 *  Copyright  2006  Gustavo Cortez
 *  gustavo@lugtucuman.org.ar
 *
 *********************************************************************** 
 * Description:
 *
 * Send data to printer, to another PC and 
 * receive data from other PC using Parallel Port.
 *
 ************************************************************************
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <linux/ppdev.h>
#include <string.h>
#include <asm/io.h>

#define BASEPORT 0x378			/* Parallel Port */
#define DATA BASEPORT+0			/* Data Register */
#define STATUS BASEPORT+1		/* Status Register */
#define CONTROL BASEPORT+2		/* Control Register */

void catcher();
void print_to(char * charset);
void reset_printer();
char _status();
char _control();
void send_parallel(char character);
void _ack();
void _busy();
void waiting_connection();
void _server_();
void _client_();
void tx_byte(const char character);
char rx_nibble();
void die_on_error(void) __attribute__ ((noreturn));

int main(int argc, char **argv)
{
	/* Set perm to write parallel port */
	if (ioperm(BASEPORT, 3, 1)) 
		die_on_error();
	
	if (argc == 1 || argc > 3) {
		printf("Send Date To Printer v0.1\n");
		printf("Enter argument --help to help list\n");
	}
	else {
		if (!(strcmp(argv[1], "--help"))) {
			printf("Send Date To Printer v0.1\n");
			printf("\nOptions: \n");
			printf("--print\t\tPrint characters set in LPTx\n");
			printf("--server\tReceive data through LPTx from other PC\n");
			printf("--client\tSend data through LPTx to other PC\n");
			printf("--chat\t\tRun in chat mode\n");
			printf("--help\t\tPrint this help\n\n");
			printf("Example:\n");
			printf("\t<command> --print 'Hola mundo'\n\n");
		}
		else if (!(strcmp(argv[1], "--print"))) {
			if (argc < 3) {
				printf("Argument error\n");
				die_on_error();
			}
			else { 
				/* Send characters to printer */
				print_to(argv[2]);
			}
		}
		else if (!(strcmp(argv[1], "--server"))) {
			if (argc > 2) {
				printf("Argument error\n");
				die_on_error();
			}
			else {
				/* Call to _server_() procedure. 
				 * Server only receive data from parallel port */
				_server_();
			}
		}
		else if (!(strcmp(argv[1], "--client"))) {
			if (argc > 2) {
				printf("Argument error\n");
				die_on_error();
			}
			else {
				/* Call to _client_() procedure. 
				 * Client only send data to parallel port */
				_client_();
			}
		}
		else {
			printf("Argument error\n");
			die_on_error();
		}
	}
	
	if (ioperm(BASEPORT, 3, 0))
		die_on_error();
	
	return 0;
}

void die_on_error(void)
{
	perror("ioperm");
	exit(EXIT_FAILURE);
}

void catcher(){
	int fd;
	ioctl(fd, PPRELEASE);
	die_on_error();
}

void reset_printer ()
{
	char control;
	control = _control();
	/* Reset printer */
	outb(control & 0xfb, CONTROL);
	outb(control | 0x04, CONTROL);
	printf("Status Register: %x\n",_status());
}

char _status ()
{
	char status;
	status = inb(STATUS);
	return status;
}

char _control()
{
	char control;
	control = inb(CONTROL);
	return control;
}

void print_to (char * charset)
{
	int charset_long, i=0;
	char control;
	control = _control();
	/* Signal for end program with Ctrl+c */
	signal(SIGINT, catcher);
	/* Always reset printer */
	reset_printer();
	charset_long = strlen(charset);
	/* Waiting for connect the printer */
	waiting_connection();
	printf("\nPrinted characters: ");
	for (i; i<charset_long; i++) {
		/* Send character to printer */
		send_parallel(charset[i]);
		printf("%c",charset[i]);
	}
	printf("\nPrint finished.\n");
}

void send_parallel (char character)
{
	char control;
	control = _control();
	outb(control & 0xfe, CONTROL); // Strobe to low
	outb(character, DATA);
	outb(control | 0x01, CONTROL); // Strobe to high
}

void _busy()
{
	char status;
	printf("Busy ");
	do {
		status = _status();
		sleep(1);
		printf(".");
	}while (status & 0x80);
}

void _ack()
{
	char status;
	do {
		status = _status();
		sleep(1);
	}while (!(status & 0x40));
}

void waiting_connection()
{
	char status;
	printf("Waiting for connection ");
	while(1) {
		status = _status();
		if (status & 0x10) break;
		printf(".");
		sleep(5);
	}
	printf(" Connected.\n");
}

void _client_()
{
	char * word, control;
	int word_long, i=0;
	word = (char *)malloc(sizeof(char));
	control = _control();
	/* Signal for end program with Ctrl+c */
	signal(SIGINT, catcher);
	printf("Client mode:\n");
	printf("Type characters to send (or zero to close):\n");
	while(1) {
		printf("\r> ");
		scanf("%s", word);
		word_long = strlen(word);
		/* Close connection if character is zero '0' */
		if (word_long == 1) {
			if (word[0] == '0') {
				tx_byte(word[i]);
				break;
			}
		}
		for(i=0; i<word_long; i++) {
			tx_byte(word[i]);
		}
	}
	printf("\nConnection closed.\n");
}

void _server_()
{
	char control, status, nibble_high, nibble_low;
	char character;
	int flag;
	control = _control();
	/* Signal for end program with Ctrl+c */
	signal(SIGINT, catcher);
	printf("Server mode:\n");
	printf("Data received:\n");
	while (1) {
		if (!(((inb(STATUS) >> 3) & 0x1f) & 0x10)) {
			/* Receive 4 LSB */
			nibble_low = rx_nibble();
			nibble_low = nibble_low & 0x0f;
			/* Send confirmation to client */
			outb(0x10,DATA);
			/* Receive 4 MSB */
			nibble_high = rx_nibble();
			nibble_high = nibble_high << 4;
			nibble_high = nibble_high & 0x0f;
			/* Send confirmation to client */
			outb(0x0,DATA);
			/* Get MSB and LSB */
			character = nibble_high | nibble_low;
			/* Print LSB, MSB and character received */
			printf("\nNibble_low: %x\n", nibble_low);
			printf("Nibble_high: %x\n", nibble_high);
			printf("Character (hex): %x\n", character);
			printf("Character (chr): %c\n", character);
			/* Close connection if character is zero '0' */
			if (nibble_high == 0x0 && nibble_low == 0x0) break;
		}
	}
	printf("\nConnection closed.\n");
}

int received_ok()
{
	char status;
	status = _status();
	if(!(status & 0x80)) 
		return 1;
	else 
		return 0;		
}

void tx_byte(const char character)
{
	char nibble_high, nibble_low;
	outb(0x10,DATA);
	usleep(10);
	nibble_low = character & 0x0f;
	outb(nibble_low, DATA);
	while(((inb(STATUS) >> 3) & 0x1f) & 0x10) usleep(10000);
	nibble_high = character >> 4;
	nibble_high = nibble_high & 0x0f;
	outb(nibble_high, DATA);
	usleep(500000);
	if(((inb(STATUS) >> 3) & 0x1f) & 0x10) {
		usleep(100000);
		outb(0x0,DATA);
		printf("Character sent\n");
	}
	else {
		usleep(100000);
		outb(0x0,DATA);
		printf("Character fail to send\n");
	}
}

char rx_nibble()
{
	char status;
	/* Select 5 LSB */
	status = inb(STATUS) >> 3;
	status = status & 0x0f;
	return status;
}
