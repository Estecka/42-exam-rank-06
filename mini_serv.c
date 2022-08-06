/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abaur <abaur@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/06/28 18:17:53 by abaur             #+#    #+#             */
/*   Updated: 2022/08/06 14:42:20 by abaur            ###   ########.fr       */
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
	bool	newline;
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
	if (message) dprintf(STDERR_FILENO, "%s\n", message);
	if (errnum)  dprintf(STDERR_FILENO, "%d %s\n", errnum, strerror(errnum));
	clean_exit(1);
}

/**
 * Returns the index of either the first ocurrence of c, or the null terminator.
 */
static size_t	strichr(const char* str, char c){
	for (size_t i=0; true; i++)
		if (str[i] == c || !str[i])
			return i;
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
	cl->newline = true;
	return cl;
}

static void DeleteClient(t_client* cl){
	close(cl->fd);
	g_clients[cl->fd] = NULL;
	free(cl);
}

static void	BroadcastRaw(const char* str, int senderuid, size_t len){
	write(STDOUT_FILENO, str, len);
	for (int fd=0; fd<FD_SETSIZE; fd++)
	if (g_clients[fd] && senderuid != g_clients[fd]->uid)
		send(fd, str, len, MSG_DONTWAIT);
}
/**
 * @param format	May contain an optional %i flag.
 */
static void	BroadcastFormat(const char* format, int senderuid){
	char	sbuff[strlen(format) + 16];

	int	bufflen = sprintf(sbuff, format, senderuid);
	if (bufflen < 0)
		throw(errno, "Sprintf error");
	if (sizeof(sbuff) <= (size_t)bufflen)
		throw(1, "sprintf output somehow exceeded the buffer.");
	BroadcastRaw(sbuff, senderuid, bufflen);
}

static void	ReadClient(t_client* cl){
	char buff[BUFFLEN+2] = { '\0' };

	ssize_t rcount = recv(cl->fd, buff, BUFFLEN, MSG_DONTWAIT);
	if (rcount < 0)
		throw(errno, "Recv error");
	else if (rcount == 0){
		BroadcastFormat("server: client %i just left\n", cl->uid);
		DeleteClient(cl);
	}
	else for (char* msg=buff; msg[0]; ){
		if (cl->newline){
			cl->newline = false;
			BroadcastFormat("client %i : ", cl->uid);
		}
		ssize_t msglen = strichr(msg, '\n');
		if (msg[msglen] == '\n'){
			cl->newline = true;
			BroadcastRaw(msg, cl->uid, msglen+1);
		} else
			BroadcastRaw(msg, cl->uid, msglen);
		msg += msglen + 1;
	}
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
	    && 0 == listen(g_sockfd, 128)
	    ;
}

static void	Fd_Init(fd_set* fd_read){
	FD_ZERO(fd_read);

	FD_SET(g_sockfd, fd_read);

	for (int fd=0; fd<FD_SETSIZE; fd++) 
	if (g_clients[fd])
		FD_SET(fd, fd_read);
}

static noreturn void	SelectLoop() {
	fd_set	fd_read;

	while (true) 
	{
		Fd_Init(&fd_read);

		int r = select(FD_SETSIZE, &fd_read, NULL, NULL, NULL);
		if (r < 0)
			throw (errno, "Select error");
		else if (r == 0)
			continue;

		if (FD_ISSET(g_sockfd, &fd_read)){
			int uid = NewClient()->uid;
			BroadcastFormat("server: client %i just arrived\n", uid);
		}
		for (int fd=0; fd<FD_SETSIZE; fd++)
		if (g_clients[fd]) {
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
	write(STDOUT_FILENO, "Server open on port ", 20);
	write(STDOUT_FILENO, argv[1], strlen(argv[1]));
	write(STDOUT_FILENO, "\n", 1);

	SelectLoop();
}
