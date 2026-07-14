# toolchain
CC           = sdcc
CP           = sdobjcopy
AS           = sdas8051
LD			= sdld
HEX          = packihx
BIN          = $(CP) -O binary -S

ifeq ($(OS),Windows_NT)
PYTHON ?= D:/Python310/python.exe
PS     ?= powershell -NoProfile -ExecutionPolicy Bypass -Command
else
PYTHON ?= python3
endif

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
# PRG Size = 64K Bytes
CODE_SIZE = --code-loc 0x0000 --code-size 65536
# INT-MEM Size = 256 Bytes
IRAM_SIZE = --idata-loc 0x0000  --iram-size 256
# EXT-MEM Size = 3K Bytes
XRAM_SIZE = --xram-loc 0x0000 --xram-size 3072

# all the files will be generated with this name (main.elf, main.bin, main.hex, etc)
PROJECT_NAME=music-box-8051

# Storage backend: runtime-selectable via function pointer dispatcher
# Both backends are always compiled; backend chosen by storage_auto_detect() at boot

# specify define
DEFS       = NO_RUN_TEST
DEFS += STC8

# define root dir
ROOT_DIR     = .

# define include dir
INCLUDE_DIRS = Player Synthesizer

# define lib dir
LIBDIR   = 

# user specific

SRC 	+= main.c
SRC 	+= Synthesizer/SynthCore.c
SRC 	+= Player/Player.c
SRC 	+= UartRedirect.c
SRC 	+= Bsp.c
SRC 	+= Protocol.c
SRC 	+= Synthesizer/WaveTable.c
SRC 	+= Synthesizer/CompressorGenerated.c

# Storage backends (both always compiled)
SRC 	+= Storage.c
SRC 	+= Storage_Internal.c
SRC 	+= Storage_SPI.c
SRC 	+= SpiFlash.c
SRC 	+= scoreList.c

ifneq ($(filter RUN_TEST,$(DEFS)),)
SRC 	+= Synthesizer/AlgorithmTest.c
endif

ASM_SRC =
ASM_SRC   += Synthesizer/SynthCoreAsm.s

ifneq ($(filter RUN_TEST,$(DEFS)),)
ASM_SRC   += Synthesizer/Synth_testbench.s
ASM_SRC   += Synthesizer/UpdateTick_testbench.s
else
ASM_SRC   += Synthesizer/PeriodTimer.s
endif

INC_DIR  = $(patsubst %, -I%, $(INCLUDE_DIRS))
AS_INC   = $(INC_DIR)

COMPRESSOR_INPUT_BITS ?= 11
COMPRESSOR_OUTPUT_BITS ?= 8
COMPRESSOR_OUTPUT_MIN ?= -127
COMPRESSOR_OUTPUT_MAX ?= 127
COMPRESSOR_PRESET ?= loud
COMPRESSOR_THRESHOLD_DB ?=
COMPRESSOR_RATIO ?=
COMPRESSOR_MAKEUP_DB ?=
COMPRESSOR_DB_ARGS = --preset $(COMPRESSOR_PRESET)
ifneq ($(strip $(COMPRESSOR_THRESHOLD_DB)),)
COMPRESSOR_DB_ARGS += --threshold-db $(COMPRESSOR_THRESHOLD_DB)
endif
ifneq ($(strip $(COMPRESSOR_RATIO)),)
COMPRESSOR_DB_ARGS += --ratio $(COMPRESSOR_RATIO)
endif
ifneq ($(strip $(COMPRESSOR_MAKEUP_DB)),)
COMPRESSOR_DB_ARGS += --makeup-db $(COMPRESSOR_MAKEUP_DB)
endif
COMPRESSOR_GEN = Synthesizer/CompressorGenerated.h Synthesizer/CompressorGenerated.c
COMPRESSOR_STAMP = Synthesizer/CompressorGenerated.stamp
COMPRESSOR_GEN_ARGS = --input-bits $(COMPRESSOR_INPUT_BITS) --output-bits $(COMPRESSOR_OUTPUT_BITS) --output-min $(COMPRESSOR_OUTPUT_MIN) --output-max $(COMPRESSOR_OUTPUT_MAX) $(COMPRESSOR_DB_ARGS) --out-header Synthesizer/CompressorGenerated.h --out-source Synthesizer/CompressorGenerated.c

