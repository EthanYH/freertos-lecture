# =============================================================================
# 공용 빌드 규칙
#
# 각 레슨의 Makefile은 이렇게 두 줄이면 된다:
#     LESSON := 01-tasks
#     include ../../common/common.mk
#
# 타깃:  make        빌드
#        make run    빌드 후 실행
#        make clean  산출물 삭제
# =============================================================================

# 이 파일(common/common.mk)의 위치로부터 프로젝트 루트를 구한다.
# 레슨 디렉터리에서 make를 돌려도 경로가 깨지지 않게 하려는 것.
COMMON_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
ROOT       := $(abspath $(COMMON_DIR)/..)

KERNEL     := $(ROOT)/kernel
BUILD      := $(ROOT)/build
OBJDIR     := $(BUILD)/obj/$(LESSON)
TARGET     := $(BUILD)/$(LESSON).exe

# 힙 구현. 11강에서 레슨 Makefile이 HEAP := heap_1 처럼 덮어쓴다.
HEAP ?= heap_4

# -----------------------------------------------------------------------------
# 인클루드 경로
#
# 레슨 디렉터리를 맨 앞에 둔다. 레슨에 자체 FreeRTOSConfig.h가 있으면
# 그것이 common/FreeRTOSConfig.h보다 먼저 발견된다.
# -----------------------------------------------------------------------------
INCLUDES := -I. \
            -I$(ROOT)/common \
            -I$(KERNEL)/include \
            -I$(KERNEL)/portable/MSVC-MingW

# -----------------------------------------------------------------------------
# 소스
# -----------------------------------------------------------------------------
KERNEL_SRC := $(KERNEL)/tasks.c \
              $(KERNEL)/list.c \
              $(KERNEL)/queue.c \
              $(KERNEL)/timers.c \
              $(KERNEL)/event_groups.c \
              $(KERNEL)/stream_buffer.c \
              $(KERNEL)/portable/MSVC-MingW/port.c \
              $(KERNEL)/portable/MemMang/$(HEAP).c

COMMON_SRC := $(ROOT)/common/hooks.c \
              $(ROOT)/common/lab.c

LESSON_SRC := $(wildcard *.c)

SRC := $(LESSON_SRC) $(COMMON_SRC) $(KERNEL_SRC)

# 소스 경로가 제각각이므로 오브젝트 이름을 평탄화한다.
OBJ := $(addprefix $(OBJDIR)/,$(notdir $(SRC:.c=.o)))
VPATH := $(sort $(dir $(SRC)))

# -----------------------------------------------------------------------------
# 컴파일 옵션
# -----------------------------------------------------------------------------
# gcc는 컴파일 중 임시 파일을 만든다. 환경에 따라 TMP/TEMP가 레시피까지
# 전달되지 않아 gcc가 쓰기 불가능한 경로로 폴백하는 경우가 있으므로,
# 빌드 디렉터리 안에 임시 폴더를 고정해 둔다.
# 경로는 반드시 Windows 형식이어야 한다. gcc는 네이티브 프로그램이라
# MSYS 형식(/c/...)을 이해하지 못하고 쓰기 불가능한 경로로 폴백한다.
TMPDIR_POSIX := $(BUILD)/tmp
TMPDIR       := $(shell cygpath -m "$(TMPDIR_POSIX)" 2>/dev/null || echo "$(TMPDIR_POSIX)")
TMP          := $(TMPDIR)
TEMP         := $(TMPDIR)
export TMPDIR TMP TEMP

CC      := gcc
CFLAGS  := -O0 -g -Wall -Wextra -Wno-unused-parameter $(INCLUDES)
LDFLAGS := -lwinmm            # Win32 포트가 멀티미디어 타이머(timeSetEvent) 사용

# -----------------------------------------------------------------------------
# 규칙
# -----------------------------------------------------------------------------
.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJ) | $(BUILD)
	@echo "  LD    $@"
	@$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: %.c | $(OBJDIR) $(TMPDIR_POSIX)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD) $(OBJDIR) $(TMPDIR_POSIX):
	@mkdir -p $@

run: $(TARGET)
	@echo "----- $(LESSON) 실행 -----"
	@$(TARGET)

clean:
	@rm -rf $(OBJDIR) $(TARGET)
	@echo "  CLEAN $(LESSON)"
