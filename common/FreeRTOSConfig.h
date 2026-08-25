/*
 * FreeRTOS 설정 파일 (Windows 시뮬레이터 포트 공용)
 *
 * 이 파일은 FreeRTOS 커널의 "컴파일 타임 설정"이다.
 * 커널 소스는 이 파일의 #define 값을 보고 어떤 기능을 포함할지 결정한다.
 * 즉 여기서 0으로 끈 기능은 아예 컴파일되지 않아 코드 크기가 줄어든다.
 *
 * 전체 옵션 목록: https://www.freertos.org/a00110.html
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * 스케줄러 동작
 *----------------------------------------------------------*/

/* 1 = 선점형(preemptive). 더 높은 우선순위 태스크가 Ready가 되는 순간
 *     현재 태스크를 즉시 중단시킨다. 이게 RTOS의 기본 동작이다.
 * 0 = 협조형(cooperative). 태스크가 스스로 양보할 때까지 기다린다. */
#define configUSE_PREEMPTION                    1

/* 1 = 같은 우선순위 태스크끼리 매 틱마다 돌아가며 실행(라운드로빈).
 *     타임 슬라이스는 정확히 1틱이다. */
#define configUSE_TIME_SLICING                  1

/* 틱 주파수. 1000 = 1틱이 1ms.
 * 높일수록 시간 분해능은 좋아지지만 틱 인터럽트 오버헤드가 커진다. */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )

/* 사용 가능한 우선순위 개수. 0 ~ (configMAX_PRIORITIES-1) 범위를 쓴다.
 * 숫자가 클수록 높은 우선순위다. 0번은 Idle 태스크가 쓴다. */
#define configMAX_PRIORITIES                    ( 7 )

/* 태스크 하나의 최소 스택 크기. 단위는 바이트가 아니라 "워드"다.
 * Windows 포트에서는 실제 Windows 스레드 스택을 쓰므로 큰 의미는 없지만,
 * 실제 MCU에서는 이 값이 부족하면 스택 오버플로우가 난다. */
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 70 )

/* pvPortMalloc()이 쓸 힙 전체 크기. 태스크 스택/TCB/큐가 모두 여기서 나온다. */
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 128 * 1024 ) )

/* 태스크 이름의 최대 길이 (널 문자 포함) */
#define configMAX_TASK_NAME_LEN                 ( 12 )

/* 틱 카운터의 비트 수. Windows/32비트 MCU는 0(=32비트)을 쓴다.
 * 32비트 + 1000Hz면 약 49일마다 오버플로우된다 (커널이 알아서 처리함). */
#define configUSE_16_BIT_TICKS                  0

/* Idle 태스크가 자기와 같은 우선순위(0) 태스크에게 양보할지 여부 */
#define configIDLE_SHOULD_YIELD                 1

/*-----------------------------------------------------------
 * 메모리 할당
 *----------------------------------------------------------*/
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

/*-----------------------------------------------------------
 * 오류 검출 (학습 목적상 전부 켠다)
 *----------------------------------------------------------*/

/* 스택 오버플로우 검출.
 * 1 = 컨텍스트 스위칭 시 스택 포인터 위치만 확인 (빠름, 놓칠 수 있음)
 * 2 = 1번 + 스택 끝을 특정 패턴으로 채워두고 훼손 여부 확인 (느리지만 확실)
 * 감지되면 vApplicationStackOverflowHook()이 호출된다. */
#define configCHECK_FOR_STACK_OVERFLOW          2

/* pvPortMalloc() 실패 시 vApplicationMallocFailedHook() 호출 */
#define configUSE_MALLOC_FAILED_HOOK            1

/* 매 틱마다 vApplicationTickHook() 호출 */
#define configUSE_TICK_HOOK                     0

/* Idle 태스크가 돌 때마다 vApplicationIdleHook() 호출 */
#define configUSE_IDLE_HOOK                     0

/* 커널 내부의 가정이 깨지면 여기서 잡는다.
 * 학습 중 API를 잘못 쓰면 조용히 죽는 대신 이 메시지가 뜬다. */
extern void vAssertCalled( const char * pcFile, unsigned long ulLine );
#define configASSERT( x )   if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )

/*-----------------------------------------------------------
 * 커널 기능 on/off
 *----------------------------------------------------------*/
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_QUEUE_SETS                    0
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_CO_ROUTINES                   0
#define configQUEUE_REGISTRY_SIZE               10

/* 소프트웨어 타이머 (7강).
 * 타이머는 별도의 "타이머 서비스 태스크"가 콜백을 실행하는 구조다. */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/*-----------------------------------------------------------
 * 선택적 API 포함 여부
 * 0으로 두면 해당 함수가 링크되지 않아 코드 크기가 줄어든다.
 *----------------------------------------------------------*/
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xQueueGetMutexHolder            1

#endif /* FREERTOS_CONFIG_H */
