# Task API 한글 레퍼런스

이 문서는 이 저장소에 포함된 커널(`kernel/include/task.h`, **V11.1.0+**)의
헤더를 직접 읽고 정리한 것이다. 공식 *FreeRTOS Reference Manual V10.0.0*의
번역이 아니며, 그 문서와 다른 점은 아래 "V10.0.0 매뉴얼과 달라진 것"에
따로 적어 두었다.

시그니처는 헤더 원문 그대로다. 설명·주의사항·예제는 이 저장소의
`common/FreeRTOSConfig.h` 설정(`configMAX_PRIORITIES = 7`,
`configUSE_PREEMPTION = 1`, `configUSE_TIME_SLICING = 1`,
`configSUPPORT_STATIC_ALLOCATION = 0`)을 기준으로 쓴 것이다.

---

## 0. 먼저 알아야 할 타입과 규칙

| 타입 | 의미 |
|------|------|
| `TaskHandle_t` | 태스크를 지목하는 불투명 핸들. 대부분의 API에서 `NULL`은 **"호출한 태스크 자신"** 을 뜻한다. |
| `TickType_t` | 틱 카운트. `configTICK_RATE_HZ`가 단위를 정한다. ms → 틱 변환은 `pdMS_TO_TICKS(ms)`. |
| `BaseType_t` | 아키텍처의 자연 워드 크기 정수. 성공/실패 반환값에 쓴다(`pdPASS`/`pdFAIL`, `pdTRUE`/`pdFALSE`). |
| `UBaseType_t` | 위의 부호 없는 버전. 우선순위·개수에 쓴다. |
| `TaskFunction_t` | `void (*)( void * )`. 태스크 본체의 형태. |

**이름 앞글자 규칙** (FreeRTOS 전체 공통, MISRA 스타일 헝가리안):

- `v` = `void` 반환, `x` = `BaseType_t`/구조체 반환, `ux` = `UBaseType_t` 반환,
  `e` = enum 반환, `pc` = `char *` 반환
- 인자의 `px` = 포인터, `pv` = `void *`, `pc` = `char *`, `uc` = `uint8_t`

**두 가지 절대 규칙**

1. **태스크 함수는 `return`하지 않는다.** 끝내야 하면 반드시 `vTaskDelete(NULL)`.
   그냥 `return`하면 동작이 정의되지 않는다(`configASSERT`가 잡거나 폭주한다).
2. **`FromISR` 접미사가 붙은 함수만 인터럽트에서 호출할 수 있다.** 섞으면
   커널 자료구조가 깨진다.

---

## 1. 태스크 생성과 삭제

### xTaskCreate

```c
BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                        const char * const pcName,
                        const configSTACK_DEPTH_TYPE uxStackDepth,
                        void * const pvParameters,
                        UBaseType_t uxPriority,
                        TaskHandle_t * const pxCreatedTask );
```

태스크를 만들어 **Ready 상태**로 넣는다. 스케줄러가 이미 돌고 있고 새 태스크의
우선순위가 현재 태스크보다 높으면, **이 함수가 반환하기 전에 즉시 선점**이
일어난다.

| 인자 | 설명 |
|------|------|
| `pxTaskCode` | 태스크 본체 함수 |
| `pcName` | 디버깅용 이름. 동작에 영향 없음. `configMAX_TASK_NAME_LEN`(널 포함)을 넘으면 잘린다 |
| `uxStackDepth` | **워드 단위**(바이트 아님). Cortex-M이면 `100`은 400바이트다 |
| `pvParameters` | 태스크 함수의 `pvParameters`로 그대로 전달 |
| `uxPriority` | `0 ~ configMAX_PRIORITIES-1`. **숫자가 클수록 높다.** 0은 Idle과 같은 등급 |
| `pxCreatedTask` | 생성된 핸들을 받을 곳. 필요 없으면 `NULL` |

**반환값**: `pdPASS` 또는 `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY`
(= `pdFAIL`과 같은 값). 힙 부족이 유일한 실패 원인이므로 **반드시 확인하라.**

TCB와 스택이 모두 힙(`configTOTAL_HEAP_SIZE`)에서 할당된다.
남은 힙은 `xPortGetFreeHeapSize()`로 볼 수 있다.

