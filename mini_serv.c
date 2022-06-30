/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   mini_serv.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abaur <abaur@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/06/28 18:17:53 by abaur             #+#    #+#             */
/*   Updated: 2022/06/30 11:56:51 by abaur            ###   ########.fr       */
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


int g_sockfd = -1;

static noreturn void	clean_exit(int status){
	if (g_sockfd != -1)
		close(g_sockfd);
	exit(status);
}
static noreturn void	throw(int errnum, char* message){
	write(STDERR_FILENO, "Fatal error\n", 12);
	if (message) dprintf(STDERR_FILENO, "%s\n", message);
	if (errnum)  dprintf(STDERR_FILENO, "%d %s\n", errnum, strerror(errnum));
	clean_exit(1);
}


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

extern int	main(int argc, char** argv) {
	if (argc != 2) {
		write(STDERR_FILENO, "Wrong number of arguments !\n", 28);
		return 1;
	}

	if (!SockInit(atoi(argv[1])))
		throw(errno, "Unable to bind create or bind socket.");

	//...

	clean_exit(0);
}
