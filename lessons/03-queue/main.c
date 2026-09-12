/* =============================================================================
 * 3강: 큐 (Queue)
 *
 * 이 프로그램이 보여주는 것
 *   실습 1) 큐는 값을 "복사"한다 - 보낸 뒤 원본을 바꿔도 받은 값은 그대로다
 *   실습 2) 생산자-소비자. 소비자는 데이터가 올 때까지 CPU를 쓰지 않는다
 *   실습 3) 생산이 소비보다 빠르면 큐가 차고, 데이터가 버려진다
 *
 * 실습 2, 3은 태스크를 만들었다가 끝나면 지우는 방식으로 진행한다.
 * 실습끼리 간섭하지 않게 하려는 것이다.
 * ========================================================================== */

#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "lab.h"

#define PRIO_CTRL       1       /* 진행을 지휘하는 태스크 (가장 낮게) */
#define PRIO_PRODUCER   2
#define PRIO_CONSUMER   3       /* 소비자를 높게 두는 것이 일반적이다 (README 참고) */
#define STACK_SIZE      ( configMINIMAL_STACK_SIZE * 4 )

#define QUEUE_LEN       5

/* 큐로 주고받을 데이터.
 * 구조체를 통째로 보낼 수 있다는 점이 큐의 큰 장점이다.
 * 전역 구조체를 공유하면 "찢어진 읽기"가 생기지만, 큐는 통째로 복사되므로
 * 항상 일관된 상태만 전달된다. */
typedef struct
{
    uint32_t ulSeq;         /* 몇 번째 데이터인가 */
    uint32_t ulSentAt;      /* 보낸 시각 (ms) */
    int16_t  sValue;        /* 측정값 */
} Sample_t;

static QueueHandle_t xQueue        = NULL;
static TaskHandle_t  xProducerTask = NULL;
static TaskHandle_t  xConsumerTask = NULL;

/* 실습 3에서 쓸 통계 */
static unsigned long ulSentCount = 0;
static unsigned long ulDropCount = 0;
static unsigned long ulRecvCount = 0;

/* -----------------------------------------------------------------------------
 * 실습 1 : 큐는 값을 복사한다
 *
 * xQueueSend(q, &x, 0) 에서 &x 는 "여기서 읽어가라"는 뜻이지
 * 포인터를 넘겨서 공유하는 것이 아니다. 커널이 memcpy로 큐 내부 버퍼에
 * 복사한다 (kernel/queue.c 의 prvCopyDataToQueue 참고).
 *
 * 그래서 보낸 직후 원본을 마음대로 써도 안전하다. 지역 변수여도 된다.
 * 이것이 큐를 "가장 안전한 기본 선택"으로 만드는 성질이다.
 * -------------------------------------------------------------------------- */
static void prvDemoCopySemantics( void )
{
    Sample_t xOut;
    Sample_t xIn;

    lab_printf( "\n=== 실습 1: 큐는 값을 복사한다 ===\n\n" );

    xOut.ulSeq    = 1;
    xOut.ulSentAt = lab_now_ms();
    xOut.sValue   = 25;

    lab_printf( "  보내기 전   : sValue = %d\n", xOut.sValue );

    xQueueSend( xQueue, &xOut, 0 );

    /* 보낸 직후 원본을 훼손해 본다.
     * 만약 포인터를 공유하는 방식이었다면 받는 쪽도 999를 보게 된다. */
    xOut.sValue = 999;
    lab_printf( "  보낸 뒤 원본을 999로 덮어씀\n" );

    xQueueReceive( xQueue, &xIn, 0 );
    lab_printf( "  받은 값     : sValue = %d\n\n", xIn.sValue );

    if( xIn.sValue == 25 )
    {
        lab_printf( "  -> 원본을 바꿔도 받은 값은 그대로다. 복사되었다는 증거다.\n" );
        lab_printf( "     따라서 보낼 데이터는 지역 변수여도 안전하다.\n" );
    }
    else
    {
        lab_printf( "  -> 예상과 다르다!\n" );
    }
}