> **함정 1 — `pvParameters`의 수명.** 지역 변수 주소를 넘기고 그 함수가
> 반환해 버리면 태스크는 죽은 스택을 읽는다. 정적/전역/힙 객체를 넘겨라.
> 문자열 리터럴은 안전하다.
>
> **함정 2 — 스택 단위.** `configMINIMAL_STACK_SIZE`를 기준 배수로 쓰는 게
> 안전하다. `printf` 계열은 스택을 크게 먹는다.
>
> **함정 3 — 스케줄러 시작 전 생성.** `vTaskStartScheduler()` 전에 만들면
> 선점이 일어나지 않고 그냥 Ready 큐에 쌓인다. `lessons/01-tasks/main.c`가
> 이 형태다.

### vTaskDelete

```c
void vTaskDelete( TaskHandle_t xTaskToDelete );   /* INCLUDE_vTaskDelete == 1 */
```

`NULL`이면 자기 자신을 삭제한다. 자기 자신을 지운 경우 **이 호출은 반환하지
않는다.**

**메모리 회수 시점이 중요하다.** 커널이 즉시 처리하는 것은 스케줄러 자료구조에서
빼는 것뿐이고, **스택과 TCB의 실제 해제는 Idle 태스크가 한다.** 따라서:

- Idle 태스크(우선순위 0)가 실행될 틈이 없으면 **메모리가 회수되지 않는다.**
  모든 태스크가 항상 Ready라면 삭제한 만큼 힙이 계속 줄어든다.
- 태스크가 직접 잡고 있던 자원(뮤텍스, 큐, `pvPortMalloc` 한 버퍼, 파일 핸들)은
  커널이 전혀 모른다. **삭제 전에 스스로 정리해야 한다.**
- 뮤텍스를 쥔 채로 삭제하면 그 뮤텍스는 영원히 잠긴다.

> 실무에서는 `vTaskDelete`를 쓰지 않고, 태스크가 큐나 노티피케이션을 기다리며
> Blocked로 잠들어 있게 하는 설계를 더 선호한다. 삭제·재생성은 힙 단편화의
> 주범이다.

### xTaskCreateStatic

```c
TaskHandle_t xTaskCreateStatic( TaskFunction_t pxTaskCode,
                                const char * const pcName,
                                const configSTACK_DEPTH_TYPE uxStackDepth,
                                void * const pvParameters,
                                UBaseType_t uxPriority,
                                StackType_t * const puxStackBuffer,
                                StaticTask_t * const pxTaskBuffer );
```

힙을 전혀 쓰지 않고, 호출자가 준 정적 버퍼에 태스크를 만든다. 실패하면
`NULL`을 반환한다(버퍼 중 하나라도 `NULL`인 경우).
`configSUPPORT_STATIC_ALLOCATION`이 1이어야 하는데 **이 저장소에서는 0이라
현재 쓸 수 없다.** 인증(IEC 61508 등)이 필요한 제품에서 동적 할당을 아예
금지할 때 쓰는 방식이다.

---

## 2. 지연 — Blocked 상태로 들어가기

지연 API의 핵심은 **CPU를 내놓는다**는 것이다. 바쁜 대기(busy-wait)는
자기보다 낮은 우선순위를 전부 굶긴다.

### vTaskDelay

```c
void vTaskDelay( const TickType_t xTicksToDelay );   /* INCLUDE_vTaskDelay == 1 */
```

**"이 호출 시점부터" `xTicksToDelay` 틱 동안** Blocked. 상대 지연이다.

```c
vTaskDelay( pdMS_TO_TICKS( 200 ) );
```

`0`을 넘기면 Blocked에 들어가지 않고, 같은 우선순위의 다른 태스크에게
차례만 넘긴다(`taskYIELD()`와 유사).

> **주기적 실행에 쓰면 안 된다.** 실제 주기는
> `(작업 시간) + (지연 시간) + (선점당한 시간)` 이 되어 계속 밀린다.
> 1강 출력이 `202ms`, `302ms`처럼 어긋나 있는 이유가 이것이다.

### xTaskDelayUntil