# run from Flash
DDEFS	 = $(patsubst %, -D%, $(DEFS))
DEPS  = $(SRC:.c=.d)
OBJECTS  = $(SRC:.c=.rel) $(ASM_SRC:.s=.rel)
OTHER_OUTPUTS += $(ASM_SRC:.s=.asm) $(SRC:.c=.asm)
OTHER_OUTPUTS += $(ASM_SRC:.s=.lst) $(SRC:.c=.lst)
OTHER_OUTPUTS += $(ASM_SRC:.s=.rst) $(SRC:.c=.rst)
OTHER_OUTPUTS += $(ASM_SRC:.s=.sym) $(SRC:.c=.sym)
TEST_OBJECTS = Synthesizer/AlgorithmTest.rel Synthesizer/Synth_testbench.rel Synthesizer/UpdateTick_testbench.rel
TEST_OUTPUTS = $(TEST_OBJECTS:.rel=.asm) $(TEST_OBJECTS:.rel=.lst) $(TEST_OBJECTS:.rel=.rst) $(TEST_OBJECTS:.rel=.sym)
TEST_DEPS = Synthesizer/AlgorithmTest.d


CFLAGS  = -m$(ARCH) -p$(MCU) --model-$(MODEL) --std-sdcc11
CFLAGS += -DF_CPU=$(F_CPU)UL -I. $(patsubst %, -I%, $(LIBDIR)) $(DDEFS) --stack-auto
ASFLAGS  = $(AS_INC) -plosgff -l -s
LD_FLAGS = -m$(ARCH) -l$(ARCH) --out-fmt-ihx -m$(MCU_MODEL) --model-$(MODEL) $(CODE_SIZE) $(IRAM_SIZE) $(XRAM_SIZE) --stack-auto

#
# makefile rules
#
all: $(OBJECTS) $(PROJECT_NAME).ihx $(PROJECT_NAME).hex $(PROJECT_NAME).bin

# Don't delete dependency files
.PRECIOUS: %.d

.PHONY: FORCE

# Don't rebuild deps if cleaning
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPS)
# Beacuse SDCC's assembler has no way to auto output dependency info,
# the dependency is manually written here.	
Synthesizer/PeriodTimer.rel: Synthesizer/SynthCore.inc Synthesizer/8051.inc Synthesizer/Synth.inc Synthesizer/UpdateTick.inc Synthesizer/WaveTable.inc
Synthesizer/Synth_testbench.rel: Synthesizer/SynthCore.inc Synthesizer/8051.inc Synthesizer/Synth.inc Synthesizer/UpdateTick.inc
Synthesizer/UpdateTick_testbench.rel: Synthesizer/SynthCore.inc Synthesizer/8051.inc Synthesizer/Synth.inc Synthesizer/UpdateTick.inc
Synthesizer/SynthCoreAsm.rel: Synthesizer/SynthCore.inc Synthesizer/WaveTable.inc
Synthesizer/SynthCore.rel: Synthesizer/CompressorGenerated.h
Synthesizer/AlgorithmTest.rel: Synthesizer/CompressorGenerated.h
Synthesizer/CompressorGenerated.rel: Synthesizer/CompressorGenerated.h
endif




ifeq ($(OS),Windows_NT)
%.rel: %.c Makefile
	@echo [CC] $(notdir $<)
# Output dependency
	@$(CC) $(CFLAGS) $(INC_DIR) -MM -c $< | $(PS) "$$input | ForEach-Object { $$_ -replace '^[^:]*:', '$@:' } | Set-Content -Encoding ASCII '$(patsubst %.c,%.d,$<)'"
# Do compiling
	@$(CC) $(CFLAGS) $(INC_DIR) -c $< -o $@

$(COMPRESSOR_STAMP): FORCE
	@$(PS) "$$genArgs = '$(COMPRESSOR_GEN_ARGS)'; function NewerThan($$a, $$b) { (Test-Path $$a) -and ((-not (Test-Path $$b)) -or ((Get-Item $$a).LastWriteTime -gt (Get-Item $$b).LastWriteTime)) }; $$first = if (Test-Path '$@') { Get-Content '$@' -TotalCount 1 } else { '' }; if ((-not (Test-Path '$@')) -or (-not (Test-Path 'Synthesizer/CompressorGenerated.h')) -or (-not (Test-Path 'Synthesizer/CompressorGenerated.c')) -or ($$first -ne $$genArgs) -or (NewerThan 'tools/gen_compressor.py' '$@') -or (NewerThan 'Makefile' '$@')) { Write-Host '[GEN] CompressorGenerated'; $$genArgv = $$genArgs -split ' '; & $(PYTHON) tools/gen_compressor.py @genArgv; if ($$LASTEXITCODE -ne 0) { exit $$LASTEXITCODE }; Set-Content -Encoding ASCII '$@' $$genArgs }"
