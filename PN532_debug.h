/**
 * ============================================================
 *  PN532_debug.h — 调试输出宏定义
 * ============================================================
 *
 * 【文件作用】
 *   提供条件编译的调试输出宏，用于在开发阶段打印
 *   PN532 通信过程中的调试信息。
 *
 * 【使用方法】
 *   取消注释 #define DEBUG 即可启用调试输出。
 *   启用后，所有 DMSG/DMSG_HEX/DMSG_INT 调用都会
 *   通过串口打印调试信息；禁用时这些宏展开为空，不产生任何代码。
 *
 * 【可用宏】
 *   DMSG(args...)    — 打印任意类型的调试信息（支持多参数）
 *   DMSG_STR(str)    — 打印字符串并换行
 *   DMSG_HEX(num)    — 以两位十六进制格式打印一个字节（前导空格）
 *   DMSG_INT(num)    — 以十进制格式打印一个整数（前导空格）
 *
 * ============================================================
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

/* 取消下面这行的注释即可启用调试输出 */
//#define DEBUG

#include "Arduino.h"

#ifdef DEBUG
/* ---- 调试模式：所有调试宏都会产生实际输出 ---- */
#define DMSG(args...)       Serial.print(args)
#define DMSG_STR(str)       Serial.println(str)
#define DMSG_HEX(num)       Serial.print(' '); Serial.print((num>>4)&0x0F, HEX); Serial.print(num&0x0F, HEX)
#define DMSG_INT(num)       Serial.print(' '); Serial.print(num)
#else
/* ---- 非调试模式：所有调试宏展开为空，不产生任何代码 ---- */
#define DMSG(args...)
#define DMSG_STR(str)
#define DMSG_HEX(num)
#define DMSG_INT(num)
#endif

#endif