```c
BaseType_t xTaskDelayUntil( TickType_t * const pxPreviousWakeTime,
                            const TickType_t xTimeIncrement );
/* INCLUDE_xTaskDelayUntil == 1 */
```

**절대 시각 기준** 지연. `*pxPreviousWakeTime + xTimeIncrement` 시점까지 잠들고,
깨어날 때 커널이 `*pxPreviousWakeTime`을 그 시각으로 **자동 갱신**한다.
주기가 드리프트하지 않는다.

```c
TickType_t xLastWake = xTaskGetTickCount();   /* 루프 밖에서 한 번만 초기화 */
const TickType_t xPeriod = pdMS_TO_TICKS( 100 );

for( ;; )
{
    /* ... 100ms 주기 작업 ... */
    xTaskDelayUntil( &xLastWake, xPeriod );
}
```

**반환값**: 실제로 Blocked 되었으면 `pdTRUE`, 아니면 `pdFALSE`. `pdFALSE`는
**마감을 놓쳤다(deadline miss)** 는 뜻이다 — 목표 시각이 이미 지나가서 잘
필요가 없었던 것이다. 실시간 시스템에서는 이 반환값을 오버런 검출에 쓴다.

> **함정** — `xLastWake`를 루프 **안**에서 초기화하면 `vTaskDelay`와 똑같아진다.
> 반드시 루프 밖에서 한 번만.
>
> 이 이름은 V10.4.0에서 바뀐 것이다. 아래 호환성 절 참고.

### xTaskAbortDelay

```c
BaseType_t xTaskAbortDelay( TaskHandle_t xTask );  /* INCLUDE_xTaskAbortDelay == 1 */
```

Blocked 상태인 태스크를 **타임아웃을 기다리지 않고 즉시 Ready로 끌어낸다.**
지연이든, 큐/세마포어 대기든 상관없다. 깨어난 태스크 입장에서는 마치
타임아웃이 난 것처럼 보인다(큐 수신이라면 `pdFALSE`를 반환).
대상이 Blocked가 아니었으면 `pdFAIL`.

### taskYIELD

```c
taskYIELD();     /* 매크로 */
```

**같은 우선순위**의 다음 Ready 태스크에게 차례를 넘긴다. 더 높은 우선순위
태스크가 있었다면 이미 실행 중이었을 테니, 사실상 라운드로빈을 앞당기는
용도다. `configUSE_TIME_SLICING = 1`이면 틱마다 자동으로 일어나므로 굳이
부를 일은 많지 않다.

---

## 3. 우선순위

### uxTaskPriorityGet / vTaskPrioritySet

```c
UBaseType_t uxTaskPriorityGet( const TaskHandle_t xTask );
void        vTaskPrioritySet( TaskHandle_t xTask, UBaseType_t uxNewPriority );
/* INCLUDE_uxTaskPriorityGet, INCLUDE_vTaskPrioritySet */
```

`xTask`가 `NULL`이면 자기 자신. `uxNewPriority`가 `configMAX_PRIORITIES-1`을
넘으면 커널이 최대값으로 깎는다(`configASSERT`가 켜져 있으면 먼저 잡힌다).

우선순위를 바꾸는 즉시 스케줄링이 재평가된다.
- 자기 우선순위를 **낮췄는데** 더 높은 Ready 태스크가 있으면 → 즉시 선점당함
- 남의 우선순위를 **올려서** 자기보다 높아지면 → 즉시 선점당함

### uxTaskBasePriorityGet

```c
UBaseType_t uxTaskBasePriorityGet( const TaskHandle_t xTask );
UBaseType_t uxTaskBasePriorityGetFromISR( const TaskHandle_t xTask );
```

**우선순위 상속으로 일시적으로 올라간 값이 아니라, 원래 설정된 우선순위**를
돌려준다. `uxTaskPriorityGet`은 상속 중이면 올라간 값(유효 우선순위)을 준다.
뮤텍스 우선순위 상속을 디버깅할 때 두 값을 비교하면 상속이 실제로 걸렸는지
바로 보인다.

> V10.0.0 매뉴얼에는 없는 함수다(V11에서 추가).

### uxTaskPriorityGetFromISR

