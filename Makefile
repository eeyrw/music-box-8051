# toolchain
CC           = sdcc
CP           = sdobjcopy
AS           = sdas8051
LD			= sdld
HEX          = packihx
BIN          = $(CP) -O binary -S

# define mcu, specify the target processor
F_CPU   ?= 16000000
MCU          = mcs51
ARCH = mcs51

# ------------------------------------------------------
# Usually SDCC's small memory model is the best choice.  If
# you run out of internal RAM, you will need to declare
# variables as "xdata", or switch to largeer model

# Memory Model (small, medium, large, huge)
MODEL  = large
# ------------------------------------------------------
# Memory Layout
# PRG Size = 32K Bytes
CODE_SIZE = --code-loc 0x0000 --code-size 61440
# INT-MEM Size = 256 Bytes
IRAM_SIZE = --idata-loc 0x0000  --iram-size 256
# EXT-MEM Size = 1K Bytes
XRAM_SIZE = --xram-loc 0x0000 --xram-size 1792

# all the files will be generated with this name (main.elf, main.bin, main.hex, etc)
PROJECT_NAME=music-box-8051

# specify define
DEFS       = NO_RUN_TEST
DEFS += STC8

# define root dir
ROOT_DIR     = .

# define include dir
INCLUDE_DIRS = WavetableSynthesizer

# define lib dir
LIBDIR   = 

# user specific

SRC 	+= main.c
SRC 	+= WavetableSynthesizer/AlgorithmTest.c
SRC 	+= WavetableSynthesizer/SynthCore.c
SRC 	+= WavetableSynthesizer/Player.c
SRC 	+= UartRedirect.c
SRC 	+= Bsp.c
SRC 	+= WavetableSynthesizer/WaveTable.c
SRC 	+= WavetableSynthesizer/EnvelopTable.c
SRC 	+= WavetableSynthesizer/score.c

ASM_SRC =
ASM_SRC   += WavetableSynthesizer/PeriodTimer.s
ASM_SRC   += WavetableSynthesizer/PlayerUtil.s
ASM_SRC   += WavetableSynthesizer/SynthCoreAsm.s

# ASM_SRC   += WavetableSynthesizer/Synth_testbench.s
# ASM_SRC   += WavetableSynthesizer/UpdateTick_testbench.s

INC_DIR  = $(patsubst %, -I%, $(INCLUDE_DIRS))

# run from Flash
DDEFS	 = $(patsubst %, -D%, $(DEFS))
DEPS  = $(SRC:.c=.d)
OBJECTS  = $(SRC:.c=.rel) $(ASM_SRC:.s=.rel)
OTHER_OUTPUTS += $(ASM_SRC:.s=.asm) $(SRC:.c=.asm)
OTHER_OUTPUTS += $(ASM_SRC:.s=.lst) $(SRC:.c=.lst)
OTHER_OUTPUTS += $(ASM_SRC:.s=.rst) $(SRC:.c=.rst)
OTHER_OUTPUTS += $(ASM_SRC:.s=.sym) $(SRC:.c=.sym)


CFLAGS  = -m$(ARCH) -p$(MCU) --model-$(MODEL) --std-sdcc11
CFLAGS += -DF_CPU=$(F_CPU)UL -I. -I$(LIBDIR) $(DDEFS) --stack-auto
ASFLAGS  = -plosgff -l -s
LD_FLAGS = -m$(ARCH) -l$(ARCH) --out-fmt-ihx -m$(MCU_MODEL) --model-$(MODEL) $(CODE_SIZE) $(IRAM_SIZE) $(XRAM_SIZE) --stack-auto

#
# makefile rules
#
all: $(OBJECTS) $(PROJECT_NAME).ihx $(PROJECT_NAME).hex $(PROJECT_NAME).bin

# Don't delete dependency files
.PRECIOUS: %.d

# Don't rebuild deps if cleaning
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
# Beacuse SDCC's assembler has no way to auto output dependency info,
# the dependency is manually written here.	
PeriodTimer.rel: SynthCore.inc 8051.inc Synth.inc UpdateTick.inc WaveTable.inc
Synth_testbench.rel: SynthCore.inc 8051.inc Synth.inc UpdateTick.inc
UpdateTick_testbench.rel: SynthCore.inc 8051.inc Synth.inc UpdateTick.inc
SynthCoreAsm.rel: SynthCore.inc WaveTable.inc
PlayerUtil.rel: SynthCore.inc Player.inc
endif




%.rel: %.c Makefile
	@echo [CC] $(notdir $<)
# Output dependency
	@$(CC) $(CFLAGS) $(INC_DIR) -MM -c $< > $(patsubst %.c,%.d,$<)
# Do compiling
	@$(CC) $(CFLAGS) $(INC_DIR) -c $< -o $@
	


%.rel: %.s
	@echo [AS] $(notdir $<)
	@$(AS) $(ASFLAGS) $<

%.ihx: $(OBJECTS)
	@echo [LD] $(PROJECT_NAME).ihx
	@$(CC) $(LD_FLAGS) $(OBJECTS) -o $@
	
%.hex: %.ihx
	@echo [HEX] $(PROJECT_NAME).hex
	@$(HEX) $< > $@	
	
%.bin: %.ihx
	@echo [BIN] $(PROJECT_NAME).bin
	@$(CP) -I ihex -O binary $< $@

flash: $(PROJECT_NAME).hex
	stm8flash -c stlinkv2 -p $(MCU) -w $(PROJECT_NAME).hex
	
clean:
	@echo [RM] OBJ
	@-rm -rf $(OBJECTS)
	@echo [RM] HEX
	@-rm -rf $(PROJECT_NAME).ihx
	@echo [RM] Intermediate outputs
	@-rm -rf $(OTHER_OUTPUTS)
	@-rm -rf $(PROJECT_NAME).lk
	@-rm -rf $(PROJECT_NAME).map	
	@-rm -rf $(PROJECT_NAME).cdb	
	@-rm -rf $(PROJECT_NAME).hex
	@-rm -rf $(PROJECT_NAME).bin
	@-rm -rf $(PROJECT_NAME).mem
	@-rm -rf $(DEPS)