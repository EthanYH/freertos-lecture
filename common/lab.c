#include "lab.h"
#include <windows.h>

unsigned long lab_now_ms( void )
{
    /* 틱 카운트를 ms로 환산. 틱이 1000Hz면 1틱 = 1ms 이므로 그대로다. */
    return ( unsigned long ) ( xTaskGetTickCount() * ( 1000UL / configTICK_RATE_HZ ) );
}

const char * lab_task_name( void )
{
    /* 스케줄러가 아직 안 돌고 있으면 태스크 컨텍스트가 없다. */
    if( xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED )
    {
        return "main";
    }
    return pcTaskGetName( NULL );   /* NULL = 현재 태스크 */
}

void lab_burn_ms( unsigned long ms )
{
    /* Windows 포트에서는 한 번에 한 태스크만 실제로 도므로,
     * QueryPerformanceCounter로 실제 벽시계 시간을 태운다.
     * 실제 MCU라면 for 루프로 사이클을 태우는 것과 같은 의미다. */
    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency( &freq );
    QueryPerformanceCounter( &start );

    for( ;; )
    {
        QueryPerformanceCounter( &now );
        if( ( ( now.QuadPart - start.QuadPart ) * 1000 ) / freq.QuadPart >= ( LONGLONG ) ms )
        {
            break;
        }
    }
}
