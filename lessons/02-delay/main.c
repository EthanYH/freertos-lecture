/* =============================================================================
 * 2강: 딜레이와 주기 실행
 *
 * 이 프로그램이 보여주는 것
 *   실습 1) vTaskDelay()로 주기를 만들면 오차가 누적된다 (드리프트)
 *   실습 2) xTaskDelayUntil()은 절대 시각 기준이라 드리프트가 없다
 *   실습 3) 작업이 주기보다 길어지면 어떻게 되는가 (마감 위반과 catch-up)
 *
 * 세 실습을 한 태스크가 순서대로 실행한다.
 * 여러 태스크를 동시에 돌리면 서로 CPU를 뺏어 측정값이 오염되기 때문이다.
 * ========================================================================== */

#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lab.h"

#define PRIO_DEMO     2
#define STACK_SIZE    ( configMINIMAL_STACK_SIZE * 4 )

/* 주기와 반복 횟수 */
#define PERIOD_MS     100
#define CYCLES        8

/* 매 회차 작업 시간을 일부러 들쭉날쭉하게 만든다.
 * 실제 시스템에서 작업 시간이 매번 같은 경우는 없기 때문이다. */
static unsigned long prvWorkMs( int i )
{
    static const unsigned long ulPattern[] = { 10, 25, 15, 30, 12, 28, 18, 22 };
    return ulPattern[ i % ( sizeof( ulPattern ) / sizeof( ulPattern[ 0 ] ) ) ];
}

/* -----------------------------------------------------------------------------
 * 실습 1 : vTaskDelay() - 상대 시간 기준
 *
 * vTaskDelay(N)은 "지금부터 N틱 뒤"에 깨운다.
 * 따라서 실제 주기 = 작업 시간 + N 이 되어 매번 밀린다.
 * 게다가 작업 시간이 들쭉날쭉하면 밀리는 양도 들쭉날쭉해진다(지터).
 * -------------------------------------------------------------------------- */
static void prvDemoTaskDelay( void )
{
    TickType_t xStart;
    int i;

    lab_printf( "\n" );
    lab_printf( "=== 실습 1: vTaskDelay() - 주기가 밀린다 ===\n" );
    lab_printf( "  목표 주기 %d ms, %d 회 반복\n\n", PERIOD_MS, CYCLES );
    lab_printf( "  회차 | 작업시간 | 실제 경과 | 이상적 | 누적 오차\n" );
    lab_printf( "  -----+----------+-----------+--------+----------\n" );

    xStart = xTaskGetTickCount();

    for( i = 1; i <= CYCLES; i++ )
    {
        unsigned long ulWork = prvWorkMs( i - 1 );

        /* 작업 (CPU를 실제로 태운다. 블로킹이 아니다) */
        lab_burn_ms( ulWork );

        /* 그리고 쉰다. 문제는 이 시점이 매번 달라진다는 것이다. */
        vTaskDelay( pdMS_TO_TICKS( PERIOD_MS ) );

        {
            unsigned long ulElapsed = ( unsigned long ) ( xTaskGetTickCount() - xStart );
            unsigned long ulIdeal   = ( unsigned long ) i * PERIOD_MS;

            lab_printf( "  %4d | %6lu ms | %7lu ms | %4lu ms | %+6ld ms\n",
                        i, ulWork, ulElapsed, ulIdeal,
                        ( long ) ulElapsed - ( long ) ulIdeal );
        }
    }

    lab_printf( "\n  -> 오차가 매 회차 쌓인다. 이것이 드리프트(drift)다.\n" );
}

/* -----------------------------------------------------------------------------
 * 실습 2 : xTaskDelayUntil() - 절대 시각 기준
 *
 * xLastWakeTime에 "마지막으로 깨어난 시각"이 저장되고,
 * 커널이 거기에 주기를 더한 시점에 깨워준다.
 * 작업이 얼마나 걸렸든 깨어나는 시각은 항상 0, 100, 200, 300...이다.
 * -------------------------------------------------------------------------- */
static void prvDemoDelayUntil( void )
{
    TickType_t xLastWake;
    TickType_t xStart;
    int i;

    lab_printf( "\n" );
    lab_printf( "=== 실습 2: xTaskDelayUntil() - 주기가 고정된다 ===\n" );
    lab_printf( "  목표 주기 %d ms, %d 회 반복 (작업 시간은 실습 1과 동일)\n\n",
                PERIOD_MS, CYCLES );
    lab_printf( "  회차 | 작업시간 | 깨어난 시각 | 이상적 | 누적 오차\n" );
    lab_printf( "  -----+----------+-------------+--------+----------\n" );

    /* 기준점을 현재 시각으로 초기화한다. 이 한 줄을 빠뜨리면
     * xLastWake가 쓰레기 값이라 동작이 엉망이 된다. */
    xLastWake = xTaskGetTickCount();
    xStart    = xLastWake;

    for( i = 1; i <= CYCLES; i++ )
    {
        unsigned long ulWork = prvWorkMs( i - 1 );

        /* 관용적으로 루프 맨 앞에 둔다.
         * "주기의 시작점에서 깨어난다"는 의미가 코드에 드러난다. */
        xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( PERIOD_MS ) );

        {
            unsigned long ulElapsed = ( unsigned long ) ( xTaskGetTickCount() - xStart );
            unsigned long ulIdeal   = ( unsigned long ) i * PERIOD_MS;

            lab_printf( "  %4d | %6lu ms | %9lu ms | %4lu ms | %+6ld ms\n",
                        i, ulWork, ulElapsed, ulIdeal,
                        ( long ) ulElapsed - ( long ) ulIdeal );
        }

        lab_burn_ms( ulWork );
    }

    lab_printf( "\n  -> 작업 시간이 들쭉날쭉해도 깨어나는 시각은 정확하다.\n" );
}

