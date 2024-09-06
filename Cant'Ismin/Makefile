# Variables
CC = gcc #On utilise le compilateur gcc
CFLAGS = -Wall -Wextra -I./Librairie
LDFLAGS = -L./Librairie
LIBS = -lm

# Dossiers
LIB_DIR = Librairie
MOD_DIR = Modules
SRC_DIR = Projet
BIN_DIR = .

# Fichiers objets des modules
MOD_OBJS = $(patsubst %.c, %.o, $(wildcard $(MOD_DIR)/*.c))

# Cibles
TARGETS = $(BIN_DIR)/serveur $(BIN_DIR)/client

# Règles
.PHONY: all clean

all: $(TARGETS)

$(BIN_DIR)/serveur: $(SRC_DIR)/serveur.o $(MOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ $(LDFLAGS) $(LIBS)

$(BIN_DIR)/client: $(SRC_DIR)/client.o $(MOD_OBJS)
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/$@ $(LDFLAGS) $(LIBS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(MOD_DIR)/%.o: $(MOD_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o $(MOD_DIR)/*.o $(BIN_DIR)/serveur $(BIN_DIR)/client
