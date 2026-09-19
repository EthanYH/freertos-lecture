# 4강: 바이너리 세마포어와 ISR 동기화

## 실행

```bash
cd lessons/04-semaphore
make run
```

약 2.5초 뒤 스스로 종료한다.

## 배우는 것

- **지연 인터럽트 처리** — ISR은 신호만 주고, 실제 일은 태스크가 한다
- `xSemaphoreGiveFromISR()` / `xSemaphoreTake()`
- `portYIELD_FROM_ISR()`을 빠뜨리면 **응답이 최대 1틱 늦어진다**
- 바이너리 세마포어는 **세지 않는다** — 빠른 이벤트는 유실된다

## Windows에서 어떻게 ISR을 실습하는가

시뮬레이터에는 실제 인터럽트가 없지만, 포트가 이 API를 제공한다.

```c
/* 벡터 테이블에 ISR을 등록하는 것에 해당 */
vPortSetInterruptHandler( SIM_IRQ_NUM, prvSimulatedIsr );

/* 별도 Windows 스레드에서 인터럽트를 일으킨다 */
vPortGenerateSimulatedInterruptFromWindowsThread( SIM_IRQ_NUM );
```

인터럽트를 일으키는 쪽은 **FreeRTOS 태스크가 아니라 순수 Windows 스레드**다.
커널과 무관하게 돌기 때문에, 실제 주변장치처럼 완전히 비동기적이다.

> 인터럽트 번호는 **2번부터 31번**까지 쓸 수 있다.
> 0번은 Yield, 1번은 Tick이 이미 쓰고 있다
> (`portINTERRUPT_APPLICATION_DEFINED_START`).

## 지연 인터럽트 처리란

ISR 안에서 일을 다 하면 안 되는 이유는 분명하다. ISR이 도는 동안
**다른 모든 인터럽트가 지연**되고, 스케줄러도 개입하지 못한다.
ISR이 1ms 걸리면 시스템 전체의 응답성이 1ms 나빠진다.

그래서 이렇게 나눈다.

```
하드웨어 인터럽트
   ↓
ISR: 최소한의 일만 (레지스터 읽기, 세마포어 주기)  ← 수 us
   ↓ 세마포어
태스크: 실제 처리 (파싱, 계산, 로깅, 블로킹 가능)  ← 얼마든지
```

ISR에서 지켜야 할 규칙:

1. **최대한 짧게**
2. **`printf` 같은 무거운 호출 금지** — 그래서 이 실습의 ISR은 시각만 기록한다
3. **블로킹 금지** — 기다릴 태스크가 없으므로 애초에 불가능하다
4. **`FromISR` 접미사가 붙은 API만** 호출 가능

## 실습 1 — 정상 동작

```c
static uint32_t prvSimulatedIsr( void )
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ullIsrAtUs = lab_now_us();                      /* 시각만 기록 */
    xSemaphoreGiveFromISR( xSem, &xHigherPriorityTaskWoken );

    return ( uint32_t ) xHigherPriorityTaskWoken;   /* yield 요청 */
}
```

### 실행 결과

```
[   145 ms][Handler  ] ISR #1 처리  | 응답 지연 37 us
[   296 ms][Handler  ] ISR #2 처리  | 응답 지연 37 us
[   441 ms][Handler  ] ISR #3 처리  | 응답 지연 29 us
[   591 ms][Handler  ] ISR #4 처리  | 응답 지연 35 us
```

**ISR이 신호를 준 뒤 30µs 만에 태스크가 실행됐다.**

태스크는 그 사이 150ms 동안 `Blocked` 상태로 CPU를 전혀 쓰지 않았다.

## 실습 2 — yield를 빠뜨리면

핸들러가 `0`을 반환하도록만 바꿨다. 실제 Cortex-M 코드에서
`portYIELD_FROM_ISR(xHigherPriorityTaskWoken);` 줄을 지운 것과 같다.

### 실행 결과