/* -----------------------------------------------------------------------------
 * 실습 2 : 생산자 - 소비자
 * -------------------------------------------------------------------------- */

/* 생산자: 150ms마다 데이터를 하나 만든다 */
static void vProducerTask( void * pvParameters )
{
    TickType_t xLastWake = xTaskGetTickCount();
    uint32_t   ulSeq     = 0;

    for( ;; )
    {
        Sample_t xSample;

        xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( 150 ) );

        xSample.ulSeq    = ++ulSeq;
        xSample.ulSentAt = lab_now_ms();
        xSample.sValue   = ( int16_t ) ( 20 + ( ulSeq * 3 ) % 15 );

        LOG( "생산 #%lu (값 %d) -> 큐에 넣음",
             ( unsigned long ) xSample.ulSeq, xSample.sValue );

        /* 타임아웃 0 : 자리가 없으면 즉시 포기한다.
         * 주기를 지켜야 하는 태스크는 블로킹하면 안 되기 때문이다 (2강 참고). */
        if( xQueueSend( xQueue, &xSample, 0 ) != pdPASS )
        {
            LOG( "  큐가 가득 참! #%lu 버림", ( unsigned long ) xSample.ulSeq );
        }
    }
}

/* 소비자: 큐에서 꺼낼 게 생길 때까지 잠들어 있는다 */
static void vConsumerTask( void * pvParameters )
{
    for( ;; )
    {
        Sample_t xSample;

        /* portMAX_DELAY = 데이터가 올 때까지 무한 대기.
         * 이 동안 CPU를 전혀 쓰지 않는다 (Blocked 상태).
         *
         * 폴링과 결정적으로 다른 점이다:
         *     while( uxQueueMessagesWaiting(q) == 0 ) { }   <- CPU 100% 낭비
         */
        if( xQueueReceive( xQueue, &xSample, portMAX_DELAY ) == pdPASS )
        {
            LOG( "  소비 #%lu (값 %d) <- 큐에서 대기한 시간 %lu ms",
                 ( unsigned long ) xSample.ulSeq,
                 xSample.sValue,
                 ( unsigned long ) ( lab_now_ms() - xSample.ulSentAt ) );
        }
    }
}

/* -----------------------------------------------------------------------------
 * 실습 3 : 생산이 소비보다 빠르면
 *
 * 생산 40ms 주기, 소비 150ms 주기.
 * 소비자가 못 따라가므로 큐가 차고, 결국 데이터가 버려진다.
 * -------------------------------------------------------------------------- */
static void vFastProducerTask( void * pvParameters )
{
    TickType_t xLastWake = xTaskGetTickCount();
    uint32_t   ulSeq     = 0;

    for( ;; )
    {
        Sample_t xSample;

        xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( 40 ) );

        xSample.ulSeq    = ++ulSeq;
        xSample.ulSentAt = lab_now_ms();
        xSample.sValue   = ( int16_t ) ulSeq;

        ulSentCount++;

        if( xQueueSend( xQueue, &xSample, 0 ) == pdPASS )
        {
            LOG( "생산 #%-2lu  [큐 %lu/%d]",
                 ( unsigned long ) xSample.ulSeq,
                 ( unsigned long ) uxQueueMessagesWaiting( xQueue ),
                 QUEUE_LEN );
        }
        else
        {
            ulDropCount++;
            LOG( "생산 #%-2lu  [큐 %lu/%d]  <- 가득 참, 버림!",
                 ( unsigned long ) xSample.ulSeq,
                 ( unsigned long ) uxQueueMessagesWaiting( xQueue ),
                 QUEUE_LEN );
        }
    }
}

static void vSlowConsumerTask( void * pvParameters )
{
    for( ;; )
    {
        Sample_t xSample;

        if( xQueueReceive( xQueue, &xSample, portMAX_DELAY ) == pdPASS )
        {
            ulRecvCount++;
            LOG( "  소비 #%-2lu [큐 %lu/%d]",
                 ( unsigned long ) xSample.ulSeq,
                 ( unsigned long ) uxQueueMessagesWaiting( xQueue ),
                 QUEUE_LEN );
        }

        /* 처리에 150ms가 걸린다고 가정 */
        vTaskDelay( pdMS_TO_TICKS( 150 ) );
    }
}

