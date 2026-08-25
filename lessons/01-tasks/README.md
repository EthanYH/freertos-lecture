# 1강: 태스크와 우선순위

## 실행

```bash
cd lessons/01-tasks
make run
```

무한 루프이므로 `Ctrl+C`로 종료한다.

## 배우는 것

- `xTaskCreate()`로 태스크를 만드는 법
- 우선순위가 높은 태스크가 낮은 태스크를 **선점(preemption)**하는 모습
- `vTaskDelete()`로 태스크를 안전하게 끝내는 법
- 태스크가 `vTaskDelay()`로 **Blocked** 상태에 들어가면 CPU를 내놓는다는 것

## 코드 구성

세 개의 태스크가 서로 다른 우선순위로 돈다.

| 태스크 | 우선순위 | 주기 | 동작 |
|--------|---------|------|------|
| `Low`  | 1 | 200ms | 계속 반복 |
| `Mid`  | 2 | 500ms | 계속 반복 |
| `High` | 3 | 300ms | 5번 실행 후 `vTaskDelete(NULL)` |

## 예상 출력

```
[     0 ms][main     ] 태스크를 생성한다 (아직 스케줄러는 안 돌고 있음)
[     0 ms][main     ] 생성 완료. 남은 힙: 123808 바이트
[     0 ms][main     ] 스케줄러를 시작한다.

[     2 ms][High     ] 높은 우선순위! 다른 태스크를 모두 밀어낸다 (1/5)
[     2 ms][Mid      ] Mid태스크 가 깨어나서 낮은 태스크를 선점했다
[     2 ms][Low      ] 낮은 우선순위 작업 중... (1)
[   202 ms][Low      ] 낮은 우선순위 작업 중... (2)
[   302 ms][High     ] 높은 우선순위! 다른 태스크를 모두 밀어낸다 (2/5)
...
[  1202 ms][High     ] 높은 우선순위! 다른 태스크를 모두 밀어낸다 (5/5)
[  1502 ms][High     ] 할 일이 끝났다. 스스로 종료한다.
```

이후에는 `Low`와 `Mid`만 남아서 계속 돈다.

## 출력에서 확인할 것

**① 0ms 시점의 출력 순서가 High → Mid → Low 다.**

세 태스크 모두 스케줄러 시작과 동시에 Ready 상태가 되지만, 커널은
"Ready 중 우선순위가 가장 높은 것"을 고르므로 High가 먼저 실행된다.
High가 `vTaskDelay()`로 Blocked에 빠지는 순간 Mid가, Mid가 빠지면
Low가 실행된다. **이것이 우선순위 기반 스케줄링의 전부다.**

**② 시각이 202ms, 302ms처럼 2ms씩 밀려 있다.**

스케줄러가 시작되고 Windows 스레드가 준비되는 데 걸린 초기 지연이
그대로 유지된 것이다. `vTaskDelay()`는 "호출 시점부터 N ms"이므로
초기 오차가 계속 따라다닌다. 이 문제는 2강에서 `xTaskDelayUntil()`로
해결한다.

**③ 1502ms 이후 High가 사라진다.**

`vTaskDelete(NULL)`로 자기 자신을 삭제했다. 이때 TCB와 스택 메모리는
즉시 반환되지 않고, **Idle 태스크가 나중에 회수**한다. 그래서 Idle이
굶으면 메모리가 새는 것이다.

## 과제

### 과제 1 — 우선순위를 뒤집어 본다

`PRIO_LOW`를 3, `PRIO_HIGH`를 1로 바꾸면 출력이 어떻게 달라질까?
**먼저 예측한 뒤** 실행해서 확인하라.

> 힌트: 태스크 이름은 그대로 "Low"인데 우선순위만 바뀐다는 점에 주의.

### 과제 2 — 블로킹을 없애면 무슨 일이 일어나는가

`vHighTask()`의 `vTaskDelay()`를 지우고, 대신 `lab_burn_ms(300)`으로
바꿔라. (`lab_burn_ms`는 CPU를 실제로 태우는 함수로, 블로킹하지 않는다)

```c
/* vTaskDelay( pdMS_TO_TICKS( 300 ) ); */
lab_burn_ms( 300 );
```

Low와 Mid의 출력이 어떻게 되는가? 그리고 High가 5번을 다 채우고
`vTaskDelete()`한 뒤에는 어떻게 되는가?

> 이것이 **기아(starvation)** 현상이다. High가 종료되기 전까지
> 다른 태스크는 단 한 줄도 출력하지 못한다.

### 과제 3 — 시뮬레이터의 한계를 확인한다

`STACK_SIZE`를 `configMINIMAL_STACK_SIZE * 4`에서 `configMINIMAL_STACK_SIZE`로
줄여 보라. 실제 MCU라면 `printf`가 스택을 다 써서 오버플로우가 나고
`vApplicationStackOverflowHook()`이 잡아야 한다.

**하지만 이 환경에서는 아무 일도 일어나지 않는다.** 왜 그럴까?

`kernel/portable/MSVC-MingW/port.c`의 `pxPortInitialiseStack()` 주석을 읽어 보라.

> In this simulated case a stack is not initialised, but instead a thread
> is created that will execute the task being created. ... the stack buffer
> is still used, just not in the conventional way.

Windows 포트는 FreeRTOS가 할당한 스택 버퍼를 실제 실행에 쓰지 않는다.
각 태스크는 `CreateThread()`로 만든 **Windows 스레드의 스택**에서 돌고,
FreeRTOS가 준 버퍼는 포트 내부 구조체(`ThreadState_t`)를 담는 용도로만 쓴다.

그래서 `configCHECK_FOR_STACK_OVERFLOW`가 검사하는 버퍼는 애초에
태스크가 건드리지 않는 영역이고, 오버플로우는 영원히 감지되지 않는다.
`uxTaskGetStackHighWaterMark()`도 같은 이유로 의미 없는 값을 준다.

```c
/* 태스크 안에서 찍어 보라. 값이 거의 변하지 않는다. */
LOG( "스택 여유: %lu 워드", ( unsigned long ) uxTaskGetStackHighWaterMark( NULL ) );
```

**교훈:** 시뮬레이터는 API 의미론(semantics)은 정확히 재현하지만
하드웨어 자원 모델은 재현하지 않는다. 스택 크기 산정과 오버플로우 검출은
**실제 타깃에서만 검증할 수 있다.** 어떤 것이 시뮬레이션되고 어떤 것이
안 되는지 아는 것이, 시뮬레이터를 쓰는 사람의 기본기다.

### 과제 4 — 태스크를 계속 만들어 힙을 고갈시킨다

`main()`에서 반복문으로 태스크를 100개 만들어 보라.
`xTaskCreate()`의 반환값이 언제 `pdFAIL`이 되는가?
`xPortGetFreeHeapSize()`를 함께 찍어서 확인하라.

## 이 환경에서 재현되지 않는 것

| 항목 | 이유 |
|------|------|
| 스택 오버플로우 검출 | 태스크가 Windows 스레드 스택에서 실행됨 (과제 3 참고) |
| `uxTaskGetStackHighWaterMark()` | 같은 이유 |
| 정확한 실시간 타이밍 | Windows 스케줄러 위에 얹혀 있어 ms 단위 지터가 있음 |

반대로 **정확히 재현되는 것**: 우선순위 스케줄링 순서, 선점, 블로킹/기아,
큐·세마포어·뮤텍스의 모든 의미론, 힙 사용량과 할당 실패.

## 참고

- `xTaskCreate()` — https://www.freertos.org/a00125.html
- `vTaskDelete()` — https://www.freertos.org/a00126.html
