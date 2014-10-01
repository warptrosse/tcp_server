# Copyright 2014 Federico Casares <warptrosse@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# @file Makefile


#### CONFIG ####################################################################

# COMPILER/LINKER
CC=gcc
CFLAGS+=-g -Werror -pedantic -Wall -Wextra
LDFLAGS+=-pthread -lrt

# OUTPUT
EXEC=tcp_server


#### PATHS #####################################################################

# TCP_SERVER
SRC_PATH=../src
INCL_PATH=../include


#### SOURCES ###################################################################

# TCP_SERVER
TCP_SERVER_SRC=$(SRC_PATH)/tcp_server_pool.c $(SRC_PATH)/tcp_server_cli.c \
        $(SRC_PATH)/tcp_server.c $(SRC_PATH)/main.c

# SOURCES
SOURCES=$(TCP_SERVER_SRC)


#### HEADERS ###################################################################

# TCP_SERVER
TCP_SERVER_H=$(INCL_PATH)

# HEADERS
HEADERS=-I$(TCP_SERVER_H)


################################################################################


OBJECTS=$(notdir $(SOURCES:.c=.o))

all:
	$(CC) $(CFLAGS) $(HEADERS) -c $(SOURCES)
	$(CC) $(CFLAGS) $(HEADERS) $(OBJECTS) -o $(EXEC) $(LDFLAGS)

clean:
	rm -f *.o $(EXEC)
