HDRS = \

SRCS = \
	mini_serv.c

OBJS = ${SRCS:.c=.o}

NAME = miniserv

CFLAGS = -g -Wall -Wextra #-Werror


all: client example ${NAME} 

${NAME}: ${SRCS} ${HDRS}
	gcc   ${SRCS} -o ${NAME} ${CFGLAGS}
	clang ${SRCS} -o ${NAME} ${CFGLAGS}

client: client.c
	gcc ${CFLAGS} client.c -o client

example: mini_serv_example.c
	gcc ${CFLAGS} mini_serv_example.c -o example

clean:
	rm -f *.o

fclean: clean
	rm -f example client
	rm -f ${NAME}

re: fclean all

.PHONY: clean fclean all re