```
[   883 ms][Handler  ] ISR #1 처리  | 응답 지연 908 us
[  1027 ms][Handler  ] ISR #2 처리  | 응답 지연 1031 us
[  1171 ms][Handler  ] ISR #3 처리  | 응답 지연 1045 us
[  1316 ms][Handler  ] ISR #4 처리  | 응답 지연 1097 us
[  1458 ms][Handler  ] ISR #5 처리  | 응답 지연 1841 us
```

**30µs → 1000µs. 약 30배 느려졌다.**

### 왜 이렇게 되는가

`xSemaphoreGiveFromISR()`은 태스크를 `Blocked` → `Ready`로 옮긴다.
하지만 **스케줄러를 부르지는 않는다.** ISR이 끝나면 원래 실행 중이던
컨텍스트로 돌아간다.

그러면 그 태스크는 언제 실행될까? **다음 틱 인터럽트 때**다.
틱이 1ms이므로 0~1000µs를 더 기다리게 된다.

```
yield 함:    [ISR] → 즉시 스위칭 → [태스크]              30 us
yield 안 함: [ISR] → 원래대로 복귀 → ... → [틱] → [태스크]  최대 1000 us
```

**주의할 점은 이게 "버그"로 보이지 않는다는 것이다.** 기능은 정상 동작한다.
단지 느릴 뿐이다. 그래서 발견하기 어렵고, 1ms가 중요한 시스템에서
치명적이다.

> 실무에서 `portYIELD_FROM_ISR()` 누락은 가장 흔한 실수 중 하나다.
> `FromISR` 함수를 썼다면 **반드시 짝으로 따라와야 한다**고 외워두는 게 좋다.

```c
void UART_IRQHandler(void)
{
    BaseType_t xWoken = pdFALSE;
    xSemaphoreGiveFromISR(xSem, &xWoken);
    portYIELD_FROM_ISR(xWoken);        /* ← 이 줄 */
}
```

## 실습 3 — 세마포어는 세지 않는다

하드웨어가 3발씩 연사(1ms 간격)하고, 태스크는 한 건 처리에 50ms가 걸린다.

### 실행 결과

```
[  1603 ms][Handler  ] ISR #1 처리  | 응답 지연 25 us
[  1653 ms][Handler  ] ISR #3 처리  | 응답 지연 50338 us
[  1847 ms][Handler  ] ISR #4 처리  | 응답 지연 31 us
[  1897 ms][Handler  ] ISR #6 처리  | 응답 지연 50574 us
...
  ISR 12 회 -> 태스크 8 회 처리 (유실 4 회)
```

### 읽는 법

**① 3발 중 2발만 처리된다.** 매 연사마다 1발씩 사라진다.

```
ISR #1 발생 → 세마포어 [비어있음 → 있음]  → 태스크 깨어남
ISR #2 발생 → 세마포어 [있음 → 있음]      → 무시됨!
ISR #3 발생 → 세마포어 [있음 → 있음]      → 무시됨... 은 아니고
```

정확히는 이렇다. #1로 깨어난 태스크가 세마포어를 가져가 비운 뒤 50ms짜리
처리에 들어간다. 그 사이 #2가 들어와 다시 채운다. #3이 들어오지만
**이미 차 있어서 버려진다.** 태스크가 50ms 뒤 돌아와 #2가 남긴 신호를
받는데, 로그에는 그 시점의 `ulIsrCount`인 `#3`으로 찍힌다.

**바이너리 세마포어는 상태가 "있음 / 없음" 두 가지뿐이다.**
이미 있는 상태에서 또 주면 그냥 사라진다.

**② 두 번째 처리의 지연이 50ms다.** 태스크가 앞 건을 처리하느라 바빠서
신호가 세마포어에 머물러 있던 시간이다. 지연 자체는 정상이다 —
**유실이 문제다.**

### 언제 문제가 되는가

