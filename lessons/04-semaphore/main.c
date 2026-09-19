/* =============================================================================
 * 4강: 바이너리 세마포어와 ISR 동기화
 *
 * 이 프로그램이 보여주는 것
 *   실습 1) 지연 인터럽트 처리 - ISR은 신호만 주고, 실제 일은 태스크가 한다
 *   실습 2) ISR에서 yield를 빠뜨리면 응답이 최대 1틱 늦어진다
 *   실습 3) 바이너리 세마포어는 "세지" 않는다 - 빠른 이벤트는 유실된다
 *
 * Windows 시뮬레이터에는 실제 인터럽트가 없지만, 포트가
 * vPortGenerateSimulatedInterruptFromWindowsThread() 를 제공한다.
 * 별도 Windows 스레드를 "하드웨어" 삼아 인터럽트를 일으키므로,
 * FreeRTOS 입장에서는 진짜 비동기 인터럽트와 똑같다.
 * ========================================================================== */

#include <stdlib.h>
#include <windows.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "lab.h"

#define PRIO_CTRL       1
#define PRIO_HANDLER    3       /* 인터럽트를 처리할 태스크 (높게) */
#define STACK_SIZE      ( configMINIMAL_STACK_SIZE * 4 )

/* 애플리케이션이 쓸 수 있는 인터럽트 번호는 2번부터다 (0=Yield, 1=Tick).
 * 32번 미만이어야 한다. */
#define SIM_IRQ_NUM     ( portINTERRUPT_APPLICATION_DEFINED_START )

/* -----------------------------------------------------------------------------
 * 공유 상태
 * -------------------------------------------------------------------------- */
static SemaphoreHandle_t xSem = NULL;

/* ISR이 기록하고 태스크가 읽는다 */
static volatile unsigned long long ullIsrAtUs   = 0;    /* ISR이 신호를 준 시각 */
static volatile unsigned long      ulIsrCount   = 0;    /* ISR이 몇 번 돌았나 */
static volatile unsigned long      ulTaskCount  = 0;    /* 태스크가 몇 번 받았나 */

/* ISR 핸들러가 yield를 요청할지 여부 (실습 2에서 끈다) */
static volatile int iYieldFromIsr = 1;

/* "하드웨어" 스레드 설정 */
static volatile int           iHwEnabled    = 0;
static volatile unsigned long ulHwIntervalMs = 150;
static volatile unsigned long ulHwBurst      = 1;   /* 한 번에 몇 발 쏘나 */

/* -----------------------------------------------------------------------------
 * 인터럽트 서비스 루틴 (ISR)
 *
 * 실제 MCU의 ISR과 똑같이 취급해야 한다. 지켜야 할 규칙:
 *
 *   1) 최대한 짧게. 여기 머무는 동안 다른 인터럽트가 지연된다.
 *   2) printf 같은 무거운 호출 금지. (그래서 여기서는 시각만 기록한다)
 *   3) 블로킹 금지. 기다릴 태스크가 없으므로 애초에 불가능하다.
 *   4) FromISR 접미사가 붙은 API만 호출 가능.
 *
 * 이 핸들러는 "일을 하지 않는다". 세마포어를 주어 태스크를 깨울 뿐이다.
 * 실제 처리는 우선순위 높은 태스크가 맡는다.
 * 이 구조를 지연 인터럽트 처리(deferred interrupt processing)라고 한다.
 * -------------------------------------------------------------------------- */
