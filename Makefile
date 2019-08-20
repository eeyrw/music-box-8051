#
# SDCC Makefile for mcs51
#
# ------------------------------------------------------
# PATH

INCDIR  = .
SRCDIR  =.
OBJDIR  = ./obj
HEXDIR = ./hex

LIBDIR  =  
# ------------------------------------------------------
# Target and Source
TARGET = $(HEXDIR)/music-box-8051

#C_SRC = $(SRCDIR)/helloworld.c
#C_SRC = $(wildcard  $(SRCDIR)/*.c)
C_SRC 	+= $(SRCDIR)/main.c
C_SRC 	+= $(SRCDIR)/AlgorithmTest.c
C_SRC 	+= $(SRCDIR)/SynthCore.c
C_SRC 	+= $(SRCDIR)/Player.c
C_SRC 	+= $(SRCDIR)/UartRedirect.c
C_SRC 	+= $(SRCDIR)/WaveTable_Celesta_C5.c
C_SRC 	+= $(SRCDIR)/EnvelopTable.c
C_SRC 	+= $(SRCDIR)/score.c

ASM_SRC = 

# ASM_SRC   += PlayerUtil.s
# ASM_SRC   += Synth.s

C_SRC_FILE = $(notdir $(C_SRC))
C_OBJ_FILE = $(C_SRC_FILE:%.c=%.c.rel)

#OBJ = $(OBJDIR)/helloworld.c.rel      
OBJ = $(addprefix $(OBJDIR)/, $(C_OBJ_FILE)) 

# ------------------------------------------------------
# Usually SDCC's small memory model is the best choice.  If
# you run out of internal RAM, you will need to declare
# variables as "xdata", or switch to largeer model

# Memory Model (small, medium, large, huge)
MODEL  = small
# ------------------------------------------------------
# Memory Layout
# PRG Size = 4K Bytes
CODE_SIZE = --code-loc 0x0000 --code-size 4096
# INT-MEM Size = 256 Bytes
IRAM_SIZE = --idata-loc 0x0000  --iram-size 256
# EXT-MEM Size = 4K Bytes
XRAM_SIZE = --xram-loc 0x0000 --xram-size 4096



# ------------------------------------------------------
# SDCC

CC = sdcc
AS = sdas8051

MCU_MODEL = mcs51

#LIBS    = 
#LIBPATH = -L $(LIBDIR)

#DEBUG = --debug
AFLAGS = -l -s
CFLAGS = -I./inc -I$(INCDIR) -m$(MCU_MODEL) --model-$(MODEL) --out-fmt-ihx --no-xinit-opt $(DEBUG)
LFLAGS = $(LIBPATH) $(LIBS) -m$(MCU_MODEL) --model-$(MODEL) $(CODE_SIZE) $(IRAM_SIZE) $(XRAM_SIZE) --out-fmt-ihx  $(DEBUG)

# ------------------------------------------------------
# S = @

.PHONY: all clean

all: $(TARGET).hex


$(HEXDIR)/%.hex: $(OBJDIR)/%.ihx
	$(S) packihx $^ > $@

$(OBJDIR)/%.ihx: $(OBJ)
	$(S) $(CC) -o $@ $(LFLAGS) $^

$(OBJDIR)/%.c.rel: $(SRCDIR)/%.c
	$(S) $(CC) -o $@ $(CFLAGS) -c $^

$(OBJDIR)/%.asm.rel: $(SRCDIR)/%.asm
	$(S) $(AS) $(AFLAGS) -o $@ $^ 

flash: $(TARGET)
	#./stm8flash -c stlinkv2 -p $(MCU) -w $(TARGET)

clean:
	$(S) rm -rf $(OBJDIR)/*
	$(S) rm -rf $(TARGET).hex