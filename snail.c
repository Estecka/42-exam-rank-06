/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   snail.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abaur <abaur@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2022/08/10 12:45:45 by abaur             #+#    #+#             */
/*   Updated: 2022/08/10 13:11:26 by abaur            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

static noreturn void	throw(int errnum){
	dprintf(STDERR_FILENO, "%i %s\n", errnum, strerror(errnum));
	exit(errnum ?: -1);
}

extern int  main(int argc, char**  argv){
    int pace        = (argc > 1) ? atoi(argv[1]) : 1;
	size_t buffsize = (argc > 2) ? atoi(argv[2]) : 1024;

    while (1)
	{
		fd_set fd_read;
		FD_ZERO(&fd_read);
		FD_SET(STDIN_FILENO, &fd_read);

		sleep(pace);
		int r = select(STDIN_FILENO+1, &fd_read, NULL, NULL, NULL);
		if (r < 0)
			throw(errno);

		char buffer[buffsize];
		ssize_t rcount = read(STDIN_FILENO, buffer, sizeof(buffer));
		if (rcount < 0)
			throw(errno);
		else if (rcount == 0)
			exit (0);
		else
			write(STDOUT_FILENO, buffer, rcount);
    }
}