```c
UBaseType_t uxTaskPriorityGetFromISR( const TaskHandle_t xTask );
```

ISR에서 우선순위를 읽을 때. ISR에는 "자기 자신"이 없으므로 `NULL`을 넘기면 안 된다.

---

## 4. 일시 정지와 재개

### vTaskSuspend / vTaskResume

```c
void vTaskSuspend( TaskHandle_t xTaskToSuspend );  /* INCLUDE_vTaskSuspend == 1 */
void vTaskResume ( TaskHandle_t xTaskToResume  );
```

Suspended 상태의 태스크는 **어떤 이유로도 스케줄되지 않는다** — 타임아웃도,
큐 데이터 도착도 깨우지 못한다. 오직 `vTaskResume`(또는 `xTaskResumeFromISR`)
만이 꺼낼 수 있다.

`NULL`이면 자기 자신을 정지시킨다. 이 경우 다른 태스크가 깨워 줄 때까지
그 줄에서 멈춰 있는다.

> **함정 — 카운팅이 아니다.** `vTaskSuspend`를 3번 불러도 `vTaskResume` 한 번이면
> 깨어난다. 중첩 카운트가 없다.
>
> **함정 — 임계 구역.** Blocked 중인 태스크를 정지시켰다가 재개하면 원래
> 기다리던 이벤트 대기 상태로 정확히 복귀하므로 그건 안전하다. 위험한 건
> 태스크가 뮤텍스나 임계 구역을 쥔 상태에서 정지시키는 경우다 — 그대로 교착이다.
> 남을 임의로 정지시키는 설계는 피하고, 스스로 블로킹하게 만들어라.

### xTaskResumeFromISR

```c
BaseType_t xTaskResumeFromISR( TaskHandle_t xTaskToResume );
```

**반환값이 `pdTRUE`면 깨운 태스크가 현재 태스크보다 우선순위가 높다는 뜻**이므로,
ISR을 나가기 전에 컨텍스트 스위치를 요청해야 한다.

```c
void vAnISR( void )
{
    BaseType_t xHigherPriorityTaskWoken = xTaskResumeFromISR( xHandle );
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}
```

> 이 함수보다 **태스크 노티피케이션(`vTaskNotifyGiveFromISR`)이나 세마포어를
> 쓰는 쪽이 거의 항상 낫다.** `xTaskResumeFromISR`은 대상이 아직 정지되기
> 전이면 신호가 그냥 사라진다(정지 요청과 재개가 경합).

---

## 5. 스케줄러 제어

### vTaskStartScheduler

```c
void vTaskStartScheduler( void );
```

Idle 태스크(그리고 `configUSE_TIMERS`가 1이면 타이머 서비스 태스크)를 만들고
스케줄링을 시작한다. **정상적인 경우 절대 반환하지 않는다.**

반환했다면 원인은 둘 중 하나다:
1. Idle/타이머 태스크를 만들 힙이 부족했다 → `configTOTAL_HEAP_SIZE`를 늘려라
2. 누군가 `vTaskEndScheduler()`를 불렀다

### vTaskEndScheduler

```c
void vTaskEndScheduler( void );
```

스케줄러를 멈추고 `vTaskStartScheduler()` 다음 줄로 돌아간다.
**포트에서 지원해야만 동작하며, 대부분의 임베디드 포트는 지원하지 않는다.**
태스크·큐 등이 쓰던 메모리는 자동으로 회수되지 않는다. 사실상
호스트 시뮬레이터 전용이라고 보면 된다.

### vTaskSuspendAll / xTaskResumeAll

```c
void       vTaskSuspendAll( void );
BaseType_t xTaskResumeAll ( void );
```

**스케줄러 자체를 잠근다.** 컨텍스트 스위치는 막히지만 **인터럽트는 그대로
동작한다.** 이것이 임계 구역(`taskENTER_CRITICAL`)과의 결정적 차이다.

- 중첩 호출을 카운트하므로 `vTaskSuspendAll` 횟수만큼 `xTaskResumeAll`을
  불러야 한다.