/* -----------------------------------------------------------------------------
 * 진행 지휘
 * -------------------------------------------------------------------------- */
static void prvRunPair( TaskFunction_t pxProducer,
                        TaskFunction_t pxConsumer,
                        unsigned long ulRunMs )
{
    configASSERT( xTaskCreate( pxProducer, "Prod", STACK_SIZE, NULL,
                               PRIO_PRODUCER, &xProducerTask ) == pdPASS );
    configASSERT( xTaskCreate( pxConsumer, "Cons", STACK_SIZE, NULL,
                               PRIO_CONSUMER, &xConsumerTask ) == pdPASS );

    vTaskDelay( pdMS_TO_TICKS( ulRunMs ) );

    /* 큐에서 대기 중인 태스크를 지워도 안전하다.
     * 커널이 대기 리스트에서 먼저 제거해 준다. */
    vTaskDelete( xProducerTask );
    vTaskDelete( xConsumerTask );
    xProducerTask = NULL;
    xConsumerTask = NULL;

    /* 다음 실습을 위해 큐를 비운다 */
    xQueueReset( xQueue );
}

static void vControlTask( void * pvParameters )
{
    prvDemoCopySemantics();

    lab_printf( "\n=== 실습 2: 생산자-소비자 ===\n" );
    lab_printf( "  생산 150ms 주기 / 소비자는 데이터를 기다리며 Blocked\n\n" );
    prvRunPair( vProducerTask, vConsumerTask, 800 );
    lab_printf( "\n  -> 생산과 소비가 같은 시각에 찍힌다.\n" );
    lab_printf( "     소비자가 폴링이 아니라 Blocked 상태로 기다렸기 때문이다.\n" );

    lab_printf( "\n=== 실습 3: 생산이 소비보다 빠르면 ===\n" );
    lab_printf( "  생산 40ms 주기 / 소비 150ms 주기 / 큐 길이 %d\n\n", QUEUE_LEN );
    prvRunPair( vFastProducerTask, vSlowConsumerTask, 900 );

    lab_printf( "\n  생산 %lu 건, 소비 %lu 건, 버림 %lu 건\n",
                ulSentCount, ulRecvCount, ulDropCount );
    lab_printf( "  -> 큐는 속도 차이를 '흡수'할 뿐 '해결'하지 못한다.\n" );
    lab_printf( "     평균 생산 속도가 평균 소비 속도보다 빠르면 반드시 넘친다.\n" );

    lab_printf( "\n실습 종료.\n" );
    exit( 0 );
}

/* ========================================================================== */
int main( void )
{
    lab_printf( "===============================================\n" );
    lab_printf( " 3강: 큐\n" );
    lab_printf( "===============================================\n" );

    /* xQueueCreate( 항목 개수, 항목 하나의 바이트 크기 )
     *
     * 항목 크기를 여기서 못박는다. 큐는 (크기 x 개수)만큼의 버퍼를
     * 힙에 미리 잡아둔다. 그래서 가변 길이 데이터는 큐로 보낼 수 없다
     * (그건 10강의 메시지 버퍼가 담당한다). */
    xQueue = xQueueCreate( QUEUE_LEN, sizeof( Sample_t ) );
    configASSERT( xQueue != NULL );

    lab_printf( "\n큐 생성: %d개 x %u바이트. 남은 힙 %lu바이트\n",
                QUEUE_LEN, ( unsigned ) sizeof( Sample_t ),
                ( unsigned long ) xPortGetFreeHeapSize() );

    configASSERT( xTaskCreate( vControlTask, "Ctrl", STACK_SIZE, NULL,
                               PRIO_CTRL, NULL ) == pdPASS );

    vTaskStartScheduler();

    lab_printf( "스케줄러 시작 실패!\n" );
    for( ;; );
    return 0;
}
