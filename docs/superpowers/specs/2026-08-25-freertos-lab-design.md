# FreeRTOS 학습 실습장 설계

작성일: 2026-08-25

## 목적

FreeRTOS 커널 API를 단계별로 학습한다. 실제 MCU 펌웨어 개발이 아니라
API와 스케줄러 동작 이해가 목표이므로, 툴체인 설치 부담이 없는
Windows 시뮬레이터 포트를 사용한다.

## 환경

- 커널: FreeRTOS-Kernel V11.1.0+ (`kernel/`, git clone, 수정하지 않음)
- 포트: `kernel/portable/MSVC-MingW/port.c` (MinGW gcc 호환)
- 컴파일러: MSYS2 gcc 16.1.0 / GNU Make 4.4.1
- 링크: `-lwinmm` (Win32 포트가 멀티미디어 타이머 사용)
- 힙: 기본 heap_4, 11강만 교체

Windows 포트는 FreeRTOS 태스크를 Windows 스레드 위에 얹어 흉내내므로
타이밍이 실시간이 아니다. 그러나 API 동작(큐, 세마포어, 우선순위 스케줄링
순서)은 동일하게 관찰되므로 학습 목적에 충분하다.

실제 ISR은 없지만 포트가 `vPortGenerateSimulatedInterrupt()` /
`vPortSetInterruptHandler()`를 제공하므로, `xSemaphoreGiveFromISR` +
`portYIELD_FROM_ISR`을 실제 인터럽트 컨텍스트에서 실습할 수 있다.
애플리케이션 정의 인터럽트 번호는 2번부터 사용한다.

### 시뮬레이터가 재현하지 못하는 것

Windows 포트는 FreeRTOS가 할당한 스택 버퍼를 실제 실행에 쓰지 않는다.
각 태스크는 `CreateThread()`로 만든 Windows 스레드 스택에서 실행되고,
FreeRTOS 스택 버퍼는 포트 내부 구조체를 담는 용도로만 쓰인다
(`pxPortInitialiseStack()` 주석 참조).

따라서 다음은 이 환경에서 재현되지 않으며, 이론 설명으로만 다룬다.

- 스택 오버플로우 검출 (`configCHECK_FOR_STACK_OVERFLOW`)
- `uxTaskGetStackHighWaterMark()`
- 정확한 실시간 타이밍 (ms 단위 지터 존재)

재현되는 것: 우선순위 스케줄링 순서, 선점, 블로킹/기아, 큐·세마포어·
뮤텍스의 모든 의미론, 힙 사용량과 할당 실패, 힙 단편화.

### 한글 출력

소스는 UTF-8이고 Windows 콘솔 기본 코드페이지는 CP949이므로 printf로는
한글이 깨진다. `common/lab.c`의 `lab_printf()`가 콘솔 출력 시 UTF-16으로
변환해 `WriteConsoleW()`로 직접 쓴다. 코드페이지와 무관하게 동작한다.
리다이렉트된 경우에는 UTF-8 바이트를 그대로 쓴다.
모든 레슨은 `printf` 대신 `LOG()` 또는 `lab_printf()`를 쓴다.

## 디렉터리 구조

```
FreeRTOS/
├─ kernel/                    FreeRTOS-Kernel (불변)
├─ common/
│   ├─ FreeRTOSConfig.h       기본 설정 (대부분 레슨이 공유)
│   ├─ hooks.c                malloc 실패 / 스택오버플로우 / idle / tick 훅
│   ├─ lab.h, lab.c           LOG() 매크로, 시뮬레이션 인터럽트 헬퍼
│   └─ common.mk              공용 빌드 규칙
├─ lessons/NN-topic/          main.c, Makefile, README.md
├─ build/                     산출물 (gitignore)
└─ docs/superpowers/specs/    설계 문서
```

## 빌드 구성

레슨별 Makefile은 실질 2줄이다.

```make
LESSON := 01-tasks
include ../../common/common.mk
```

`common.mk`는 레슨 디렉터리에 `FreeRTOSConfig.h`가 있으면 그것을,
없으면 `common/FreeRTOSConfig.h`를 사용한다. 설정 비교가 필요한
레슨(06 우선순위 역전, 11 메모리 관리)만 자체 설정 파일을 둔다.

타깃: `make`, `make run`, `make clean`

## 커리큘럼

기본 8강:

| # | 주제 | 핵심 |
|---|------|------|
| 01 | 태스크와 우선순위 | xTaskCreate, vTaskDelete, 우선순위별 실행 순서 |
| 02 | 딜레이와 주기 실행 | vTaskDelay vs xTaskDelayUntil, 드리프트 실측, catch-up |
| 03 | 큐 | xQueueSend/Receive, 블로킹 타임아웃, 구조체 전달 |
| 04 | 바이너리 세마포어 | 시뮬레이션 인터럽트로 ISR→태스크 지연 처리 |
| 05 | 카운팅 세마포어 | 리소스 풀 관리 |
| 06 | 뮤텍스 / 우선순위 역전 | 역전 재현 후 뮤텍스로 해결, 두 바이너리 비교 |
| 07 | 소프트웨어 타이머 | one-shot vs auto-reload, 콜백 제약 |
| 08 | 태스크 노티피케이션 | 경량 신호, 큐/세마포어 대체 |

심화 4강 (8강 완료 후 추가):

| # | 주제 | 핵심 |
|---|------|------|
| 09 | 이벤트 그룹 | AND/OR 대기, 랑데부 |
| 10 | 스트림/메시지 버퍼 | 바이트 스트림 vs 메시지 경계 |
| 11 | 메모리 관리 | heap_1~5 비교, 단편화 재현, 할당 실패 처리 |
| 12 | 런타임 통계와 훅 | uxTaskGetSystemState, CPU 사용률 |

## 레슨 구성

각 레슨 디렉터리는 세 파일을 갖는다.

- `README.md` — 개념 설명, 이 코드가 보여주는 것, 예상 출력, 과제 2~3개
- `main.c` — 상세 한글 주석
- `Makefile` — common.mk include

로그는 `[  120ms][TaskA] ...` 형식으로 시각과 태스크명을 찍어
스케줄링 순서가 눈에 보이도록 한다.

## 오류 처리

전 레슨에 기본 활성화한다. 실수했을 때 조용히 죽지 않고 명확히 알리는 것이
학습 목적상 중요하다.

- `configASSERT`
- `vApplicationMallocFailedHook`
- `vApplicationStackOverflowHook`
- `configCHECK_FOR_STACK_OVERFLOW = 2` (Windows 포트에서는 동작하지 않지만,
  실제 타깃으로 옮길 때를 대비해 켜 둔다)

## 검증

각 레슨을 실제로 `make run` 실행해 README에 적은 예상 출력과 일치하는지
확인한 뒤 다음 레슨으로 넘어간다.
