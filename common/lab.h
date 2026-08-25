/*
 * lab.h - 실습 공용 헬퍼
 */
#ifndef LAB_H
#define LAB_H

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * lab_printf() - printf 대신 쓰는 출력 함수
 *
 * Windows 콘솔은 기본 코드페이지가 CP949(EUC-KR)라, UTF-8로 저장된
 * 소스의 한글 문자열을 printf로 그냥 내보내면 깨진다.
 * lab_printf()는 콘솔에 출력할 때 UTF-16으로 변환해 WriteConsoleW()로
 * 직접 쓰므로, 콘솔 코드페이지가 무엇이든 항상 올바르게 나온다.
 * 파일이나 파이프로 리다이렉트한 경우에는 UTF-8 바이트를 그대로 쓴다.
 */
void lab_printf( const char * fmt, ... );

/*
 * LOG() - 실행 시각과 태스크 이름을 함께 찍는다.
 *
 * 출력 예:  [   120 ms][TaskA    ] 안녕
 *
 * 스케줄링 순서를 눈으로 보는 것이 이 실습의 핵심이므로,
 * 모든 레슨에서 이걸 쓴다.
 */
#define LOG( fmt, ... )                                     \
    lab_printf( "[%6lu ms][%-9s] " fmt "\n",                \
                (unsigned long) lab_now_ms(),               \
                lab_task_name(),                            \
                ##__VA_ARGS__ )

/* 스케줄러 시작 후 경과 시간(ms) */
unsigned long lab_now_ms( void );

/* 현재 실행 중인 태스크의 이름. 스케줄러 시작 전이면 "main" */
const char * lab_task_name( void );

/* CPU를 실제로 소모하는 가짜 작업.
 * vTaskDelay와 달리 블로킹하지 않으므로 "바쁜 대기"를 재현할 때 쓴다.
 * (1강 과제 2, 2강에서 주기 초과 상황을 만들 때 사용) */
void lab_burn_ms( unsigned long ms );

#endif /* LAB_H */
