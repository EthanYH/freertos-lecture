# FreeRTOS 학습 실습장

FreeRTOS 커널 API를 단계별로 익히기 위한 실습 저장소.
툴체인 설치 부담이 없도록 **Windows 시뮬레이터 포트**를 사용한다.

각 레슨은 하나의 개념만 격리해서 다루며, 독립 실행 파일로 빌드된다.

## 필요한 것

| | 버전 | 확인 |
|---|---|---|
| MSYS2 gcc | 13 이상 | `gcc --version` |
| GNU Make | 4.x | `make --version` |
| git | | `git --version` |

ARM 툴체인이나 QEMU, CMake는 필요 없다.

## 시작하기

커널은 이 저장소에 포함되어 있지 않다. clone 후 한 번 받아야 한다.

```bash
git clone <이 저장소 URL>
cd FreeRTOS
git clone --depth 1 https://github.com/FreeRTOS/FreeRTOS-Kernel.git kernel
```

그다음 아무 레슨이나 실행한다.

```bash
cd lessons/01-tasks
make run
```

## 레슨

| # | 주제 | 핵심 |
|---|------|------|
| [01](lessons/01-tasks/) | 태스크와 우선순위 | `xTaskCreate`, 선점, 기아, `vTaskDelete` |
| [02](lessons/02-delay/) | 딜레이와 주기 실행 | `vTaskDelay` vs `xTaskDelayUntil`, 드리프트, catch-up |
| 03 | 큐 | 준비 중 |
| 04 | 바이너리 세마포어 / ISR 동기화 | 준비 중 |
| 05 | 카운팅 세마포어 | 준비 중 |
| 06 | 뮤텍스와 우선순위 역전 | 준비 중 |
| 07 | 소프트웨어 타이머 | 준비 중 |
| 08 | 태스크 노티피케이션 | 준비 중 |
| 09 | 이벤트 그룹 | 준비 중 |
| 10 | 스트림/메시지 버퍼 | 준비 중 |
| 11 | 메모리 관리 | 준비 중 |
| 12 | 런타임 통계와 훅 | 준비 중 |

각 레슨 폴더의 `README.md`에 개념 설명, 예상 출력, 과제가 있다.

## 구조

```
├─ kernel/          FreeRTOS-Kernel (clone로 받음, 추적하지 않음)
├─ common/
│   ├─ FreeRTOSConfig.h   기본 커널 설정
│   ├─ hooks.c            malloc 실패 / 스택오버플로우 / assert 훅
│   ├─ lab.c, lab.h       LOG() 매크로, 시간 측정 헬퍼
│   └─ common.mk          공용 빌드 규칙
├─ lessons/NN-topic/  main.c, Makefile, README.md
├─ build/             산출물 (추적하지 않음)
└─ docs/
    ├─ api/task-api.md              Task API 한글 레퍼런스
    └─ superpowers/specs/           설계 문서
```

레슨의 `Makefile`은 두 줄이다.

```make
LESSON := 01-tasks
include ../../common/common.mk
```

`common.mk`는 레슨 폴더에 `FreeRTOSConfig.h`가 있으면 그것을, 없으면
`common/FreeRTOSConfig.h`를 쓴다. 설정 비교가 필요한 레슨만 자체 설정을 둔다.

## 시뮬레이터의 한계

Windows 포트는 FreeRTOS 태스크를 Windows 스레드 위에 얹어 흉내낸다.
무엇이 재현되고 무엇이 안 되는지 아는 것이 중요하다.

**재현되지 않는 것**

| 항목 | 이유 |
|---|---|
| 스택 오버플로우 검출 | 태스크가 Windows 스레드 스택에서 실행됨 |
| `uxTaskGetStackHighWaterMark()` | 같은 이유 |
| 정확한 실시간 타이밍 | Windows 스케줄러 위에 있어 ms 단위 지터 존재 |

`kernel/portable/MSVC-MingW/port.c`의 `pxPortInitialiseStack()` 주석 참고.
FreeRTOS가 할당한 스택 버퍼는 포트 내부 구조체를 담는 용도로만 쓰인다.

**정확히 재현되는 것**

우선순위 스케줄링 순서, 선점, 블로킹과 기아, 큐·세마포어·뮤텍스의 모든
의미론, 힙 사용량과 할당 실패, 힙 단편화.

실제 ISR은 없지만 포트가 `vPortGenerateSimulatedInterrupt()`를 제공하므로
`xSemaphoreGiveFromISR` 등은 실제 인터럽트 컨텍스트에서 실습할 수 있다.

## 한글 출력

소스는 UTF-8이고 Windows 콘솔 기본 코드페이지는 CP949라, `printf`로는 깨진다.
`common/lab.c`의 `lab_printf()`가 콘솔에 쓸 때 UTF-16으로 변환해
`WriteConsoleW()`로 직접 출력하므로 코드페이지와 무관하게 동작한다.
파일로 리다이렉트하면 UTF-8 바이트를 그대로 쓴다.

**모든 레슨은 `printf` 대신 `LOG()` 또는 `lab_printf()`를 쓴다.**

## 라이선스

이 저장소의 실습 코드와 문서는 MIT.
`kernel/`은 포함되어 있지 않으며, FreeRTOS-Kernel은 MIT 라이선스다.
