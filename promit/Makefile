# Default target will be executed if no argument is passed on 'make'.

default_target : build_debug_64bit

.PHONY : default_target

# Default goal.

.DEFAULT_GOAL = build_debug_64bit

# Global variable declarations.

CC             := gcc
SRC_DIR        := src/
INCLUDE        := include/
SRC_FILES       = $(wildcard $(SRC_DIR)*.c)
OBJ_FILES       = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.c.obj, $(SRC_FILES))

# Pre variable declaration

PRE_OUTPUT_DIR   := ../bin/
PRE_CFLAGS       := -std=c11 -MMD -MF
PRE_OBJ_DIR      := ../bin/intermediate/
DEPENDENCY_DIR   := $(PRE_OBJ_DIR)deps/

# Default variable delclaration for default target 'build_debug_64bit'.

CFLAGS       = -DDEBUG -fsanitize=address -g -Wall -Wextra $(PRE_CFLAGS)
LFLAGS       = -lasan
OUTPUT_DIR   = $(PRE_OUTPUT_DIR)Debug-64bit/
OBJ_DIR      = $(PRE_OBJ_DIR)Debug-64bit/
CONFIG      := Debug-64bit
ARCH        := -m64

# Variable declaration for different targets.

ifneq ($(findstring build_debug_32bit, $(MAKECMDGOALS)), ) 
	.DEFAULT_GOAL = build_debug_32bit
	ARCH         := -m32

	CFLAGS        = -DDEBUG -DM32 -Wall -Wextra -g $(ARCH) $(PRE_CFLAGS)
	OUTPUT_DIR    = $(PRE_OUTPUT_DIR)Debug-32bit/
	OBJ_DIR       = $(PRE_OBJ_DIR)Debug-32bit/
	CONFIG        = Debug-32bit
else ifneq ($(findstring build_release_64bit, $(MAKECMDGOALS)), ) 
	.DEFAULT_GOAL = build_release_64bit

	CFLAGS        = -DRELEASE -s -O3 $(PRE_CFLAGS)
	LFLAGS        = 
	OUTPUT_DIR    = $(PRE_OUTPUT_DIR)Release-64bit/
	OBJ_DIR       = $(PRE_OBJ_DIR)Release-64bit/
	CONFIG        = Release-64bit
else ifneq ($(findstring build_release_32bit, $(MAKECMDGOALS)), ) 
	.DEFAULT_GOAL = build_release_32bit
	ARCH         := -m32

	CFLAGS        = -DRELEASE -DM32 -s -O3 $(ARCH) $(PRE_CFLAGS)
	LFLAGS        = 
	OUTPUT_DIR    = $(PRE_OUTPUT_DIR)Release-32bit/
	OBJ_DIR       = $(PRE_OBJ_DIR)Release-32bit/
	CONFIG        = Release-32bit
endif

# Color variable.

PURPLE_BOLD := \e[1;35m
BLUE_BOLD   := \e[1;34m
CYAN        := \e[0;36m
GREEN       := \e[0;32m
RESET       := \e[0m

# OS detection and variable declaration.

EXE_NAME    := promit.exe
OS_NAME     := Windows
DESCRIPTION := "Executable Binary"

ifneq ($(findstring Linux, $(shell uname -s)),) 
	EXE_NAME    := promit
	OS_NAME     := Linux
	DESCRIPTION := "ELF Binary"
else ifneq ($(findstring Darwin, $(shell uname -s)),) 
	EXE_NAME    := promit
	OS_NAME     := MacOS/Darwin
	DESCRIPTION := "Mach-O Binary"
endif

.PHONY : 
build_debug_64bit : project_promit all

all : $(OBJ_FILES)
	@echo -e "\n$(BLUE_BOLD)Linking C translations and building $(DESCRIPTION) '$(EXE_NAME)' ($(OS_NAME)).\n$(RESET)"
	$(CC) $^ $(ARCH) -o $(OUTPUT_DIR)$(EXE_NAME) -lm $(LFLAGS)

.PHONY : 
build_debug_32bit : build_debug_64bit

.PHONY :
build_release_64bit: build_debug_64bit

.PHONY :
build_release_32bit: build_debug_64bit

.SILENT : 
$(OBJ_DIR)%.c.obj : $(SRC_DIR)%.c
	@echo -e "$(GREEN)Building C translation '$(patsubst $(SRC_DIR)%.c,%.c, $<)', generating object '$@'$(RESET)"
	$(CC) -c $(CFLAGS) $(patsubst $(SRC_DIR)%.c,$(DEPENDENCY_DIR)%.d, $<) -I$(INCLUDE) $< -o $@

project_promit : 
	@echo -e "$(PURPLE_BOLD)<=======================================| Project Promit |=======================================>$(RESET)\n"
	@echo -e "$(CYAN)Building Project Promit 'promit' on $(shell date) with configuration $(CONFIG).$(RESET)\n"
	@mkdir -p $(OBJ_DIR) $(OUTPUT_DIR) $(DEPENDENCY_DIR)

clean : 
	rm -rf $(PRE_OUTPUT_DIR)

.PHONY : clean

# Including dependencies.

-include $(wildcard $(DEPENDENCY_DIR)*.d)
