HDRS = \

SRCS = \
	mini_serv.c

OBJS = ${SRCS:.c=.o}

NAME = miniserv

CC = clang
CFLAGS = -g -Wall -Wextra #-Werror


all: client example ${NAME} 

${NAME}: ${OBJS}
	${CC} ${OBJS} -o ${NAME} ${CFGLAGS}

${OBJS}: ${HDRS}

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