| 용도 | 바이너리 세마포어로 충분한가 |
|---|---|
| "버튼이 눌렸다" | ✅ 몇 번 눌렸는지보다 눌렸다는 사실이 중요 |
| "DMA 전송이 끝났다" | ✅ 완료 신호는 하나면 됨 |
| "센서 값이 도착했다" | ❌ **값을 잃는다.** 큐를 써야 한다 |
| "펄스를 세야 한다" | ❌ 카운팅 세마포어가 필요 |

데이터를 잃으면 안 되면 **큐**(3강), 개수를 세야 하면 **카운팅 세마포어**(5강)다.

## 세마포어 API

| 함수 | 설명 |
|---|---|
| `xSemaphoreCreateBinary()` | 생성. **비어 있는 상태로 시작**한다 |
| `xSemaphoreTake(sem, timeout)` | 신호를 가져감. 없으면 블로킹 |
| `xSemaphoreGive(sem)` | 신호를 줌 (태스크에서) |
| `xSemaphoreGiveFromISR(sem, &woken)` | 신호를 줌 (ISR에서) |
| `vSemaphoreDelete(sem)` | 삭제 |

> **생성 직후 비어 있다**는 점에 주의. 만들자마자 `Take`하면 블로킹된다.
> 옛 매크로 `vSemaphoreCreateBinary()`는 반대로 **채워진 상태**로 만들었다.
> 지금은 폐기됐으니 `xSemaphoreCreateBinary()`를 쓴다.

## 과제

### 과제 1 — 태스크 우선순위를 낮춘다

`PRIO_HANDLER`를 `3`에서 `1`(제어 태스크와 동일)로 바꿔라.

실습 1의 응답 지연이 어떻게 변하는가? yield를 요청해도 지연이 줄지 않는
이유는 무엇인가?

> `xHigherPriorityTaskWoken`은 깨어난 태스크가 **현재 실행 중인 것보다
> 높을 때만** pdTRUE가 된다. 높지 않으면 yield 요청 자체가 일어나지 않는다.

### 과제 2 — 연사 간격을 바꾼다

실습 3의 `ulHwBurst`를 `3` → `10`으로 늘려라. 유실이 얼마나 늘어나는가?

그다음 태스크 처리 시간(`prvRunPhase`의 첫 인자)을 `50` → `5`로 줄이면?

### 과제 3 — 큐로 바꿔 유실을 없앤다

실습 3을 세마포어 대신 **큐**로 고쳐라.

```c
/* ISR에서 */
uint32_t ulSeq = ulIsrCount;
xQueueSendFromISR( xQueue, &ulSeq, &xHigherPriorityTaskWoken );
```

유실이 사라지는가? 큐를 몇 칸으로 만들어야 하는가?
큐도 가득 차면 어떻게 되는가? (3강 실습 3 참고)

### 과제 4 — ISR에서 잘못된 API를 호출해 본다

ISR 안에서 `FromISR`이 아닌 버전을 불러 보라.

```c
xSemaphoreGive( xSem );      /* ❌ ISR에서 금지 */
```

무슨 일이 일어나는가? `configASSERT`가 잡아주는가?

> 잡히면 다행이다. 실제 프로젝트에서 `configASSERT`가 꺼져 있으면
> 이런 실수가 조용히 통과했다가 나중에 시스템을 무너뜨린다.

### 과제 5 — 두 개의 인터럽트 소스 (심화)

인터럽트 번호 3번을 추가로 등록하고, 하드웨어 스레드가 2번과 3번을
번갈아 일으키게 하라. 각각 다른 세마포어와 태스크를 붙인다.

두 태스크의 우선순위를 다르게 주면 어떤 순서로 처리되는가?

## 다음 단계

실습 3에서 본 유실 문제를 **개수를 세는 방식**으로 해결하는 것이
5강의 카운팅 세마포어다.

## 참고

- 바이너리 세마포어 — https://www.freertos.org/xSemaphoreCreateBinary.html
- `xSemaphoreGiveFromISR()` — https://www.freertos.org/a00124.html
- ISR에서의 FreeRTOS 사용 — https://www.freertos.org/RTOS-Cortex-M3-M4.html