static uint32_t prvSimulatedIsr( void )
{
    /* ISR에서 태스크를 깨웠을 때, 그 태스크가 지금 실행 중인 것보다
     * 우선순위가 높으면 커널이 이 변수를 pdTRUE로 바꿔 준다. */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    ulIsrCount++;
    ullIsrAtUs = lab_now_us();

    /* 태스크 버전이 아니라 FromISR 버전이다.
     * 타임아웃 인자가 없다 - ISR은 블로킹할 수 없기 때문이다. */
    xSemaphoreGiveFromISR( xSem, &xHigherPriorityTaskWoken );

    /* 이 포트에서는 핸들러의 반환값이 portYIELD_FROM_ISR() 역할을 한다.
     *   0이 아닌 값 = ISR을 나가면서 즉시 컨텍스트 스위칭
     *   0            = 스위칭하지 않음 (다음 틱까지 기다리게 됨)
     *
     * 실제 Cortex-M 코드라면 이렇게 쓴다:
     *     portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
     */
    if( iYieldFromIsr )
    {
        return ( uint32_t ) xHigherPriorityTaskWoken;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
 * "하드웨어" 역할을 하는 Windows 스레드
 *
 * FreeRTOS 태스크가 아니라 순수 Windows 스레드다.
 * 커널과 무관하게 돌면서 인터럽트를 일으키므로, 실제 주변장치처럼
 * 완전히 비동기적이다.
 * -------------------------------------------------------------------------- */
static DWORD WINAPI prvHardwareThread( LPVOID pvParam )
{
    ( void ) pvParam;

    for( ;; )
    {
        Sleep( ( DWORD ) ulHwIntervalMs );

        if( iHwEnabled )
        {
            unsigned long i;

            for( i = 0; i < ulHwBurst; i++ )
            {
                /* Windows 스레드에서 부를 수 있는 전용 버전이다.
                 * (태스크 컨텍스트에서는 vPortGenerateSimulatedInterrupt) */
                vPortGenerateSimulatedInterruptFromWindowsThread( SIM_IRQ_NUM );

                if( ulHwBurst > 1 )
                {
                    Sleep( 1 );     /* 연사 간격 */
                }
            }
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
 * 인터럽트를 처리하는 태스크
 *
 * ISR이 신호를 줄 때까지 Blocked 상태로 기다린다. CPU를 쓰지 않는다.
 * 깨어나면 "무거운 일"을 마음껏 할 수 있다 - ISR이 아니라 태스크이므로
 * 블로킹해도 되고, printf를 써도 되고, 시간이 걸려도 된다.
 * -------------------------------------------------------------------------- */
static void vHandlerTask( void * pvParameters )
{
    unsigned long ulWorkMs = ( unsigned long ) ( size_t ) pvParameters;

    for( ;; )
    {
        /* 신호가 올 때까지 무한 대기 */
        if( xSemaphoreTake( xSem, portMAX_DELAY ) == pdTRUE )
        {
            unsigned long long ullNowUs = lab_now_us();
            unsigned long long ullLatencyUs = ullNowUs - ullIsrAtUs;

            ulTaskCount++;

            LOG( "ISR #%lu 처리  | 응답 지연 %llu us",
                 ulIsrCount, ullLatencyUs );

            /* 실습 3에서 태스크를 일부러 느리게 만든다 */
            if( ulWorkMs > 0 )
            {
                vTaskDelay( pdMS_TO_TICKS( ulWorkMs ) );
            }
        }
    }
}

/* -----------------------------------------------------------------------------
 * 진행 지휘
 * -------------------------------------------------------------------------- */
static void prvResetCounters( void )
{
    ulIsrCount  = 0;
    ulTaskCount = 0;

    /* 이전 실습에서 남은 신호를 버린다.
     * 타임아웃 0이므로 비어 있으면 즉시 pdFALSE로 돌아온다. */
    while( xSemaphoreTake( xSem, 0 ) == pdTRUE )
    {
    }
}

static void prvRunPhase( unsigned long ulWorkMs,
                         unsigned long ulIntervalMs,
                         unsigned long ulBurst,
                         int iYield,
                         unsigned long ulRunMs )
{
    TaskHandle_t xHandler = NULL;

    prvResetCounters();

    iYieldFromIsr  = iYield;
    ulHwIntervalMs = ulIntervalMs;
    ulHwBurst      = ulBurst;

    configASSERT( xTaskCreate( vHandlerTask, "Handler", STACK_SIZE,
                               ( void * ) ( size_t ) ulWorkMs,
                               PRIO_HANDLER, &xHandler ) == pdPASS );

    iHwEnabled = 1;
    vTaskDelay( pdMS_TO_TICKS( ulRunMs ) );
    iHwEnabled = 0;

    /* 하드웨어 스레드가 마지막 발사를 끝낼 시간을 준다 */
    vTaskDelay( pdMS_TO_TICKS( 50 ) );

    vTaskDelete( xHandler );
}

static void vControlTask( void * pvParameters )
{
    lab_printf( "\n=== 실습 1: 지연 인터럽트 처리 ===\n" );
    lab_printf( "  하드웨어가 150ms마다 인터럽트를 건다.\n" );
    lab_printf( "  ISR은 세마포어만 주고 끝. 실제 처리는 태스크가 한다.\n\n" );

    prvRunPhase( 0, 150, 1, 1, 700 );

    lab_printf( "\n  ISR %lu 회 -> 태스크 %lu 회 처리\n", ulIsrCount, ulTaskCount );
    lab_printf( "  -> 응답 지연이 수십~수백 us 수준이다.\n" );
    lab_printf( "     ISR이 yield를 요청해 즉시 컨텍스트 스위칭이 일어났기 때문이다.\n" );

    lab_printf( "\n=== 실습 2: ISR에서 yield를 빠뜨리면 ===\n" );
    lab_printf( "  핸들러가 0을 반환하도록 바꾼다 (portYIELD_FROM_ISR 누락과 동일).\n\n" );

    prvRunPhase( 0, 150, 1, 0, 700 );

    lab_printf( "\n  ISR %lu 회 -> 태스크 %lu 회 처리\n", ulIsrCount, ulTaskCount );
    lab_printf( "  -> 지연이 크게 늘었다. 깨어난 태스크가 즉시 실행되지 못하고\n" );
    lab_printf( "     다음 틱 인터럽트(최대 1000us)까지 기다렸기 때문이다.\n" );

    lab_printf( "\n=== 실습 3: 바이너리 세마포어는 세지 않는다 ===\n" );
    lab_printf( "  하드웨어가 3발씩 연사한다. 태스크는 한 건에 50ms가 걸린다.\n\n" );

    prvRunPhase( 50, 250, 3, 1, 900 );

    lab_printf( "\n  ISR %lu 회 -> 태스크 %lu 회 처리 (유실 %lu 회)\n",
                ulIsrCount, ulTaskCount,
                ( ulIsrCount > ulTaskCount ) ? ( ulIsrCount - ulTaskCount ) : 0 );
    lab_printf( "  -> 바이너리 세마포어는 상태가 '있음/없음' 두 가지뿐이다.\n" );
    lab_printf( "     이미 신호가 있는 상태에서 또 주면 그냥 무시된다.\n" );
    lab_printf( "     이벤트 개수를 세야 한다면 카운팅 세마포어나 큐가 필요하다 (5강).\n" );

    lab_printf( "\n실습 종료.\n" );
    exit( 0 );
}

/* ========================================================================== */
int main( void )
{
    lab_printf( "===============================================\n" );
    lab_printf( " 4강: 바이너리 세마포어와 ISR 동기화\n" );
    lab_printf( "===============================================\n" );

    /* 바이너리 세마포어: 신호 1개를 담는 그릇.
     * 만들어진 직후에는 "비어 있는" 상태라 바로 Take하면 블로킹된다. */
    xSem = xSemaphoreCreateBinary();
    configASSERT( xSem != NULL );

    /* 시뮬레이션 인터럽트 번호에 핸들러를 등록한다.
     * 실제 MCU라면 벡터 테이블에 ISR 주소를 넣는 것에 해당한다. */
    vPortSetInterruptHandler( SIM_IRQ_NUM, prvSimulatedIsr );

    /* "하드웨어" 스레드 기동 */
    configASSERT( CreateThread( NULL, 0, prvHardwareThread, NULL, 0, NULL ) != NULL );

    configASSERT( xTaskCreate( vControlTask, "Ctrl", STACK_SIZE, NULL,
                               PRIO_CTRL, NULL ) == pdPASS );

    vTaskStartScheduler();

    lab_printf( "스케줄러 시작 실패!\n" );
    for( ;; );
    return 0;
}
