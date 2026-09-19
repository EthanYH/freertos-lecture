#include "lab.h"
#include <stdarg.h>
#include <string.h>
#include <windows.h>

/* -----------------------------------------------------------------------------
 * 출력
 *
 * Windows에서 한글이 깨지는 이유:
 *   소스 파일은 UTF-8인데, Windows 콘솔의 기본 코드페이지는 CP949다.
 *   printf는 문자열의 바이트를 그대로 내보내므로, UTF-8 바이트열을
 *   콘솔이 CP949로 해석해서 깨진 글자가 나온다.
 *
 * 해결책은 세 가지가 있다.
 *   1) SetConsoleOutputCP(CP_UTF8) - 콘솔 설정을 바꾼다.
 *      간단하지만 콘솔 호스트/폰트에 따라 불안정하고, 프로그램이 끝난 뒤에도
 *      사용자의 콘솔 설정이 바뀐 채로 남는 부작용이 있다.
 *   2) -fexec-charset=CP949 로 컴파일 - 문자열을 아예 CP949로 저장한다.
 *      콘솔이 UTF-8인 환경에서는 반대로 깨진다.
 *   3) UTF-16으로 변환해 WriteConsoleW()로 직접 쓴다.  ← 이걸 쓴다
 *      콘솔 코드페이지와 무관하게 항상 올바르다. Windows 콘솔의 내부
 *      표현이 UTF-16이므로, 코드페이지 변환 단계를 아예 건너뛰는 셈이다.
 * -------------------------------------------------------------------------- */

/* 이 핸들이 콘솔인가, 아니면 파일/파이프로 리다이렉트된 것인가 */
static int prvIsConsole( HANDLE h )
{
    DWORD dwMode;
    return ( h != INVALID_HANDLE_VALUE ) && ( GetConsoleMode( h, &dwMode ) != 0 );
}

void lab_printf( const char * fmt, ... )
{
    char    cUtf8[ 512 ];
    WCHAR   wcUtf16[ 512 ];
    va_list args;
    int     iLen;
    HANDLE  hOut;
    DWORD   dwWritten;

    va_start( args, fmt );
    iLen = vsnprintf( cUtf8, sizeof( cUtf8 ), fmt, args );
    va_end( args );

    if( iLen < 0 )
    {
        return;
    }

    /* 버퍼보다 긴 출력은 잘린다 (학습용이라 이걸로 충분하다) */
    if( iLen >= ( int ) sizeof( cUtf8 ) )
    {
        iLen = ( int ) sizeof( cUtf8 ) - 1;
    }

    hOut = GetStdHandle( STD_OUTPUT_HANDLE );

    if( prvIsConsole( hOut ) )
    {
        int iWide = MultiByteToWideChar( CP_UTF8, 0, cUtf8, iLen,
                                         wcUtf16, sizeof( wcUtf16 ) / sizeof( WCHAR ) );
        if( iWide > 0 )
        {
            WriteConsoleW( hOut, wcUtf16, ( DWORD ) iWide, &dwWritten, NULL );
            return;
        }
    }

    /* 콘솔이 아니거나 변환에 실패한 경우: UTF-8 바이트를 그대로 쓴다.
     * 리다이렉트된 파일은 UTF-8로 열면 정상적으로 읽힌다. */
    WriteFile( hOut, cUtf8, ( DWORD ) iLen, &dwWritten, NULL );
}

/* -----------------------------------------------------------------------------
 * 시간 / 태스크 정보
 * -------------------------------------------------------------------------- */

unsigned long long lab_now_us( void )
{
    static LARGE_INTEGER xFreq  = { .QuadPart = 0 };
    static LARGE_INTEGER xStart = { .QuadPart = 0 };
    LARGE_INTEGER xNow;

    if( xFreq.QuadPart == 0 )
    {
        QueryPerformanceFrequency( &xFreq );
        QueryPerformanceCounter( &xStart );
    }

    QueryPerformanceCounter( &xNow );
    return ( unsigned long long )
           ( ( ( xNow.QuadPart - xStart.QuadPart ) * 1000000LL ) / xFreq.QuadPart );
}

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
