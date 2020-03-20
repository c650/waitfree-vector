
CC=g++
CFLAGS= --std=c++17 -Wall -Werror -Ofast -lpthread

SEQ_SRC=src/sequential/*.cpp
MRL_SRC=src/mrlock/*.cpp
CON_SRC=src/concurrent/*.cpp

SEQ_OUT=bin/sequential.out
MRL_OUT=bin/mrlock.out
CON_OUT=bin/concurrent.out

.PHONY: clean get_deps format

clean:
	rm -rf mrlock

# ubuntu only
get_deps:
	sudo apt install libboost-dev libboost-random-dev libtbb-dev

ensure_dirs:
	mkdir -p bin

format:
	find -type f -path "./src/*" -name "*.cpp" | xargs clang-format -i --style=file
	find -type f -path "./src/*" -name "*.hpp" | xargs clang-format -i --style=file

mrlock: ensure_dirs
	$(MAKE) -C mrlock/
	${CC} -I mrlock/src/ ${MRL_SRC} mrlock/src/strategy/*.o ${CFLAGS} -o ${MRL_OUT}

sequential: ensure_dirs
	${CC} ${SEQ_SRC} ${CFLAGS} -o ${SEQ_OUT}

concurrent: ensure_dirs
	${CC} ${CON_SRC} ${CFLAGS} -o ${CON_OUT}
