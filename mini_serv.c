/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abaur <abaur@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/06/28 18:17:53 by abaur             #+#    #+#             */
/*   Updated: 2022/07/14 16:52:58 by abaur            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>

#define BUFFLEN	8

struct s_client {
	int	fd;
	int	uid;
	char*	inqueue;
	char*	outqueue;
};
typedef struct s_client	t_client;

int	g_sockfd = -1;
/**
 * The total amount of client that ever connected. NOT the amount of currently 
 * active clients.
 */
int	g_clientcount = 0;
/**
 * A map that takes a client's fd as key.
 */
t_client*	g_clients[FD_SETSIZE] = { NULL };



/******************************************************************************/
/* # Utility                                                                  */
/******************************************************************************/

static void	DeleteClient(t_client*);

static noreturn void	clean_exit(int status){
	if (g_sockfd != -1)
		close(g_sockfd);
	for (int fd=0; fd<FD_SETSIZE; fd++)
	if (g_clients[fd]) 
		DeleteClient(g_clients[fd]);
	exit(status);
}
static noreturn void*	throw(int errnum, const char* message){
	write(STDERR_FILENO, "Fatal error\n", 12);
	if (message) write(STDERR_FILENO, message, strlen(message));
	if (errnum)  dprintf(STDERR_FILENO, "%d %s\n", errnum, strerror(errnum));
	clean_exit(1);
}

/**
 * Concatenates two strings. The destination string is reallocated as needed.
 */
static void	strpush(char** str, const char* push){
	size_t	slen = strlen(*str);
	size_t	plen = strlen(push);
	*str = realloc(*str, slen+plen+1) ?: throw(errno, "Realloc error");
	strcat(*str, push);
}
static void	strshift(char* str, size_t shift){
	size_t	slen = strlen(str);
	for (size_t i=shift; i<=slen; i++)
		str[i-shift] = str[i];
}

static ssize_t	strichr(const char* str, char c){
	for (size_t i=0; str[i]; i++)
		if (str[i] == c)
			return i;
	return -1;
}

/******************************************************************************/
/* # Clients Methods                                                          */
/******************************************************************************/

static t_client*	NewClient(){
	int fd = accept(g_sockfd, NULL, NULL);
	if (fd < 0)
		throw(errno, "Accept error");

	t_client* cl = malloc(sizeof(t_client)) ?: (close(fd), throw(errno, "Client Malloc error"));
	g_clients[fd] = cl;
	cl->uid = g_clientcount++;
	cl->fd  = fd;
	cl->inqueue  = malloc(128) ?: throw(errno, "Inqueue malloc error");
	cl->outqueue = malloc(128) ?: throw(errno, "Outqueue malloc error");
	cl->inqueue[0]  = '\0';
	cl->outqueue[0] = '\0';
	return cl;
}

static void DeleteClient(t_client* cl){
	close(cl->fd);
	g_clients[cl->fd] = NULL;
	if (cl->inqueue)
		free(cl->inqueue);
	if (cl->outqueue)
		free(cl->outqueue);
	free(cl);
}

/**
 * @param format	Must contain a %i flag, and an optional %.*s flag.
 * @param msg	Should not include the terminating \\n.
 */
static void	Broadcast(const char* format, int senderuid, int msglen, const char* msg){
	char	sbuff[strlen(format) + msglen + 16];

	int	bufflen = sprintf(sbuff, format, senderuid, msglen, msg);
	write(STDOUT_FILENO, sbuff, bufflen);
	if (bufflen < 0)
		throw(errno, "Sprintf error");
	if (sizeof(sbuff) <= (size_t)bufflen)
		throw(1, "sprintf output somehow exceeded the buffer.");
	for (int fd=0; fd<FD_SETSIZE; fd++)
	if (g_clients[fd] && senderuid != g_clients[fd]->uid)
		strpush(&g_clients[fd]->outqueue, sbuff);
}

static void	ReadClient(t_client* cl){
	char buff[BUFFLEN+1] = { '\0' };

	ssize_t rcount = recv(cl->fd, buff, BUFFLEN, MSG_DONTWAIT);
	if (rcount < 0)
		throw(errno, "Recv error");
	else if (rcount == 0){
		if (cl->inqueue[0])
			Broadcast("client %i: %.*s", cl->uid, strlen(cl->inqueue), cl->inqueue);
		Broadcast("server: client %i just left\n", cl->uid, 0, NULL);
		DeleteClient(cl);
	}
	else {
		strpush(&cl->inqueue, buff);
		size_t	msglen;
		while ((msglen = 1+strichr(cl->inqueue, '\n'))){
			Broadcast("client %i: %.*s\n", cl->uid, msglen-1, cl->inqueue);
			strshift(cl->inqueue, msglen);
		}
	}
}

static void	WriteClient(t_client* cl){
	size_t wcount = send(cl->fd, cl->outqueue, strlen(cl->outqueue), MSG_DONTWAIT);
	if (wcount < 0 && (errno != EAGAIN))
		throw(errno, "Send error");
	else if (wcount != 0)
		strshift(cl->outqueue, wcount);
}


/******************************************************************************/
/* # Select Methods                                                           */
/******************************************************************************/

static bool	SockInit(int port){
	struct sockaddr_in addr;

	bzero(&addr, sizeof(addr));
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(port);

	return 0 <= (g_sockfd = socket(AF_INET, SOCK_STREAM, 0))
	    && 0 <= fcntl(g_sockfd, F_SETFL, O_NONBLOCK)
	    && 0 == bind(g_sockfd, (struct sockaddr*)&addr, sizeof(addr))
	    && 0 == listen(g_sockfd, 10)
	    ;

}

static void	Fd_Init(fd_set* fd_read, fd_set* fd_write){
	FD_ZERO(fd_read);
	FD_ZERO(fd_write);

	FD_SET(g_sockfd, fd_read);

	for (int fd=0; fd<FD_SETSIZE; fd++) 
	if (g_clients[fd]) {
		FD_SET(fd, fd_read);
		if (g_clients[fd]->outqueue[0])
			FD_SET(fd, fd_write);
	}
}

static noreturn void	SelectLoop() {
	fd_set	fd_read;
	fd_set	fd_write;

	while (true) 
	{
		Fd_Init(&fd_read, &fd_write);

		int r = select(FD_SETSIZE, &fd_read, &fd_write, NULL, NULL);
		if (r < 0)
			throw (errno, "Select error");
		else if (r == 0)
			continue;

		if (FD_ISSET(g_sockfd, &fd_read)){
			int uid = NewClient()->uid;
			Broadcast("server: client %i just arrived\n", uid, 0, NULL);
		}
		for (int fd=0; fd<FD_SETSIZE; fd++)
		if (g_clients[fd]) {
			if (FD_ISSET(fd, &fd_write))
				WriteClient(g_clients[fd]);
			if (FD_ISSET(fd, &fd_read))
				ReadClient(g_clients[fd]);
		}
	}
}

extern int	main(int argc, char** argv) {
	if (argc != 2) {
		write(STDERR_FILENO, "Wrong number of arguments !\n", 28);
		return 1;
	}

	if (!SockInit(atoi(argv[1])))
		throw(errno, "Unable to create or bind socket.");
	printf("Server open on port %s\n", argv[1]);

	SelectLoop();
}
