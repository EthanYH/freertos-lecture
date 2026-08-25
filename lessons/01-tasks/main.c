/* =============================================================================
 * 1강: 태스크와 우선순위
 *
 * 이 프로그램이 보여주는 것
 *   1) xTaskCreate()로 태스크를 만드는 법
 *   2) 우선순위가 높은 태스크가 낮은 태스크를 선점(preempt)하는 모습
 *   3) 블로킹하지 않는 태스크가 낮은 우선순위를 굶기는(starvation) 모습
 *   4) vTaskDelete()로 태스크를 끝내는 법
 * ========================================================================== */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lab.h"

/* 우선순위. 숫자가 클수록 높다. 0번은 Idle 태스크가 쓰므로 1부터 쓴다. */
#define PRIO_LOW     1
#define PRIO_MID     2
#define PRIO_HIGH    3

/* 태스크 스택 크기. 단위는 "워드"이지 바이트가 아니다.
 * printf가 스택을 꽤 먹으므로 최소값의 4배를 준다. */
#define STACK_SIZE   ( configMINIMAL_STACK_SIZE * 4 )

/* -----------------------------------------------------------------------------
 * 낮은 우선순위 태스크
 *
 * 이 태스크는 계속 실행되려 하지만, 더 높은 우선순위 태스크가 깨어날 때마다
 * 중간에 강제로 중단된다. 그게 "선점"이다.
 * -------------------------------------------------------------------------- */
static void vLowTask( void * pvParameters )
{
    unsigned long ulCount = 0;

    /* 태스크 함수는 절대 return하면 안 된다. 무한 루프가 원칙이다. */
    for( ;; )
    {
        LOG( "낮은 우선순위 작업 중... (%lu)", ++ulCount );

        /* 여기서 200ms 동안 Blocked 상태가 된다.
         * 이 동안 CPU를 전혀 쓰지 않으므로 다른 태스크가 마음껏 돈다. */
        vTaskDelay( pdMS_TO_TICKS( 200 ) );
    }
}

/* -----------------------------------------------------------------------------
 * 중간 우선순위 태스크
 *
 * xTaskCreate()의 4번째 인자로 넘긴 값을 pvParameters로 받는다.
 * 여러 태스크가 같은 함수를 공유할 때 이 인자로 구분한다.
 * -------------------------------------------------------------------------- */
static void vMidTask( void * pvParameters )
{
    const char * pcLabel = ( const char * ) pvParameters;

    for( ;; )
    {
        LOG( "%s 가 깨어나서 낮은 태스크를 선점했다", pcLabel );
        vTaskDelay( pdMS_TO_TICKS( 500 ) );
    }
}

/* -----------------------------------------------------------------------------
 * 높은 우선순위 태스크 - 5번 실행하고 스스로 종료한다
 *
 * 종료 방법이 중요하다. 그냥 return하면 동작이 정의되지 않는다.
 * 반드시 vTaskDelete(NULL)을 호출해야 한다. NULL = "나 자신"이라는 뜻.
 * -------------------------------------------------------------------------- */
static void vHighTask( void * pvParameters )
{
    int i;

    for( i = 1; i <= 5; i++ )
    {
        LOG( "높은 우선순위! 다른 태스크를 모두 밀어낸다 (%d/5)", i );
        vTaskDelay( pdMS_TO_TICKS( 300 ) );
    }

    LOG( "할 일이 끝났다. 스스로 종료한다." );

    /* 이 줄 이후는 실행되지 않는다.
     * 삭제된 태스크의 스택/TCB 메모리는 Idle 태스크가 나중에 회수한다. */
    vTaskDelete( NULL );
}

/* ========================================================================== */
int main( void )
{
    BaseType_t xResult;

    printf( "===============================================\n" );
    printf( " 1강: 태스크와 우선순위\n" );
    printf( "===============================================\n\n" );

    LOG( "태스크를 생성한다 (아직 스케줄러는 안 돌고 있음)" );

    /*
     * xTaskCreate( 함수, 이름, 스택크기, 인자, 우선순위, 핸들 )
     *
     *   함수      - 태스크 본체. void (*)(void *) 형태
     *   이름      - 디버깅/로그용 문자열 (동작에는 영향 없음)
     *   스택크기  - 워드 단위
     *   인자      - 태스크 함수에 pvParameters로 전달됨
     *   우선순위  - 0 ~ (configMAX_PRIORITIES-1), 클수록 높음
     *   핸들      - 나중에 이 태스크를 지목할 때 쓸 핸들. 필요 없으면 NULL
     *
     * 반환값 pdPASS면 성공. pdFAIL이면 힙이 부족한 것이다.
     * 실제 코드에서는 이 반환값을 반드시 확인해야 한다.
     */
    xResult = xTaskCreate( vLowTask, "Low", STACK_SIZE, NULL, PRIO_LOW, NULL );
    configASSERT( xResult == pdPASS );

    xResult = xTaskCreate( vMidTask, "Mid", STACK_SIZE, "Mid태스크", PRIO_MID, NULL );
    configASSERT( xResult == pdPASS );

    xResult = xTaskCreate( vHighTask, "High", STACK_SIZE, NULL, PRIO_HIGH, NULL );
    configASSERT( xResult == pdPASS );

    LOG( "생성 완료. 남은 힙: %lu 바이트", ( unsigned long ) xPortGetFreeHeapSize() );
    LOG( "스케줄러를 시작한다.\n" );

    /*
     * 스케줄러 시작. 이 함수는 정상적인 경우 절대 반환하지 않는다.
     * 여기서부터 커널이 CPU를 통제하며, main()은 사실상 끝난 것이다.
     */
    vTaskStartScheduler();

    /* 여기 도달했다면 힙이 부족해 Idle 태스크조차 못 만든 것이다. */
    printf( "스케줄러 시작 실패! 힙이 부족하다.\n" );
    for( ;; );
    return 0;
}