else
%.rel: %.c Makefile
	@echo [CC] $(notdir $<)
# Output dependency
	@$(CC) $(CFLAGS) $(INC_DIR) -MM -c $< | sed 's|^[^:]*:|$@:|' > $(patsubst %.c,%.d,$<)
# Do compiling
	@$(CC) $(CFLAGS) $(INC_DIR) -c $< -o $@

$(COMPRESSOR_STAMP): FORCE
	@args='$(COMPRESSOR_GEN_ARGS)'; \
	if [ ! -f $@ ] || [ ! -f Synthesizer/CompressorGenerated.h ] || [ ! -f Synthesizer/CompressorGenerated.c ] || [ "x$$(sed -n '1p' $@ 2>/dev/null)" != "x$$args" ] || [ tools/gen_compressor.py -nt $@ ] || [ Makefile -nt $@ ]; then \
		echo [GEN] CompressorGenerated; \
		$(PYTHON) tools/gen_compressor.py $$args; \
		printf '%s\n' "$$args" > $@; \
	fi
endif


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

# stcgal settings (open-source STC MCU ISP tool for Linux)
# install: pip3 install stcgal
# https://github.com/grigorig/stcgal
STCGAL_PORT   ?= /dev/ttyUSB0
STCGAL_BAUD   ?= 115200
STCGAL_PROTO  ?= stc8d
STCGAL_BOOT_CMD ?= auto

BOOT_SCRIPT = $(PYTHON) tools/boot.py $(STCGAL_PORT) $(STCGAL_BAUD)

boot:
	@echo [BOOT] sending RESET frame to $(STCGAL_PORT)
	$(BOOT_SCRIPT)

flash: $(PROJECT_NAME).ihx
	@echo [BOOT] sending RESET frame to $(STCGAL_PORT)
	@$(BOOT_SCRIPT)
	@echo [FLASH] $(PROJECT_NAME).ihx via stcgal
	stcgal -P $(STCGAL_PROTO) -p $(STCGAL_PORT) -b $(STCGAL_BAUD) $<
	
ifeq ($(OS),Windows_NT)
clean:
	@echo [RM] OBJ
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue $(foreach f,$(OBJECTS),'$(f)')"
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue $(foreach f,$(TEST_OBJECTS),'$(f)')"
	@echo [RM] HEX
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue '$(PROJECT_NAME).ihx'"
	@echo [RM] Intermediate outputs
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue $(foreach f,$(OTHER_OUTPUTS),'$(f)')"
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue $(foreach f,$(TEST_OUTPUTS),'$(f)')"
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue '$(PROJECT_NAME).lk' '$(PROJECT_NAME).map' '$(PROJECT_NAME).cdb' '$(PROJECT_NAME).hex' '$(PROJECT_NAME).bin' '$(PROJECT_NAME).mem'"
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue $(foreach f,$(DEPS),'$(f)')"
	@$(PS) "Remove-Item -Force -Recurse -ErrorAction SilentlyContinue $(foreach f,$(TEST_DEPS),'$(f)')"
else
clean:
	@echo [RM] OBJ
	@-rm -rf $(OBJECTS)
	@-rm -rf $(TEST_OBJECTS)
	@echo [RM] HEX
	@-rm -rf $(PROJECT_NAME).ihx
	@echo [RM] Intermediate outputs
	@-rm -rf $(OTHER_OUTPUTS)
	@-rm -rf $(TEST_OUTPUTS)
	@-rm -rf $(PROJECT_NAME).lk
	@-rm -rf $(PROJECT_NAME).map	
	@-rm -rf $(PROJECT_NAME).cdb	
	@-rm -rf $(PROJECT_NAME).hex
	@-rm -rf $(PROJECT_NAME).bin
	@-rm -rf $(PROJECT_NAME).mem
	@-rm -rf $(DEPS)
	@-rm -rf $(TEST_DEPS)
endif
