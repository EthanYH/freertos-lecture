/*
 * hooks.c - FreeRTOS 훅 함수 모음
 *
 * 훅(hook)이란 "커널이 특정 상황에서 불러주는 콜백"이다.
 * FreeRTOSConfig.h에서 기능을 켜면 커널이 이 함수들을 호출하므로,
 * 반드시 정의되어 있어야 한다 (없으면 링크 에러).
 *
 * 학습 중에는 실수를 조용히 넘기지 않는 것이 중요하므로,
 * 모든 훅이 명확한 메시지를 찍고 프로그램을 세운다.
 */
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lab.h"

/*
 * pvPortMalloc() 실패 시 호출된다.
 * 원인: configTOTAL_HEAP_SIZE 부족, 또는 힙 단편화.
 * xTaskCreate / xQueueCreate 등이 내부적으로 pvPortMalloc을 쓰므로
 * 태스크를 너무 많이 만들면 여기로 온다.
 */
void vApplicationMallocFailedHook( void )
{
    lab_printf( "\n*** 힙 할당 실패 ***\n" );
    lab_printf( "    남은 힙: %lu 바이트\n",
            ( unsigned long ) xPortGetFreeHeapSize() );
    lab_printf( "    configTOTAL_HEAP_SIZE를 늘리거나 태스크 수를 줄이세요.\n" );
    exit( 1 );
}

/*
 * 스택 오버플로우가 감지되면 호출된다.
 * configCHECK_FOR_STACK_OVERFLOW 가 1 또는 2여야 동작한다.
 *
 * 실제 MCU에서 이게 안 켜져 있으면, 넘친 스택이 옆 태스크의 메모리를
 * 조용히 뭉개서 엉뚱한 곳에서 버그가 터진다. 디버깅 지옥의 원인이다.
 */
void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName )
{
    ( void ) xTask;
    lab_printf( "\n*** 스택 오버플로우: '%s' ***\n", pcTaskName );
    lab_printf( "    xTaskCreate()의 스택 크기 인자를 늘리세요.\n" );
    exit( 1 );
}

/*
 * configASSERT()가 실패하면 호출된다.
 * 커널이 "이건 있을 수 없는 상황"이라고 판단한 지점이다.
 * 대부분 API를 잘못 쓴 경우다 (예: ISR에서 FromISR 아닌 함수 호출).
 */
void vAssertCalled( const char * pcFile, unsigned long ulLine )
{
    lab_printf( "\n*** ASSERT 실패 ***\n" );
    lab_printf( "    위치: %s : %lu\n", pcFile, ulLine );
    exit( 1 );
}

/*
 * Windows 포트가 요구하는 훅.
 * 데몬(타이머 서비스) 태스크가 시작될 때 한 번 호출된다.
 */
void vApplicationDaemonTaskStartupHook( void )
{
}
