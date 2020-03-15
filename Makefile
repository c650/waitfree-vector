
CC=g++
CFLAGS= --std=c++17 -Wall -Werror -Ofast -lpthread

SEQ_SRC=src/sequential/*.cpp
CON_SRC=src/concurrent/*.cpp

SEQ_OUT=bin/sequential.out
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

sequential: ensure_dirs
	${CC} ${SEQ_SRC} ${CFLAGS} -o ${SEQ_OUT}

concurrent: ensure_dirs
	${CC} ${CON_SRC} ${CFLAGS} -o ${CON_OUT}
