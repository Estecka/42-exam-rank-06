#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define BUFFER_SIZE 65535

char* messages[255] = {
	"Hello !\n",
	"world ?\n",
	"This is a message.\n",
	"There is two lines\nhere is the second one\n",
	"I'm not going to add a new line here",
	NULL
};
size_t messages_length = 5;

volatile sig_atomic_t running;

void stop() {
	running = 0;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		printf("Usage: client [port]");
		return 1;
	}

	// Connect to server

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket()");
		return 1;
	}

	int port = atoi(argv[1]);
	struct sockaddr_in server_addr;
	socklen_t addr_len = sizeof(struct sockaddr);
	// memset(&server_addr, 0, addr_len);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(port);

	if (connect(sockfd, (struct sockaddr*)&server_addr, addr_len) < 0) {
		perror("connect()");
		return -1;
	}
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
	printf("Connected to Server port %d\n", port);

	// Main Loop

	char buffer[BUFFER_SIZE];
	signal(SIGINT, stop);

	fd_set read_fds;
	fd_set write_fds;
	struct timeval timeout;

	size_t current_message = 0;
	running = 1;
	int do_send = 0;
	struct timespec last, start;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	while (running) {
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_SET(sockfd, &read_fds);
		if (do_send)
			FD_SET(sockfd, &write_fds);

		int activity = select(sockfd + 1, &read_fds, &write_fds, NULL, &timeout);
		if (activity > 0) {
			if (FD_ISSET(sockfd, &read_fds)) {
				int received = recv(sockfd, buffer, 65535, MSG_DONTWAIT);
				if (received < 0) {
					perror("recv()");
					close(sockfd);
					return 0;
				}
				else if (received == 0) {
					printf("Server disconnected\n");
					close(sockfd);
					return 0;
				}
				else {
					buffer[received] = 0;
					printf("%s", buffer);
				}
			}
			else if (FD_ISSET(sockfd, &write_fds) && do_send) {
				char* message = messages[current_message];
				size_t length = strlen(message);
				send(sockfd, message, length, MSG_DONTWAIT);
				do_send = 0;
				clock_gettime(CLOCK_MONOTONIC_RAW, &start);
				current_message = (current_message + 1) % messages_length;
			}
		}
		else if (activity == 0)
			printf("Timed out\n");
		clock_gettime(CLOCK_MONOTONIC_RAW, &last);
		if (last.tv_sec - start.tv_sec >= 1)
			do_send = 1;
	}

	close(sockfd);
	return 0;
}