- 잠긴 동안 발생한 틱은 누적되었다가 `xTaskResumeAll`에서 한꺼번에 처리된다.
- 잠긴 동안 **블로킹 API를 호출하면 안 된다.** 깨워 줄 스케줄러가 없다.
- `xTaskResumeAll`이 `pdTRUE`를 반환하면 그 안에서 이미 컨텍스트 스위치가
  일어났다는 뜻이다.

```c
vTaskSuspendAll();
{
    /* 여러 전역 변수를 태스크들에 대해 원자적으로 갱신.
       ISR은 계속 도니까 ISR과 공유하는 데이터에는 이걸로 부족하다. */
}
( void ) xTaskResumeAll();
```

### xTaskGetSchedulerState

```c
BaseType_t xTaskGetSchedulerState( void );  /* INCLUDE_xTaskGetSchedulerState == 1 */
```

`taskSCHEDULER_NOT_STARTED` / `taskSCHEDULER_RUNNING` / `taskSCHEDULER_SUSPENDED`
중 하나. 드라이버 초기화 코드처럼 "스케줄러 시작 전후 모두에서 불릴 수 있는
함수"가 블로킹해도 되는지 판단할 때 쓴다.

### taskENTER_CRITICAL / taskEXIT_CRITICAL

```c
taskENTER_CRITICAL();
taskEXIT_CRITICAL();
taskENTER_CRITICAL_FROM_ISR();   /* UBaseType_t를 반환 */
taskEXIT_CRITICAL_FROM_ISR( uxSavedInterruptStatus );
```

`configMAX_SYSCALL_INTERRUPT_PRIORITY` 이하 우선순위의 인터럽트를 막는다.
중첩 카운트가 있다. **매우 짧게 유지하라** — 여기 머무는 시간이 그대로
시스템의 인터럽트 지연(latency)이 된다. 임계 구역 안에서 블로킹 API를
부르는 것은 금지다.

---

## 6. 상태 조회와 디버깅

### xTaskGetTickCount

```c
TickType_t xTaskGetTickCount( void );
TickType_t xTaskGetTickCountFromISR( void );
```

부팅 이후 경과 틱. **오버플로한다** — `configUSE_16_BIT_TICKS`가 0이면 32비트,
1000Hz에서 약 49.7일이면 한 바퀴 돈다. 경과 시간 비교는 반드시 뺄셈으로:

```c
if( ( xTaskGetTickCount() - xStart ) > xTimeout )   /* OK: 부호 없는 뺄셈은 랩어라운드에 안전 */
```

절대값 비교(`xNow > xDeadline`)는 오버플로 시점에 틀린다.
타임아웃 관리가 필요하면 `vTaskSetTimeOutState()` / `xTaskCheckForTimeOut()`을 쓰라.

### eTaskGetState

```c
eTaskState eTaskGetState( TaskHandle_t xTask );  /* INCLUDE_eTaskGetState == 1 */
```

| 값 | 의미 |
|----|------|
| `eRunning` | 지금 실행 중 (질의한 대상이 자기 자신인 경우) |
| `eReady` | 실행 가능, 차례 대기 중 |
| `eBlocked` | 지연/이벤트 대기 중 (타임아웃 있음) |
| `eSuspended` | `vTaskSuspend` 되었거나, 타임아웃 없이 무한 대기 중 |
| `eDeleted` | 삭제되었으나 아직 Idle이 메모리를 회수하지 않음 |
| `eInvalid` | 핸들이 유효하지 않음 |

> `eSuspended`가 두 가지를 뜻한다는 점에 주의. `portMAX_DELAY`로 큐를 무한
> 대기 중인 태스크도 `eSuspended`로 나온다(`INCLUDE_vTaskSuspend`가 1일 때).

### 이름과 핸들

```c
char *       pcTaskGetName( TaskHandle_t xTaskToQuery );        /* NULL = 자신 */
TaskHandle_t xTaskGetHandle( const char * pcNameToQuery );      /* INCLUDE_xTaskGetHandle */
TaskHandle_t xTaskGetCurrentTaskHandle( void );                 /* INCLUDE_xTaskGetCurrentTaskHandle */
TaskHandle_t xTaskGetIdleTaskHandle( void );                    /* INCLUDE_xTaskGetIdleTaskHandle */
UBaseType_t  uxTaskGetNumberOfTasks( void );
```

