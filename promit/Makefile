# Default target will be executed if no argument is passed on 'make'.

default_target : build_debug_x64

.PHONY : default_target

# Default goal.

.DEFAULT_GOAL = build_debug_x64

# Global variable declarations.

CC             := gcc
SRC_DIR        := src/
INCLUDE        := include/
SRC_FILES       = $(wildcard $(SRC_DIR)*.c)
OBJ_FILES       = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.c.obj, $(SRC_FILES))

# Pre variable declaration

PRE_OUTPUT_DIR   := ../bin/
PRE_LFLAGS       := -std=c11 -MMD -MF
PRE_OBJ_DIR      := ../bin/intermediate/
DEPENDENCY_DIR   := $(PRE_OBJ_DIR)deps/

# Default variable delclaration for default target 'build_debug_x64'.

LFLAGS       = -DDEBUG -g $(PRE_LFLAGS)
OUTPUT_DIR   = $(PRE_OUTPUT_DIR)Debug-x64/
OBJ_DIR      = $(PRE_OBJ_DIR)Debug-x64/
CONFIG      := Debug-x64
ARCH        := 

# Variable declaration for different targets.

ifneq ($(findstring build_debug_x86, $(MAKECMDGOALS)), ) 
	.DEFAULT_GOAL = build_debug_x86
	ARCH         := -m32
	
	LFLAGS        = -DDEBUG -DM32 -g $(ARCH) $(PRE_LFLAGS)
	OUTPUT_DIR    = $(PRE_OUTPUT_DIR)Debug-x86/
	OBJ_DIR       = $(PRE_OBJ_DIR)Debug-x86/
	CONFIG        = Debug-x86
else ifneq ($(findstring build_release_x64, $(MAKECMDGOALS)), ) 
	.DEFAULT_GOAL = build_release_x64
	
	LFLAGS        = -DRELEASE -s -O3 $(PRE_LFLAGS)
	OUTPUT_DIR    = $(PRE_OUTPUT_DIR)Release-x64/
	OBJ_DIR       = $(PRE_OBJ_DIR)Release-x64/
	CONFIG        = Release-x64
else ifneq ($(findstring build_release_x86, $(MAKECMDGOALS)), ) 
	.DEFAULT_GOAL = build_release_x86
	ARCH         := -m32
	
	LFLAGS        = -DRELEASE -DM32 -s -O3 $(ARCH) $(PRE_LFLAGS)
	OUTPUT_DIR    = $(PRE_OUTPUT_DIR)Release-x86/
	OBJ_DIR       = $(PRE_OBJ_DIR)Release-x86/
	CONFIG        = Release-x86
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
endif

.PHONY : 
build_debug_x64 : project_promit all

all : $(OBJ_FILES)
	@echo -e "\n$(BLUE_BOLD)Linking C translations and building $(DESCRIPTION) '$(EXE_NAME)' ($(OS_NAME)).\n$(RESET)"
	$(CC) $^ $(ARCH) -o $(OUTPUT_DIR)$(EXE_NAME) -lm
	
.PHONY : 
build_debug_x86 : build_debug_x64

.PHONY :
build_release_x64: build_debug_x64

.PHONY :
build_release_x86: build_debug_x64

.SILENT : 
$(OBJ_DIR)%.c.obj : $(SRC_DIR)%.c
	@echo -e "$(GREEN)Building C translation '$(patsubst $(SRC_DIR)%.c,%.c, $<)', generating object '$@'$(RESET)"
	$(CC) -c $(LFLAGS) $(patsubst $(SRC_DIR)%.c,$(DEPENDENCY_DIR)%.d, $<) -I$(INCLUDE) $< -o $@

project_promit : 
	@echo -e "$(PURPLE_BOLD)<=======================================| Project Promit |=======================================>$(RESET)\n"
	@echo -e "$(CYAN)Building Project Promit on $(shell date) with configuration $(CONFIG).$(RESET)\n"
	@mkdir -p $(OBJ_DIR) $(OUTPUT_DIR) $(DEPENDENCY_DIR)

clean : 
	rm -rf $(PRE_OUTPUT_DIR)

.PHONY : clean

# Including dependencies.

-include $(wildcard $(DEPENDENCY_DIR)*.d)
