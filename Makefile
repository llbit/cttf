CC=gcc
LD=gcc
CFLAGS=-Wall -pedantic -std=c99 -g
RM=rm -f

ifdef __MINGW32__
	LDFLAGS=-lmingw32 -lSDLmain -lSDL -mwindows -lGLU -lGL -g
else
	LDFLAGS=-lSDL -lGLU -lGL -g
endif

all:   ftest 3dtest vex libcttf.a otfdbg

libcttf.a: ttf.o triangulate.o shape.o list.o bstree.o qsortv.o stack.o \
	cttftext.o typeset.o treeset.o
	ar rcs $@ $^

ftest:	ftest.o shape.o ttf.o triangulate.o list.o bstree.o qsortv.o stack.o \
	treeset.o
	${LD} -o $@ $^ ${LDFLAGS}

3dtest:	3dtest.o shape.o ttf.o triangulate.o list.o bstree.o qsortv.o stack.o \
	cttftext.o treeset.o
	${LD} -o $@ $^ ${LDFLAGS}

vex:	vex.o shape.o list.o
	${LD} -o $@ $^ ${LDFLAGS}

otfdbg:	otfdbg.o ttf.o list.o shape.o
	${LD} -o $@ $^ ${LDFLAGS}

clean:
	${RM} *.o
	${RM} ftest
	${RM} 3dtest
	${RM} vex
	${RM} libcttf.a
	${RM} otfdbg

otfdbg.o: otfdbg.c ttf.h
	${CC} ${CFLAGS} -c $< -o $@

3dtest.o: 3dtest.c triangulate.h ttf.h text.h
	${CC} ${CFLAGS} -c $< -o $@

ftest.o: ftest.c triangulate.h ttf.h
	${CC} ${CFLAGS} -c $< -o $@

vex.o: vex.c shape.h list.h vector.h
	${CC} ${CFLAGS} -c $< -o $@

cttftext.o: cttftext.c text.h ttf.h triangulate.h typeset.h
	${CC} ${CFLAGS} -c $< -o $@

stack.o: stack.c stack.h
	${CC} ${CFLAGS} -c $< -o $@

bstree.o: bstree.c bstree.h
	${CC} ${CFLAGS} -c $< -o $@

qsortv.o: qsortv.c qsortv.h triangulate.h
	${CC} ${CFLAGS} -c $< -o $@

list.o:	list.c list.h
	${CC} ${CFLAGS} -c $< -o $@

treeset.o:	treeset.c treeset.h
	${CC} ${CFLAGS} -c $< -o $@

triangulate.o: triangulate.c triangulate.h
	${CC} ${CFLAGS} -c $< -o $@

ttf.o: ttf.c ttf.h
	${CC} ${CFLAGS} -c $< -o $@

shape.o: shape.c shape.h
	${CC} ${CFLAGS} -c $< -o $@

typeset.o: typeset.c ttf.h
	${CC} ${CFLAGS} -c $< -o $@