/* -----------------------------------------------------------------------------
 * 실습 3 : 작업이 주기보다 길어지면?
 *
 * FreeRTOS는 마감을 강제하지 않는다. 에러도 예외도 없다.
 * 대신 xTaskDelayUntil()이 pdFALSE를 반환한다 -
 * "깨울 시각이 이미 지나서 대기하지 않고 즉시 돌아왔다"는 뜻이다.
 * 이것이 마감 위반을 감지할 수 있는 유일한 신호다.
 *
 * 그리고 밀린 만큼 쉬지 않고 연속 실행해서 따라잡으려 한다(catch-up).
 * -------------------------------------------------------------------------- */
#define OVR_PERIOD_MS   50
#define OVR_CYCLES      10

static unsigned long prvOverrunWorkMs( int i )
{
    /* 4, 5회차에서만 주기(50ms)를 훌쩍 넘는 작업을 시킨다 */
    if( ( i == 4 ) || ( i == 5 ) )
    {
        return 130;
    }
    return 20;
}

static void prvDemoOverrun( void )
{
    TickType_t    xLastWake;
    TickType_t    xStart;
    unsigned long ulMissCount = 0;
    int i;

    lab_printf( "\n" );
    lab_printf( "=== 실습 3: 작업이 주기를 넘으면 ===\n" );
    lab_printf( "  주기 %d ms인데 4,5회차만 130 ms 작업을 시킨다\n\n", OVR_PERIOD_MS );
    lab_printf( "  회차 | 작업시간 | 시작 시각 | 이상적 | 마감\n" );
    lab_printf( "  -----+----------+-----------+--------+--------------\n" );

    xLastWake = xTaskGetTickCount();
    xStart    = xLastWake;

    for( i = 1; i <= OVR_CYCLES; i++ )
    {
        unsigned long ulWork = prvOverrunWorkMs( i );
        BaseType_t    xDelayed;

        /* 반환값이 핵심이다.
         *   pdTRUE  = 정상적으로 대기했다
         *   pdFALSE = 깨울 시각이 이미 과거라 대기 없이 즉시 반환했다 = 마감 위반 */
        xDelayed = xTaskDelayUntil( &xLastWake, pdMS_TO_TICKS( OVR_PERIOD_MS ) );

        if( xDelayed == pdFALSE )
        {
            ulMissCount++;
        }

        {
            unsigned long ulElapsed = ( unsigned long ) ( xTaskGetTickCount() - xStart );
            unsigned long ulIdeal   = ( unsigned long ) i * OVR_PERIOD_MS;

            lab_printf( "  %4d | %6lu ms | %7lu ms | %4lu ms | %s\n",
                        i, ulWork, ulElapsed, ulIdeal,
                        ( xDelayed == pdTRUE ) ? "준수" : "위반 (대기 없이 즉시 실행)" );
        }

        lab_burn_ms( ulWork );
    }

    lab_printf( "\n  -> 마감 위반 %lu 회.\n", ulMissCount );
    lab_printf( "  -> 위반 후에는 대기 없이 연속 실행해 밀린 주기를 따라잡는다.\n" );
    lab_printf( "     이 동안 낮은 우선순위 태스크는 CPU를 거의 못 받는다.\n" );
}

/* ========================================================================== */
static void vDemoTask( void * pvParameters )
{
    prvDemoTaskDelay();
    prvDemoDelayUntil();
    prvDemoOverrun();

    lab_printf( "\n실습 종료.\n" );

    /* 학습용이므로 여기서 프로그램을 끝낸다.
     * 실제 펌웨어라면 태스크는 무한 루프여야 한다. */
    exit( 0 );
}

int main( void )
{
    lab_printf( "===============================================\n" );
    lab_printf( " 2강: 딜레이와 주기 실행\n" );
    lab_printf( "===============================================\n" );

    configASSERT( xTaskCreate( vDemoTask, "Demo", STACK_SIZE, NULL,
                               PRIO_DEMO, NULL ) == pdPASS );

    vTaskStartScheduler();

    lab_printf( "스케줄러 시작 실패!\n" );
    for( ;; );
    return 0;
}
