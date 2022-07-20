# File              : Makefile
# Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
# Date              : 06.12.2021
# Last Modified Date: 03.05.2022
# Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>

TARGET=sqlite2yandexdisk


all:
	mkdir -p build && cd build && cmake -DSQLITE2YANDEXDISK_BUILD_TEST="1" .. && make && open ${TARGET}_test


clean:
	rm -fr build
