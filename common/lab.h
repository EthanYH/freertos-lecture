/*
 * lab.h - 실습 공용 헬퍼
 */
#ifndef LAB_H
#define LAB_H

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

/*
 * LOG() - 실행 시각과 태스크 이름을 함께 찍는다.
 *
 * 출력 예:  [   120 ms][TaskA    ] 안녕
 *
 * 스케줄링 순서를 눈으로 보는 것이 이 실습의 핵심이므로,
 * 모든 레슨에서 printf 대신 이걸 쓴다.
 */
#define LOG( fmt, ... )                                     \
    printf( "[%6lu ms][%-9s] " fmt "\n",                    \
            (unsigned long) lab_now_ms(),                   \
            lab_task_name(),                                \
            ##__VA_ARGS__ )

/* 스케줄러 시작 후 경과 시간(ms) */
unsigned long lab_now_ms( void );

/* 현재 실행 중인 태스크의 이름. 스케줄러 시작 전이면 "main" */
const char * lab_task_name( void );

/* CPU를 실제로 소모하는 가짜 작업.
 * vTaskDelay와 달리 블로킹하지 않으므로 "바쁜 대기"를 재현할 때 쓴다.
 * (2강에서 주기 초과 상황을 만들 때 사용) */
void lab_burn_ms( unsigned long ms );

#endif /* LAB_H */