`xTaskGetHandle`은 태스크 목록 전체를 선형 탐색하므로 **느리다.** 초기화 때
한 번 찾아 두고 재사용하라. 같은 이름이 여럿이면 어느 것이 나올지 보장되지 않는다.

### uxTaskGetStackHighWaterMark

```c
UBaseType_t uxTaskGetStackHighWaterMark( TaskHandle_t xTask );
/* INCLUDE_uxTaskGetStackHighWaterMark == 1 */
```

그 태스크가 지금까지 남긴 **스택 여유의 최소값**(워드 단위)을 준다. 0에
가까우면 오버플로 직전이다. 스택 크기를 정할 때 실측용으로 쓴다.

> **이 저장소의 Windows 시뮬레이터에서는 의미 없는 값을 준다.** 태스크가
> FreeRTOS 스택 버퍼가 아니라 Windows 스레드 스택에서 실행되기 때문이다.
> `lessons/01-tasks/README.md`의 과제 3에 자세히 적어 두었다.

### vTaskGetInfo / uxTaskGetSystemState / vTaskListTasks

```c
void        vTaskGetInfo( TaskHandle_t xTask, TaskStatus_t * pxTaskStatus,
                          BaseType_t xGetFreeStackSpace, eTaskState eState );
UBaseType_t uxTaskGetSystemState( TaskStatus_t * const pxTaskStatusArray,
                                  const UBaseType_t uxArraySize,
                                  configRUN_TIME_COUNTER_TYPE * const pulTotalRunTime );
void        vTaskListTasks( char * pcWriteBuffer, size_t uxBufferLength );
```

`configUSE_TRACE_FACILITY`(그리고 `vTaskListTasks`는
`configUSE_STATS_FORMATTING_FUNCTIONS`도)가 1이어야 한다.
`vTaskListTasks`는 사람이 읽는 표를 문자열로 찍어 주는 **디버깅 전용** 함수이며,
그리는 동안 스케줄러를 정지시키므로 실운영 코드에 두면 안 된다.

`uxTaskGetSystemState`는 배열 크기가 부족하면 `0`을 반환한다.
`uxTaskGetNumberOfTasks()`로 개수를 먼저 확인하라.

> `vTaskListTasks`는 V11에서 `vTaskList`가 개명된 것이다. V10.0.0 매뉴얼에는
> `vTaskList`로 나온다.

---

## 7. 태스크 노티피케이션 (요약)

`configUSE_TASK_NOTIFICATIONS = 1`(이 저장소 기본값)이면 태스크마다
노티피케이션 값이 생긴다. **큐나 세마포어보다 훨씬 빠르고 RAM을 덜 쓰는**
1:1 신호 전달 수단이다.

```c
BaseType_t xTaskNotifyGive( TaskHandle_t xTaskToNotify );
void       vTaskNotifyGiveFromISR( TaskHandle_t xTaskToNotify,
                                   BaseType_t * pxHigherPriorityTaskWoken );
uint32_t   ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait );
BaseType_t xTaskNotify( TaskHandle_t xTaskToNotify, uint32_t ulValue,
                        eNotifyAction eAction );
BaseType_t xTaskNotifyWait( uint32_t ulBitsToClearOnEntry,
                            uint32_t ulBitsToClearOnExit,
                            uint32_t * pulNotificationValue,
                            TickType_t xTicksToWait );
```

한계: **보낼 대상 태스크를 알아야 하고, 여러 태스크가 같은 신호를 기다릴 수
없다.** 그런 경우는 큐·세마포어·이벤트 그룹을 써야 한다.

---

## 8. 우선순위와 스케줄링 규칙 정리

이 저장소 설정(`configUSE_PREEMPTION = 1`, `configUSE_TIME_SLICING = 1`)에서:

1. 커널은 항상 **Ready 상태 중 우선순위가 가장 높은 태스크**를 실행한다.
2. 같은 우선순위가 여럿이면 **틱마다 라운드로빈**으로 돌아간다.
3. 더 높은 우선순위 태스크가 Ready가 되는 즉시 **현재 태스크를 선점**한다.
   ISR 안에서 벌어져도 마찬가지다(`portYIELD_FROM_ISR` 필요).
