C=shared.c
T=testmaster
X=$C $T.c
O=${C:.c=.o}
S=shared.so

U=/usr/local

STUB=$U/lib/libtclstub84g.a

SFLAGS=-shared -fPIC -I$U/include -I$U/include/tcl8.4 -fno-common -DUSE_TCL_STUBS=1 -DWITH_TCL -DSHARED_TCL_EXTENSION

CFLAGS=-g -Wall -Wno-implicit-int

$T: $T.o $O
	${CC} ${CFLAGS} -o $T $T.o $O

$S: $C
	${CC} ${CFLAGS} ${SFLAGS} -o $S $C ${STUB}

shared.c: shared.h

clean:
	rm -f $O $T.o $S test.map