4. 실행할 것이 없으면 **Idle 태스크(우선순위 0)** 가 돈다. Idle은 삭제된
   태스크의 메모리를 회수하는 일도 하므로 **절대 굶기면 안 된다.**
5. 태스크가 블로킹하지 않으면 자기보다 낮은 모든 태스크를 **기아**시킨다.
   `lessons/01-tasks` 과제 2가 이 현상을 재현한다.

상태 전이:

```
             xTaskCreate()
                   |
                   v
  +------------> Ready <---------------+
  |               | ^                  |
  |   스케줄됨    | | 선점당함         | 이벤트 발생 / 타임아웃
  |               v |                  |
  |            Running ----------------+
  |               | |  vTaskDelay(), 큐/세마포어 대기
  |               | +-----------------> Blocked
  |               |
  | vTaskResume() | vTaskSuspend()
  +----------- Suspended <-------------+ (Ready/Blocked에서도 진입 가능)

                vTaskDelete() --> Deleted --> (Idle이 메모리 회수)
```

---

## 9. 자주 겪는 실수

| 증상 | 원인 |
|------|------|
| `xTaskCreate`가 `pdFAIL` | 힙 부족. `configTOTAL_HEAP_SIZE`를 늘리거나 스택을 줄여라 |
| 태스크가 아예 안 돈다 | 더 높은 우선순위 태스크가 블로킹하지 않고 있다 |
| 주기가 계속 밀린다 | `vTaskDelay` 대신 `xTaskDelayUntil`을 써라 |
| `xTaskDelayUntil`인데도 밀린다 | `pxPreviousWakeTime`을 루프 안에서 초기화했다 |
| 태스크를 삭제했는데 힙이 안 는다 | Idle이 실행될 틈이 없다 |
| 랜덤하게 죽는다 | 스택 부족, 또는 `pvParameters`로 죽은 지역 변수를 넘겼다 |
| ISR에서 깨웠는데 반응이 늦다 | `portYIELD_FROM_ISR()`을 빠뜨렸다 |
| 오랜 시간 뒤 타임아웃이 오동작 | 틱 오버플로. 절대 비교 대신 뺄셈을 써라 |

---

## 10. V10.0.0 매뉴얼과 달라진 것

이 저장소의 커널은 **V11.1.0+** 이므로, V10.0.0 매뉴얼과 아래가 다르다.

| V10.0.0 매뉴얼 | 현재 커널 |
|---------------|----------|
| `vTaskDelayUntil()` (void 반환) | **`xTaskDelayUntil()`** (`BaseType_t` 반환). 옛 이름은 호환 매크로로 남아 있다 |
| `vTaskList()` | **`vTaskListTasks()`** (버퍼 길이 인자 추가) |
| `vTaskGetRunTimeStats()` | **`vTaskGetRunTimeStatistics()`** (버퍼 길이 인자 추가) |
| — | 위 세 함수의 옛 이름은 모두 호환 매크로로 남아 있다. `vTaskList`/`vTaskGetRunTimeStats`는 길이를 `configSTATS_BUFFER_MAX_LENGTH`로 채워 새 함수를 부른다 |
| — | `uxTaskBasePriorityGet()`, `uxTaskCallForEachTask()`, `xTaskGetStaticBuffers()` 추가 |
| — | SMP API 추가: `xTaskCreateAffinitySet()`, `vTaskCoreAffinitySet()`, `vTaskPreemptionDisable()` 등 (`configNUMBER_OF_CORES > 1`일 때) |
| `usStackDepth` (`uint16_t`) | **`uxStackDepth` (`configSTACK_DEPTH_TYPE`)** — 큰 스택을 위해 타입이 확장됨 |

버퍼 길이 인자가 추가된 함수들은 **버퍼 오버런을 막기 위한 변경**이다.
옛 시그니처를 쓰는 예제 코드를 그대로 복사하면 컴파일 오류가 난다.

---

## 참고

- 원문 API 문서: https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-and-scheduler/00-TaskHandle
- 헤더 원문: `kernel/include/task.h`
- 이 저장소의 설정: `common/FreeRTOSConfig.h`
- 실습: `lessons/01-tasks/`
