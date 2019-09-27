/* ==========================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-14 Mike Karlesky, Mark VanderVoord, Greg Williams
    [Released under MIT License. Please refer to license.txt for details]
========================================== */

#ifndef UNITY_FRAMEWORK_H
#define UNITY_FRAMEWORK_H
#define UNITY

#ifdef __cplusplus
extern "C"
{
#endif
/*-------------------------------------------------------*/
/* Configuration Options Details                         */
/*-------------------------------------------------------*/
/* All options described below should be passed as a compiler flag to all files using Unity. If you must add #defines, place them BEFORE the #include above.

   Integers/longs/pointers
       - Unity assumes 32 bit integers, longs, and pointers by default
       - If your compiler treats ints of a different size, options are:
         - define UNITY_USE_LIMITS_H to use limits.h to determine sizes
         - define UNITY_INT_WIDTH, UNITY_LONG_WIDTH, nand UNITY_POINTER_WIDTH
  
   Floats
       - define UNITY_EXCLUDE_FLOAT to disallow floating point comparisons
       - define UNITY_FLOAT_PRECISION to specify the precision to use when doing TEST_ASSERT_EQUAL_FLOAT
       - define UNITY_FLOAT_TYPE to specify doubles instead of single precision floats
       - define UNITY_FLOAT_VERBOSE to print floating point values in errors (uses sprintf)
       - define UNITY_INCLUDE_DOUBLE to allow double floating point comparisons
       - define UNITY_EXCLUDE_DOUBLE to disallow double floating point comparisons (default)
       - define UNITY_DOUBLE_PRECISION to specify the precision to use when doing TEST_ASSERT_EQUAL_DOUBLE
       - define UNITY_DOUBLE_TYPE to specify something other than double 
       - define UNITY_DOUBLE_VERBOSE to print floating point values in errors (uses sprintf)
  
   Output
       - by default, Unity prints to standard out with putchar.  define UNITY_OUTPUT_CHAR(a) with a different function if desired
  
   Optimization
       - by default, line numbers are stored in unsigned shorts.  Define UNITY_LINE_TYPE with a different type if your files are huge
       - by default, test and failure counters are unsigned shorts.  Define UNITY_COUNTER_TYPE with a different type if you want to save space or have more than 65535 Tests.
  
   Test Cases
       - define UNITY_SUPPORT_TEST_CASES to include the TEST_CASE macro, though really it's mostly about the runner generator script
  
   Parameterized Tests
       - you'll want to create a define of TEST_CASE(...) which basically evaluates to nothing
*/

/* Hard code compile time switch options for both Target and Hosted Test Environmnet */
#define UNITY_USE_LIMITS_H
#define UNITY_EXCLUDE_STDINT_H
#define UNITY_EXCLUDE_FLOAT
#define UNITY_EXCLUDE_FLOAT_PRINT
#define UNITY_EXCLUDE_DOUBLE

/* Define integer and pointer sizes for Target Test Environmnet (TTE)*/
/* Host Test Environment (HTE) will calculate integer and pointer sizes  */
#ifndef HOSTED_TEST_ENVIRONMENT
#define UNITY_INT_WIDTH 16
#define UNITY_LONG_WIDTH 32
#define UNITY_POINTER_WIDTH 16
#endif

/*--------------------------------------------------------------------------------------------------------------*/
/* unity_internals.h                                                                                            */
/*--------------------------------------------------------------------------------------------------------------*/

#include <stdio.h>

#ifdef HOSTED_TEST_ENVIRONMENT
#undef UNITY_EXCLUDE_SETJMP_H
#else
#define UNITY_EXCLUDE_SETJMP_H
#endif
/* Include setjmp.h on the Hosted Test Environment (HTE)*/
/* Target Test Environment (TTE) doesn't have this capbility and uses panic to fail/abort */
#ifndef UNITY_EXCLUDE_SETJMP_H
#include <setjmp.h>
#else
#include "panic.h"
#endif

/* Unity attempts to determine sizeof(various types)      */
/* based on UINT_MAX, ULONG_MAX, etc. These are typically */
/* defined in limits.h.                                   */
#ifdef UNITY_USE_LIMITS_H
#include <limits.h>
#endif
/* As a fallback, hope that including stdint.h will */
/* provide this information.                        */
#ifndef UNITY_EXCLUDE_STDINT_H
#include <stdint.h>
#endif

/*-------------------------------------------------------*/
/* Guess Widths If Not Specified                         */
/*-------------------------------------------------------*/

/* Determine the size of an int, if not already specified.  */
/* We cannot use sizeof(int), because it is not yet defined */
/* at this stage in the translation of the C program.       */
/* Therefore, infer it from UINT_MAX if possible.           */
#ifndef UNITY_INT_WIDTH
  #ifdef UINT_MAX
    #if (UINT_MAX == 0xFFFF)
      #define UNITY_INT_WIDTH (16)
    #elif (UINT_MAX == 0xFFFFFFFF)
      #define UNITY_INT_WIDTH (32)
    #elif (UINT_MAX == 0xFFFFFFFFFFFFFFFF)
      #define UNITY_INT_WIDTH (64)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #endif
  #endif
#endif
#ifndef UNITY_INT_WIDTH
  #define UNITY_INT_WIDTH (32)
#endif

/* Determine the size of a long, if not already specified, */
/* by following the process used above to define           */
/* UNITY_INT_WIDTH.                                        */
#ifndef UNITY_LONG_WIDTH
  #ifdef ULONG_MAX
    #if (ULONG_MAX == 0xFFFF)
      #define UNITY_LONG_WIDTH (16)
    #elif (ULONG_MAX == 0xFFFFFFFF)
      #define UNITY_LONG_WIDTH (32)
    #elif (ULONG_MAX == 0xFFFFFFFFFFFFFFFF)
      #define UNITY_LONG_WIDTH (64)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #endif
  #endif
#endif
#ifndef UNITY_LONG_WIDTH
  #define UNITY_LONG_WIDTH (32)
#endif

/* Determine the size of a pointer, if not already specified, */
/* by following the process used above to define              */
/* UNITY_INT_WIDTH.                                           */
#ifndef UNITY_POINTER_WIDTH
  #ifdef UINTPTR_MAX
    #if (UINTPTR_MAX <= 0xFFFF)
      #define UNITY_POINTER_WIDTH (16)
    #elif (UINTPTR_MAX <= 0xFFFFFFFF)
      #define UNITY_POINTER_WIDTH (32)
    #elif (UINTPTR_MAX <= 0xFFFFFFFFFFFFFFFF)
      #define UNITY_POINTER_WIDTH (64)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #endif
  #endif
#endif
#ifndef UNITY_POINTER_WIDTH
  #ifdef INTPTR_MAX
    #if (INTPTR_MAX <= 0x7FFF)
      #define UNITY_POINTER_WIDTH (16)
    #elif (INTPTR_MAX <= 0x7FFFFFFF)
      #define UNITY_POINTER_WIDTH (32)
    #elif (INTPTR_MAX <= 0x7FFFFFFFFFFFFFFF)
      #define UNITY_POINTER_WIDTH (64)
      #ifndef UNITY_SUPPORT_64
      #define UNITY_SUPPORT_64
      #endif
    #endif
  #endif
#endif
#ifndef UNITY_POINTER_WIDTH
  #ifdef UNITY_SUPPORT_64
    #define UNITY_POINTER_WIDTH (64)
  #else
    #define UNITY_POINTER_WIDTH (32)
  #endif
#endif


/*-------------------------------------------------------*/
/* Int Support                                           */
/*-------------------------------------------------------*/

#if (UNITY_INT_WIDTH == 32)
    typedef unsigned char   UNITY_UINT8;
    typedef unsigned short  UNITY_UINT16;
    typedef unsigned int    UNITY_UINT32;
    typedef signed char     UNITY_INT8;
    typedef signed short    UNITY_INT16;
    typedef signed int      UNITY_INT32;
#elif (UNITY_INT_WIDTH == 16)
    typedef unsigned char   UNITY_UINT8;
    typedef unsigned int    UNITY_UINT16;
    typedef unsigned long   UNITY_UINT32;
    typedef signed char     UNITY_INT8;
    typedef signed int      UNITY_INT16;
    typedef signed long     UNITY_INT32;
#else
    #error Invalid UNITY_INT_WIDTH specified! (16 or 32 are supported)
#endif

/*-------------------------------------------------------*/
/* 64-bit Support                                        */
/*-------------------------------------------------------*/

#ifndef UNITY_SUPPORT_64
  #if UNITY_LONG_WIDTH == 64 || UNITY_POINTER_WIDTH == 64
    #define UNITY_SUPPORT_64
  #endif
#endif

#ifndef UNITY_SUPPORT_64
typedef UNITY_UINT32 UNITY_UINT;
typedef UNITY_INT32 UNITY_INT;

#else

    /* 64-bit Support */
  #if (UNITY_LONG_WIDTH == 32)
    typedef unsigned long long UNITY_UINT64;
    typedef signed long long   UNITY_INT64;
  #elif (UNITY_LONG_WIDTH == 64)
    typedef unsigned long      UNITY_UINT64;
    typedef signed long        UNITY_INT64;
  #else
    #error Invalid UNITY_LONG_WIDTH specified! (32 or 64 are supported)
  #endif
    typedef UNITY_UINT64 UNITY_UINT;
    typedef UNITY_INT64 UNITY_INT;

#endif

/*-------------------------------------------------------*/
/* Pointer Support                                       */
/*-------------------------------------------------------*/

#if (UNITY_POINTER_WIDTH == 32)
#define UNITY_PTR_TO_INT UNITY_INT32
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX32
#elif (UNITY_POINTER_WIDTH == 64)
#define UNITY_PTR_TO_INT UNITY_INT64
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX64
#elif (UNITY_POINTER_WIDTH == 16)
#define UNITY_PTR_TO_INT UNITY_INT16
#define UNITY_DISPLAY_STYLE_POINTER UNITY_DISPLAY_STYLE_HEX16
#else
    #error Invalid UNITY_POINTER_WIDTH specified! (16, 32 or 64 are supported)
#endif

#ifndef UNITY_PTR_ATTRIBUTE
#define UNITY_PTR_ATTRIBUTE
#endif

#ifndef UNITY_INTERNAL_PTR
#define UNITY_INTERNAL_PTR UNITY_PTR_ATTRIBUTE const void*
#endif

/*-------------------------------------------------------*/
/* Float Support                                         */
/*-------------------------------------------------------*/

#ifdef UNITY_EXCLUDE_FLOAT

/* No Floating Point Support */
#undef UNITY_FLOAT_PRECISION
#undef UNITY_FLOAT_TYPE
#undef UNITY_FLOAT_VERBOSE

#else

/* Floating Point Support */
#ifndef UNITY_FLOAT_PRECISION
#define UNITY_FLOAT_PRECISION (0.00001f)
#endif
#ifndef UNITY_FLOAT_TYPE
#define UNITY_FLOAT_TYPE float
#endif
typedef UNITY_FLOAT_TYPE UNITY_FLOAT;

/* isinf & isnan macros should be provided by math.h */
#ifndef isinf
/* The value of Inf - Inf is NaN */
#define isinf(n) (isnan((n) - (n)) && !isnan(n))
#endif

#ifndef isnan
/* NaN is the only floating point value that does NOT equal itself.
 * Therefore if n != n, then it is NaN. */
#define isnan(n) ((n != n) ? 1 : 0)
#endif

#endif

/*-------------------------------------------------------*/
/* Double Float Support                                  */
/*-------------------------------------------------------*/

/* unlike FLOAT, we DON'T include by default */
#ifndef UNITY_EXCLUDE_DOUBLE
#ifndef UNITY_INCLUDE_DOUBLE
#define UNITY_EXCLUDE_DOUBLE
#endif
#endif

#ifdef UNITY_EXCLUDE_DOUBLE

/* No Floating Point Support */
#undef UNITY_DOUBLE_PRECISION
#undef UNITY_DOUBLE_TYPE
#undef UNITY_DOUBLE_VERBOSE

#ifdef UNITY_INCLUDE_DOUBLE
#undef UNITY_INCLUDE_DOUBLE
#endif

#else

/* Floating Point Support */
#ifndef UNITY_DOUBLE_PRECISION
#define UNITY_DOUBLE_PRECISION (1e-12f)
#endif
#ifndef UNITY_DOUBLE_TYPE
#define UNITY_DOUBLE_TYPE double
#endif
typedef UNITY_DOUBLE_TYPE UNITY_DOUBLE;

#endif

/*-------------------------------------------------------*/
/* Output Method                                         */
/*-------------------------------------------------------*/
#ifndef UNITY_OUTPUT_CHAR
/* Default to using putchar, which is defined in stdio.h */
#include <stdio.h>
#define UNITY_OUTPUT_CHAR(a) (void)putchar(a)
#else
  /* If defined as something else, make sure we declare it here so it's ready for use */
  #ifdef UNITY_OUTPUT_CHAR_HEADER_DECLARATION
extern void UNITY_OUTPUT_CHAR_HEADER_DECLARATION;
  #endif
#endif

#ifndef UNITY_OUTPUT_FLUSH
#ifdef UNITY_USE_FLUSH_STDOUT
/* We want to use the stdout flush utility */
#include <stdio.h>
#define UNITY_OUTPUT_FLUSH() (void)fflush(stdout)
#else
/* We've specified nothing, therefore flush should just be ignored */
#define UNITY_OUTPUT_FLUSH()
#endif
#else
/* We've defined flush as something else, so make sure we declare it here so it's ready for use */
#ifdef UNITY_OUTPUT_FLUSH_HEADER_DECLARATION
extern void UNITY_OMIT_OUTPUT_FLUSH_HEADER_DECLARATION;
#endif
#endif

#ifndef UNITY_OUTPUT_FLUSH
#define UNITY_FLUSH_CALL()
#else
#define UNITY_FLUSH_CALL() UNITY_OUTPUT_FLUSH()
#endif

#ifndef UNITY_PRINT_EOL
#define UNITY_PRINT_EOL()    UNITY_OUTPUT_CHAR('\n')
#endif

#ifndef UNITY_OUTPUT_START
#define UNITY_OUTPUT_START()
#endif

#ifndef UNITY_OUTPUT_COMPLETE
#define UNITY_OUTPUT_COMPLETE()
#endif

/*-------------------------------------------------------*/
/*Footprint                                              */
/*-------------------------------------------------------*/

#ifndef UNITY_LINE_TYPE
#define UNITY_LINE_TYPE UNITY_UINT
#endif

#ifndef UNITY_COUNTER_TYPE
#define UNITY_COUNTER_TYPE UNITY_UINT
#endif

/*-------------------------------------------------------
 * Language Features Available
 *-------------------------------------------------------*/
#if !defined(UNITY_WEAK_ATTRIBUTE) && !defined(UNITY_WEAK_PRAGMA)
#   if defined(__GNUC__) || defined(__ghs__) /* __GNUC__ includes clang */
#       if !(defined(__WIN32__) && defined(__clang__)) && !defined(__TMS470__)
#           define UNITY_WEAK_ATTRIBUTE __attribute__((weak))
#       endif
#   endif
#endif

#ifdef UNITY_NO_WEAK
#   undef UNITY_WEAK_ATTRIBUTE
#   undef UNITY_WEAK_PRAGMA
#endif


/*-------------------------------------------------------*/
/* Internal Structs Needed                               */
/*-------------------------------------------------------*/

typedef void (*UnityTestFunction)(void);

#define UNITY_DISPLAY_RANGE_INT  (0x10)
#define UNITY_DISPLAY_RANGE_UINT (0x20)
#define UNITY_DISPLAY_RANGE_HEX  (0x40)
#define UNITY_DISPLAY_RANGE_AUTO (0x80)

typedef enum
{
#if (UNITY_INT_WIDTH == 16)
    UNITY_DISPLAY_STYLE_INT      = 2 + UNITY_DISPLAY_RANGE_INT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 32)
    UNITY_DISPLAY_STYLE_INT      = 4 + UNITY_DISPLAY_RANGE_INT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 64)
    UNITY_DISPLAY_STYLE_INT      = 8 + UNITY_DISPLAY_RANGE_INT + UNITY_DISPLAY_RANGE_AUTO,
#endif
    UNITY_DISPLAY_STYLE_INT8     = 1 + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_INT16    = 2 + UNITY_DISPLAY_RANGE_INT,
    UNITY_DISPLAY_STYLE_INT32    = 4 + UNITY_DISPLAY_RANGE_INT,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_INT64    = 8 + UNITY_DISPLAY_RANGE_INT,
#endif

#if (UNITY_INT_WIDTH == 16)
    UNITY_DISPLAY_STYLE_UINT     = 2 + UNITY_DISPLAY_RANGE_UINT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 32)
    UNITY_DISPLAY_STYLE_UINT     = 4 + UNITY_DISPLAY_RANGE_UINT + UNITY_DISPLAY_RANGE_AUTO,
#elif (UNITY_INT_WIDTH  == 64)
    UNITY_DISPLAY_STYLE_UINT     = 8 + UNITY_DISPLAY_RANGE_UINT + UNITY_DISPLAY_RANGE_AUTO,
#endif
    UNITY_DISPLAY_STYLE_UINT8    = 1 + UNITY_DISPLAY_RANGE_UINT,
    UNITY_DISPLAY_STYLE_UINT16   = 2 + UNITY_DISPLAY_RANGE_UINT,
    UNITY_DISPLAY_STYLE_UINT32   = 4 + UNITY_DISPLAY_RANGE_UINT,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_UINT64   = 8 + UNITY_DISPLAY_RANGE_UINT,
#endif

    UNITY_DISPLAY_STYLE_HEX8     = 1 + UNITY_DISPLAY_RANGE_HEX,
    UNITY_DISPLAY_STYLE_HEX16    = 2 + UNITY_DISPLAY_RANGE_HEX,
    UNITY_DISPLAY_STYLE_HEX32    = 4 + UNITY_DISPLAY_RANGE_HEX,
#ifdef UNITY_SUPPORT_64
    UNITY_DISPLAY_STYLE_HEX64    = 8 + UNITY_DISPLAY_RANGE_HEX,
#endif

    UNITY_DISPLAY_STYLE_UNKNOWN
} UNITY_DISPLAY_STYLE_T;

typedef enum
{
    UNITY_EQUAL_TO         = 1,
    UNITY_GREATER_THAN     = 2,
    UNITY_GREATER_OR_EQUAL = 2 + UNITY_EQUAL_TO,
    UNITY_SMALLER_THAN     = 4,
    UNITY_SMALLER_OR_EQUAL = 4 + UNITY_EQUAL_TO
} UNITY_COMPARISON_T;

#ifndef UNITY_EXCLUDE_FLOAT
typedef enum UNITY_FLOAT_TRAIT
{
    UNITY_FLOAT_IS_NOT_INF       = 0,
    UNITY_FLOAT_IS_INF,
    UNITY_FLOAT_IS_NOT_NEG_INF,
    UNITY_FLOAT_IS_NEG_INF,
    UNITY_FLOAT_IS_NOT_NAN,
    UNITY_FLOAT_IS_NAN,
    UNITY_FLOAT_IS_NOT_DET,
    UNITY_FLOAT_IS_DET,
    UNITY_FLOAT_INVALID_TRAIT
} UNITY_FLOAT_TRAIT_T;
#endif

typedef enum
{
    UNITY_ARRAY_TO_VAL = 0,
    UNITY_ARRAY_TO_ARRAY
} UNITY_FLAGS_T;

struct UNITY_STORAGE_T
{
    const char* TestFile;
    const char* CurrentTestName;
#ifndef UNITY_EXCLUDE_DETAILS
    const char* CurrentDetail1;
    const char* CurrentDetail2;
#endif
    UNITY_LINE_TYPE CurrentTestLineNumber;
    UNITY_COUNTER_TYPE NumberOfTests;
    UNITY_COUNTER_TYPE TestFailures;
    UNITY_COUNTER_TYPE TestIgnores;
    UNITY_COUNTER_TYPE CurrentTestFailed;
    UNITY_COUNTER_TYPE CurrentTestIgnored;
#ifndef UNITY_EXCLUDE_SETJMP_H
    jmp_buf AbortFrame;
#endif
};

extern struct UNITY_STORAGE_T Unity;

/*-------------------------------------------------------*/
/* Test Suite Management                                 */
/*-------------------------------------------------------*/

void UnityBegin(const char* filename);
int  UnityEnd(void);
void UnityConcludeTest(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum);

/*-------------------------------------------------------
 * Details Support
 *-------------------------------------------------------*/

#ifdef UNITY_EXCLUDE_DETAILS
#define UNITY_CLR_DETAILS()
#define UNITY_SET_DETAIL(d1)
#define UNITY_SET_DETAILS(d1,d2)
#else
#define UNITY_CLR_DETAILS()      { Unity.CurrentDetail1 = 0;   Unity.CurrentDetail2 = 0;  }
#define UNITY_SET_DETAIL(d1)     { Unity.CurrentDetail1 = d1;  Unity.CurrentDetail2 = 0;  }
#define UNITY_SET_DETAILS(d1,d2) { Unity.CurrentDetail1 = d1;  Unity.CurrentDetail2 = d2; }

#ifndef UNITY_DETAIL1_NAME
#define UNITY_DETAIL1_NAME "Function"
#endif

#ifndef UNITY_DETAIL2_NAME
#define UNITY_DETAIL2_NAME "Argument"
#endif
#endif


/*-------------------------------------------------------*/
/* Test Output                                           */
/*-------------------------------------------------------*/

void UnityPrint(const char* string);
void UnityPrintLen(const char* string, const UNITY_UINT32 length);
void UnityPrintMask(const UNITY_UINT mask, const UNITY_UINT number);
void UnityPrintNumberByStyle(const UNITY_INT number, const UNITY_DISPLAY_STYLE_T style);
void UnityPrintNumber(const UNITY_INT number);
void UnityPrintNumberUnsigned(const UNITY_UINT number);
void UnityPrintNumberHex(const UNITY_UINT number, const char nibbles);

#ifndef UNITY_EXCLUDE_FLOAT_PRINT
void UnityPrintFloat(const UNITY_DOUBLE input_number);
#endif

/*-------------------------------------------------------*/
/* Test Assertion Fuctions                               */
/*-------------------------------------------------------*/
/* Use the macros below this section instead of calling  */
/* these directly. The macros have a consistent naming   */
/* convention and will pull in file and line information */
/* for you.                                              */

void UnityAssertEqualNumber(const UNITY_INT expected,
                            const UNITY_INT actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const char* file,
                            const UNITY_DISPLAY_STYLE_T style);

void UnityAssertGreaterOrLessOrEqualNumber(const UNITY_INT threshold,
                                           const UNITY_INT actual,
                                           const UNITY_COMPARISON_T compare,
                                           const char *msg,
                                           const UNITY_LINE_TYPE lineNumber,
                                           const char* file,
                                           const UNITY_DISPLAY_STYLE_T style);

void UnityAssertEqualIntArray(UNITY_INTERNAL_PTR expected,
                              UNITY_INTERNAL_PTR actual,
                              const UNITY_UINT32 num_elements,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const char* file,
                              const UNITY_DISPLAY_STYLE_T style,
                              const UNITY_FLAGS_T flags);

void UnityAssertBits(const UNITY_INT mask,
                     const UNITY_INT expected,
                     const UNITY_INT actual,
                     const char* msg,
                     const UNITY_LINE_TYPE lineNumber,
                     const char* file);

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const char* file);

void UnityAssertEqualStringLen(const char* expected,
                            const char* actual,
                            const UNITY_UINT32 length,
                            const char* msg,
                            const UNITY_LINE_TYPE lineNumber,
                            const char* file);

void UnityAssertEqualStringArray( UNITY_INTERNAL_PTR expected,
                                  const char** actual,
                                  const UNITY_UINT32 num_elements,
                                  const char* msg,
                                  const UNITY_LINE_TYPE lineNumber,
                                  const char* file,
                                  const UNITY_FLAGS_T flags);

void UnityAssertEqualMemory( UNITY_INTERNAL_PTR expected,
                             UNITY_INTERNAL_PTR actual,
                             const UNITY_UINT32 length,
                             const UNITY_UINT32 num_elements,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber,
                             const char* file,
                             const UNITY_FLAGS_T flags);

void UnityAssertNumbersWithin(const UNITY_UINT delta,
                              const UNITY_INT expected,
                              const UNITY_INT actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const char* file,
                              const UNITY_DISPLAY_STYLE_T style);

void UnityFail(const char* message, const UNITY_LINE_TYPE line, const char* file);

void UnityIgnore(const char* message, const UNITY_LINE_TYPE line, const char* file);

#ifndef UNITY_EXCLUDE_FLOAT
void UnityAssertFloatsWithin(const UNITY_FLOAT delta,
                             const UNITY_FLOAT expected,
                             const UNITY_FLOAT actual,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber,
                             const char* file);

void UnityAssertEqualFloatArray(UNITY_PTR_ATTRIBUTE const UNITY_FLOAT* expected,
                                UNITY_PTR_ATTRIBUTE const UNITY_FLOAT* actual,
                                const UNITY_UINT32 num_elements,
                                const char* msg,
                                const UNITY_LINE_TYPE lineNumber,
                                const char* file);

void UnityAssertFloatSpecial(const UNITY_FLOAT actual,
                             const char* msg,
                             const UNITY_LINE_TYPE lineNumber,
                             const char* file,
                             const UNITY_FLOAT_TRAIT_T style);
#endif

#ifndef UNITY_EXCLUDE_DOUBLE
void UnityAssertDoublesWithin(const UNITY_DOUBLE delta,
                              const UNITY_DOUBLE expected,
                              const UNITY_DOUBLE actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const char* file);

void UnityAssertEqualDoubleArray(UNITY_PTR_ATTRIBUTE const UNITY_DOUBLE* expected,
                                 UNITY_PTR_ATTRIBUTE const UNITY_DOUBLE* actual,
                                 const UNITY_UINT32 num_elements,
                                 const char* msg,
                                 const UNITY_LINE_TYPE lineNumber,
                                 const char* file,
                                 const UNITY_FLAGS_T flags);

void UnityAssertDoubleSpecial(const UNITY_DOUBLE actual,
                              const char* msg,
                              const UNITY_LINE_TYPE lineNumber,
                              const UNITY_FLOAT_TRAIT_T style);
#endif

/*-------------------------------------------------------
 * Helpers
 *-------------------------------------------------------*/

UNITY_INTERNAL_PTR UnityNumToPtr(const UNITY_INT num, const UNITY_UINT8 size);
#ifndef UNITY_EXCLUDE_FLOAT
UNITY_INTERNAL_PTR UnityFloatToPtr(const float num);
#endif
#ifndef UNITY_EXCLUDE_DOUBLE
UNITY_INTERNAL_PTR UnityDoubleToPtr(const double num);
#endif

/*-------------------------------------------------------
 * Error Strings We Might Need
 *-------------------------------------------------------*/

extern const char UnityStrErrFloat[];
extern const char UnityStrErrDouble[];
extern const char UnityStrErr64[];

/*-------------------------------------------------------*/
/* Test Running Macros                                   */
/*-------------------------------------------------------*/

#ifndef UNITY_EXCLUDE_SETJMP_H
#define TEST_PROTECT() (setjmp(Unity.AbortFrame) == 0)
#define TEST_ABORT() {longjmp(Unity.AbortFrame, 1);}
#else
#define TEST_PROTECT() (1)
#define TEST_ABORT() {Panic();}
#endif

#ifndef RUN_TEST
/* Using a macro rather than a function avoids the need for a function pointer to call testfunc */
#define RUN_TEST(testfunc) \
{ \
    Unity.CurrentTestName = #testfunc; \
    Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE)(__LINE__); \
    Unity.NumberOfTests++; \
    if (TEST_PROTECT()) \
    { \
        setUp(); \
        testfunc(); \
    } \
    if (TEST_PROTECT()) \
    { \
        tearDown(); \
    } \
    UnityConcludeTest(); \
}
#endif

#define TEST_LINE_NUM (Unity.CurrentTestLineNumber)
#define TEST_FILE_NAME (Unity.TestFile)
#define TEST_IS_IGNORED (Unity.CurrentTestIgnored)
#define UNITY_NEW_TEST(a) \
    Unity.CurrentTestName = (a); \
    Unity.CurrentTestLineNumber = (UNITY_LINE_TYPE)(__LINE__); \
    Unity.NumberOfTests++;

#define UNITY_BEGIN(harness) \
{ \
    UnityBegin(harness); \
    Unity.TestFile = __FILE__; \
}

#define UNITY_END() \
{ \
    UnityEnd(); \
}

/*-----------------------------------------------
 * Command Line Argument Support
 *-----------------------------------------------*/

#ifdef UNITY_USE_COMMAND_LINE_ARGS
int UnityParseOptions(int argc, char** argv);
int UnityTestMatches(void);
#endif

/*-------------------------------------------------------*/
/* Basic Fail and Ignore                                 */
/*-------------------------------------------------------*/

#define UNITY_TEST_FAIL(line, file, message)   UnityFail(   (message), (UNITY_LINE_TYPE)(line), file);
#ifdef HOSTED_TEST_ENVIRONMENT
#define UNITY_TEST_IGNORE(line, message, file) UnityIgnore( (message), (UNITY_LINE_TYPE)(line), file);
#else
#define UNITY_TEST_IGNORE(line, message, file) {UnityIgnore( (message), (UNITY_LINE_TYPE)(line), file); return;}
#endif

/*-------------------------------------------------------*/
/* Test Asserts                                          */
/*-------------------------------------------------------*/

#define UNITY_TEST_ASSERT(condition, line, file, message)                                                    if (condition) {} else {UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, (message));}
#define UNITY_TEST_ASSERT_NULL(pointer, line, file, message)                                                 UNITY_TEST_ASSERT(((pointer) == NULL), (UNITY_LINE_TYPE)(line), file, (message))
#define UNITY_TEST_ASSERT_NOT_NULL(pointer, line, file, message)                                             UNITY_TEST_ASSERT(((pointer) != NULL), (UNITY_LINE_TYPE)(line), file, (message))

/* Integers (of all sizes) */                                                                               
#define UNITY_TEST_ASSERT_EQUAL_INT(expected, actual, line, file, message)                                   UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_EQUAL_INT8(expected, actual, line, file, message)                                  UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )(expected), (UNITY_INT)(UNITY_INT8 )(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_EQUAL_INT16(expected, actual, line, file, message)                                 UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)(expected), (UNITY_INT)(UNITY_INT16)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_EQUAL_INT32(expected, actual, line, file, message)                                 UnityAssertEqualNumber((UNITY_INT)(UNITY_INT32)(expected), (UNITY_INT)(UNITY_INT32)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_EQUAL_UINT(expected, actual, line, file, message)                                  UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_EQUAL_UINT8(expected, actual, line, file, message)                                 UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )(expected), (UNITY_INT)(UNITY_UINT8 )(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_EQUAL_UINT16(expected, actual, line, file, message)                                UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT16)(expected), (UNITY_INT)(UNITY_UINT16)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_EQUAL_UINT32(expected, actual, line, file, message)                                UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)(expected), (UNITY_INT)(UNITY_UINT32)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_EQUAL_HEX8(expected, actual, line, file, message)                                  UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )(expected), (UNITY_INT)(UNITY_INT8 )(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_EQUAL_HEX16(expected, actual, line, file, message)                                 UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)(expected), (UNITY_INT)(UNITY_INT16)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_EQUAL_HEX32(expected, actual, line, file, message)                                 UnityAssertEqualNumber((UNITY_INT)(UNITY_INT32)(expected), (UNITY_INT)(UNITY_INT32)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32)
#define UNITY_TEST_ASSERT_BITS(mask, expected, actual, line, file, message)                                  UnityAssertBits((UNITY_INT)(mask), (UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file)
                                                                                                            
#define UNITY_TEST_ASSERT_GREATER_THAN_INT(threshold, actual, line, file, message)                           UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_GREATER_THAN_INT8(threshold, actual, line, file, message)                          UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT8 )(threshold), (UNITY_INT)(UNITY_INT8 )(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_GREATER_THAN_INT16(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT16)(threshold), (UNITY_INT)(UNITY_INT16)(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_GREATER_THAN_INT32(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT32)(threshold), (UNITY_INT)(UNITY_INT32)(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT(threshold, actual, line, file, message)                          UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT8(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT16(threshold, actual, line, file, message)                        UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT32(threshold, actual, line, file, message)                        UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX8(threshold, actual, line, file, message)                          UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX16(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX32(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_GREATER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32)
                                                                                                            
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT(threshold, actual, line, file, message)                           UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT8(threshold, actual, line, file, message)                          UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT8 )(threshold), (UNITY_INT)(UNITY_INT8 )(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT16(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT16)(threshold), (UNITY_INT)(UNITY_INT16)(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT32(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT32)(threshold), (UNITY_INT)(UNITY_INT32)(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT(threshold, actual, line, file, message)                          UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT8(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT16(threshold, actual, line, file, message)                        UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT32(threshold, actual, line, file, message)                        UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX8(threshold, actual, line, file, message)                          UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX16(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX32(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_SMALLER_THAN, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32)
                                                                                                            
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT(threshold, actual, line, file, message)                       UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT8(threshold, actual, line, file, message)                      UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT8 )(threshold), (UNITY_INT)(UNITY_INT8 )(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT16(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT16)(threshold), (UNITY_INT)(UNITY_INT16)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT32(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT32)(threshold), (UNITY_INT)(UNITY_INT32)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT(threshold, actual, line, file, message)                      UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT8(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT16(threshold, actual, line, file, message)                    UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT32(threshold, actual, line, file, message)                    UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX8(threshold, actual, line, file, message)                      UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX16(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX32(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32)
                                                                                                            
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT(threshold, actual, line, file, message)                       UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT8(threshold, actual, line, file, message)                      UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT8 )(threshold), (UNITY_INT)(UNITY_INT8 )(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT16(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT16)(threshold), (UNITY_INT)(UNITY_INT16)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT32(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_INT32)(threshold), (UNITY_INT)(UNITY_INT32)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT(threshold, actual, line, file, message)                      UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold),              (UNITY_INT)(actual),              UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT8(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT16(threshold, actual, line, file, message)                    UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT32(threshold, actual, line, file, message)                    UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX8(threshold, actual, line, file, message)                      UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT8 )(threshold), (UNITY_INT)(UNITY_UINT8 )(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX16(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT16)(threshold), (UNITY_INT)(UNITY_UINT16)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX32(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(UNITY_UINT32)(threshold), (UNITY_INT)(UNITY_UINT32)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32)
                                                                                                            
#define UNITY_TEST_ASSERT_INT_WITHIN(delta, expected, actual, line, file, message)                           UnityAssertNumbersWithin((delta), (UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT)
#define UNITY_TEST_ASSERT_INT8_WITHIN(delta, expected, actual, line, file, message)                          UnityAssertNumbersWithin((UNITY_UINT8 )(delta), (UNITY_INT)(UNITY_INT8 )(expected), (UNITY_INT)(UNITY_INT8 )(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8)
#define UNITY_TEST_ASSERT_INT16_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertNumbersWithin((UNITY_UINT16)(delta), (UNITY_INT)(UNITY_INT16)(expected), (UNITY_INT)(UNITY_INT16)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16)
#define UNITY_TEST_ASSERT_INT32_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertNumbersWithin((UNITY_UINT32)(delta), (UNITY_INT)(UNITY_INT32)(expected), (UNITY_INT)(UNITY_INT32)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32)
#define UNITY_TEST_ASSERT_UINT_WITHIN(delta, expected, actual, line, file, message)                          UnityAssertNumbersWithin((delta), (UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT)
#define UNITY_TEST_ASSERT_UINT8_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertNumbersWithin((UNITY_UINT8 )(delta), (UNITY_INT)(UNITY_UINT)(UNITY_UINT8 )(expected), (UNITY_INT)(UNITY_UINT)(UNITY_UINT8 )(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8)
#define UNITY_TEST_ASSERT_UINT16_WITHIN(delta, expected, actual, line, file, message)                        UnityAssertNumbersWithin((UNITY_UINT16)(delta), (UNITY_INT)(UNITY_UINT)(UNITY_UINT16)(expected), (UNITY_INT)(UNITY_UINT)(UNITY_UINT16)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16)
#define UNITY_TEST_ASSERT_UINT32_WITHIN(delta, expected, actual, line, file, message)                        UnityAssertNumbersWithin((UNITY_UINT32)(delta), (UNITY_INT)(UNITY_UINT)(UNITY_UINT32)(expected), (UNITY_INT)(UNITY_UINT)(UNITY_UINT32)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32)
#define UNITY_TEST_ASSERT_HEX8_WITHIN(delta, expected, actual, line, file, message)                          UnityAssertNumbersWithin((UNITY_UINT8 )(delta), (UNITY_INT)(UNITY_UINT)(UNITY_UINT8 )(expected), (UNITY_INT)(UNITY_UINT)(UNITY_UINT8 )(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8)
#define UNITY_TEST_ASSERT_HEX16_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertNumbersWithin((UNITY_UINT16)(delta), (UNITY_INT)(UNITY_UINT)(UNITY_UINT16)(expected), (UNITY_INT)(UNITY_UINT)(UNITY_UINT16)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16)
#define UNITY_TEST_ASSERT_HEX32_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertNumbersWithin((UNITY_UINT32)(delta), (UNITY_INT)(UNITY_UINT)(UNITY_UINT32)(expected), (UNITY_INT)(UNITY_UINT)(UNITY_UINT32)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32)
                                                                                                            
#define UNITY_TEST_ASSERT_EQUAL_PTR(expected, actual, line, file, message)                                   UnityAssertEqualNumber((UNITY_PTR_TO_INT)(expected), (UNITY_PTR_TO_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_POINTER)
#define UNITY_TEST_ASSERT_EQUAL_STRING(expected, actual, line, file, message)                                UnityAssertEqualString((const char*)(expected), (const char*)(actual), (message), (UNITY_LINE_TYPE)(line), file)
#define UNITY_TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len, line, file, message)                       UnityAssertEqualStringLen((const char*)(expected), (const char*)(actual), (UNITY_UINT32)(len), (message), (UNITY_LINE_TYPE)(line), file)
#define UNITY_TEST_ASSERT_EQUAL_MEMORY(expected, actual, len, line, file, message)                           UnityAssertEqualMemory((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(len), 1, (message), (UNITY_LINE_TYPE)(line), file, UNITY_ARRAY_TO_ARRAY)
                                                                                                            
#define UNITY_TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements, line, file, message)               UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT,     UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_INT8_ARRAY(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8,    UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_INT16_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16,   UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_INT32_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32,   UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_UINT_ARRAY(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT,    UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8,   UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual, num_elements, line, file, message)            UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16,  UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, actual, num_elements, line, file, message)            UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32,  UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8,    UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16,   UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32,   UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_PTR_ARRAY(expected, actual, num_elements, line, file, message)               UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_POINTER, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num_elements, line, file, message)            UnityAssertEqualStringArray((UNITY_INTERNAL_PTR)(expected), (const char**)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, len, num_elements, line, file, message)       UnityAssertEqualMemory((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(len), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_ARRAY_TO_ARRAY)
                                                                                                            
#define UNITY_TEST_ASSERT_EACH_EQUAL_INT(expected, actual, num_elements, line, file, message)                UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)              expected, sizeof(int)),  (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT,     UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_INT8(expected, actual, num_elements, line, file, message)               UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT8  )expected, 1),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT8,    UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_INT16(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT16 )expected, 2),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT16,   UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_INT32(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT32 )expected, 4),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT32,   UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_UINT(expected, actual, num_elements, line, file, message)               UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)              expected, sizeof(unsigned int)), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT,    UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_UINT8(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_UINT8 )expected, 1),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT8,   UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_UINT16(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_UINT16)expected, 2),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT16,  UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_UINT32(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_UINT32)expected, 4),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT32,  UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_HEX8(expected, actual, num_elements, line, file, message)               UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT8  )expected, 1),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX8,    UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_HEX16(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT16 )expected, 2),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX16,   UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_HEX32(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT32 )expected, 4),            (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX32,   UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_PTR(expected, actual, num_elements, line, file, message)                UnityAssertEqualIntArray(UnityNumToPtr((UNITY_PTR_TO_INT)       expected, sizeof(int*)), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_POINTER, UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_STRING(expected, actual, num_elements, line, file, message)             UnityAssertEqualStringArray((UNITY_INTERNAL_PTR)(expected), (const char**)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_MEMORY(expected, actual, len, num_elements, line, file, message)        UnityAssertEqualMemory((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(len), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_ARRAY_TO_VAL)
                                                                                                            
#ifdef UNITY_SUPPORT_64                                                                                     
#define UNITY_TEST_ASSERT_EQUAL_INT64(expected, actual, line, file, message)                                 UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_EQUAL_UINT64(expected, actual, line, file, message)                                UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_EQUAL_HEX64(expected, actual, line, file, message)                                 UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64)
#define UNITY_TEST_ASSERT_EQUAL_INT64_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64,  UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_UINT64_ARRAY(expected, actual, num_elements, line, file, message)            UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EQUAL_HEX64_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray((UNITY_INTERNAL_PTR)(expected), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64,  UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EACH_EQUAL_INT64(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT64)expected, 8), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64,  UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_UINT64(expected, actual, num_elements, line, file, message)             UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_UINT64)expected, 8), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64, UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_EACH_EQUAL_HEX64(expected, actual, num_elements, line, file, message)              UnityAssertEqualIntArray(UnityNumToPtr((UNITY_INT)(UNITY_INT64)expected, 8), (UNITY_INTERNAL_PTR)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64,  UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_INT64_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertNumbersWithin((delta), (UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_UINT64_WITHIN(delta, expected, actual, line, file, message)                        UnityAssertNumbersWithin((delta), (UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_HEX64_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertNumbersWithin((delta), (UNITY_INT)(expected), (UNITY_INT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64)
#define UNITY_TEST_ASSERT_GREATER_THAN_INT64(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_GREATER_THAN,     (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT64(threshold, actual, line, file, message)                        UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_GREATER_THAN,     (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX64(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_GREATER_THAN,     (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT64(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT64(threshold, actual, line, file, message)                    UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX64(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_GREATER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64)
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT64(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_SMALLER_THAN,     (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT64(threshold, actual, line, file, message)                        UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_SMALLER_THAN,     (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX64(threshold, actual, line, file, message)                         UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_SMALLER_THAN,     (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT64(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_INT64)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT64(threshold, actual, line, file, message)                    UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_UINT64)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX64(threshold, actual, line, file, message)                     UnityAssertGreaterOrLessOrEqualNumber((UNITY_INT)(threshold), (UNITY_INT)(actual), UNITY_SMALLER_OR_EQUAL, (message), (UNITY_LINE_TYPE)(line), file, UNITY_DISPLAY_STYLE_HEX64)
#else                                                                                                       
#define UNITY_TEST_ASSERT_EQUAL_INT64(expected, actual, line, file, message)                                 UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_EQUAL_UINT64(expected, actual, line, file, message)                                UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_EQUAL_HEX64(expected, actual, line, file, message)                                 UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_EQUAL_INT64_ARRAY(expected, actual, num_elements, line, file, message)             UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_EQUAL_UINT64_ARRAY(expected, actual, num_elements, line, file, message)            UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_EQUAL_HEX64_ARRAY(expected, actual, num_elements, line, file, message)             UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_INT64_WITHIN(delta, expected, actual, line, file, message)                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_UINT64_WITHIN(delta, expected, actual, line, file, message)                        UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_HEX64_WITHIN(delta, expected, actual, line, file, message)                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_GREATER_THAN_INT64(threshold, actual, line, file, message)                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_GREATER_THAN_UINT64(threshold, actual, line, file, message)                        UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_GREATER_THAN_HEX64(threshold, actual, line, file, message)                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT64(threshold, actual, line, file, message)                     UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT64(threshold, actual, line, file, message)                    UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX64(threshold, actual, line, file, message)                     UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_SMALLER_THAN_INT64(threshold, actual, line, file, message)                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_SMALLER_THAN_UINT64(threshold, actual, line, file, message)                        UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_SMALLER_THAN_HEX64(threshold, actual, line, file, message)                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT64(threshold, actual, line, file, message)                     UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT64(threshold, actual, line, file, message)                    UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#define UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX64(threshold, actual, line, file, message)                     UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErr64)
#endif                                                                                                      
                                                                                                            
#ifdef UNITY_EXCLUDE_FLOAT                                                                                  
#define UNITY_TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual, line, file, message)                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_EQUAL_FLOAT(expected, actual, line, file, message)                                 UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected, actual, num_elements, line, file, message)             UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_EACH_EQUAL_FLOAT(expected, actual, num_elements, line, file, message)              UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_INF(actual, line, file, message)                                          UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_NEG_INF(actual, line, file, message)                                      UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_NAN(actual, line, file, message)                                          UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_DETERMINATE(actual, line, file, message)                                  UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_INF(actual, line, file, message)                                      UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_NEG_INF(actual, line, file, message)                                  UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_NAN(actual, line, file, message)                                      UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE(actual, line, file, message)                              UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrFloat)
#else                                                                                                       
#define UNITY_TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual, line, file, message)                         UnityAssertFloatsWithin((UNITY_FLOAT)(delta), (UNITY_FLOAT)(expected), (UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file)
#define UNITY_TEST_ASSERT_EQUAL_FLOAT(expected, actual, line, file, message)                                 UNITY_TEST_ASSERT_FLOAT_WITHIN((UNITY_FLOAT)(expected) * (UNITY_FLOAT)UNITY_FLOAT_PRECISION, (UNITY_FLOAT)(expected), (UNITY_FLOAT)(actual), (UNITY_LINE_TYPE)(line), file, (message))
#define UNITY_TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected, actual, num_elements, line, file, message)             UnityAssertEqualFloatArray((UNITY_FLOAT*)(expected), (UNITY_FLOAT*)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EACH_EQUAL_FLOAT(expected, actual, num_elements, line, file, message)              UnityAssertEqualFloatArray(UnityFloatToPtr(expected), (UNITY_FLOAT*)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)(line), file, UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_FLOAT_IS_INF(actual, line, file, message)                                          UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_INF)
#define UNITY_TEST_ASSERT_FLOAT_IS_NEG_INF(actual, line, file, message)                                      UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NEG_INF)
#define UNITY_TEST_ASSERT_FLOAT_IS_NAN(actual, line, file, message)                                          UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NAN)
#define UNITY_TEST_ASSERT_FLOAT_IS_DETERMINATE(actual, line, file, message)                                  UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_DET)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_INF(actual, line, file, message)                                      UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_INF)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_NEG_INF(actual, line, file, message)                                  UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_NEG_INF)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_NAN(actual, line, file, message)                                      UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_NAN)
#define UNITY_TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE(actual, line, file, message)                              UnityAssertFloatSpecial((UNITY_FLOAT)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_DET)
#endif                                                                                                      
                                                                                                            
#ifdef UNITY_EXCLUDE_DOUBLE                                                                                 
#define UNITY_TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual, line, file, message)                        UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE(expected, actual, line, file, message)                                UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE_ARRAY(expected, actual, num_elements, line, file, message)            UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_EACH_EQUAL_DOUBLE(expected, actual, num_elements, line, file, message)             UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_INF(actual, line, file, message)                                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NEG_INF(actual, line, file, message)                                     UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NAN(actual, line, file, message)                                         UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_DETERMINATE(actual, line, file, message)                                 UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_INF(actual, line, file, message)                                     UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF(actual, line, file, message)                                 UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NAN(actual, line, file, message)                                     UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE(actual, line, file, message)                             UNITY_TEST_FAIL((UNITY_LINE_TYPE)(line), file, UnityStrErrDouble)
#else                                                                                                       
#define UNITY_TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual, line, file, message)                        UnityAssertDoublesWithin((UNITY_DOUBLE)(delta), (UNITY_DOUBLE)(expected), (UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)line)
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE(expected, actual, line, file, message)                                UNITY_TEST_ASSERT_DOUBLE_WITHIN((UNITY_DOUBLE)(expected) * (UNITY_DOUBLE)UNITY_DOUBLE_PRECISION, (UNITY_DOUBLE)expected, (UNITY_DOUBLE)actual, (UNITY_LINE_TYPE)(line), file, message)
#define UNITY_TEST_ASSERT_EQUAL_DOUBLE_ARRAY(expected, actual, num_elements, line, file, message)            UnityAssertEqualDoubleArray((UNITY_DOUBLE*)(expected), (UNITY_DOUBLE*)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_ARRAY_TO_ARRAY)
#define UNITY_TEST_ASSERT_EACH_EQUAL_DOUBLE(expected, actual, num_elements, line, file, message)             UnityAssertEqualDoubleArray(UnityDoubleToPtr(expected), (UNITY_DOUBLE*)(actual), (UNITY_UINT32)(num_elements), (message), (UNITY_LINE_TYPE)line, UNITY_ARRAY_TO_VAL)
#define UNITY_TEST_ASSERT_DOUBLE_IS_INF(actual, line, file, message)                                         UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_INF)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NEG_INF(actual, line, file, message)                                     UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NEG_INF)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NAN(actual, line, file, message)                                         UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NAN)
#define UNITY_TEST_ASSERT_DOUBLE_IS_DETERMINATE(actual, line, file, message)                                 UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_DET)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_INF(actual, line, file, message)                                     UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_INF)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF(actual, line, file, message)                                 UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_NEG_INF)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NAN(actual, line, file, message)                                     UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_NAN)
#define UNITY_TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE(actual, line, file, message)                             UnityAssertDoubleSpecial((UNITY_DOUBLE)(actual), (message), (UNITY_LINE_TYPE)(line), file, UNITY_FLOAT_IS_NOT_DET)
#endif

/*--------------------------------------------------------------------------------------------------------------*/
/* unity.h                                                                                                      */
/*--------------------------------------------------------------------------------------------------------------*/


/*-------------------------------------------------------*/
/* Basic Fail and Ignore                                 */
/*-------------------------------------------------------*/

#define TEST_FAIL_MESSAGE(message)                                                                 UNITY_TEST_FAIL(__LINE__, __FILE__, (message))
#define TEST_FAIL()                                                                                UNITY_TEST_FAIL(__LINE__, __FILE__, NULL)
#define TEST_IGNORE_MESSAGE(message)                                                               UNITY_TEST_IGNORE(__LINE__, __FILE__, (message))
#define TEST_IGNORE()                                                                              UNITY_TEST_IGNORE(__LINE__, __FILE__, NULL)
#define TEST_ONLY()

/* It is not necessary for you to call PASS. A PASS condition is assumed if nothing fails.
 * This method allows you to abort a test immediately with a PASS state, ignoring the remainder of the test. */
#define TEST_PASS()                                                                                TEST_ABORT()

/* This macro does nothing, but it is useful for build tools (like Ceedling) to make use of this to figure out
 * which files should be linked to in order to perform a test. Use it like TEST_FILE("sandwiches.c") */
#define TEST_FILE(a)

/*-------------------------------------------------------*/
/* Test Asserts (simple)                                 */
/*-------------------------------------------------------*/

/* Boolean */
#define TEST_ASSERT(condition)                                                                     UNITY_TEST_ASSERT((condition), __LINE__, __FILE__, " Expression Evaluated To FALSE")
#define TEST_ASSERT_TRUE(condition)                                                                UNITY_TEST_ASSERT((condition), __LINE__, __FILE__, " Expected TRUE Was FALSE")
#define TEST_ASSERT_UNLESS(condition)                                                              UNITY_TEST_ASSERT(!(condition), __LINE__, __FILE__, " Expression Evaluated To TRUE")
#define TEST_ASSERT_FALSE(condition)                                                               UNITY_TEST_ASSERT(!(condition), __LINE__, __FILE__, " Expected FALSE Was TRUE")
#define TEST_ASSERT_NULL(pointer)                                                                  UNITY_TEST_ASSERT_NULL((pointer), __LINE__, __FILE__, " Expected NULL")
#define TEST_ASSERT_NOT_NULL(pointer)                                                              UNITY_TEST_ASSERT_NOT_NULL((pointer), __LINE__, __FILE__, " Expected Non-NULL")

/* Integers (of all sizes) */
#define TEST_ASSERT_EQUAL_INT(expected, actual)                                                    UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT8(expected, actual)                                                   UNITY_TEST_ASSERT_EQUAL_INT8((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT16(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_INT16((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT32(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_INT32((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT64(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_INT64((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL(expected, actual)                                                        UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_NOT_EQUAL(expected, actual)                                                    UNITY_TEST_ASSERT(((expected) != (actual)), __LINE__, __FILE__, " Expected Not-Equal")
#define TEST_ASSERT_EQUAL_UINT(expected, actual)                                                   UNITY_TEST_ASSERT_EQUAL_UINT( (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT8(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_UINT8( (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT16(expected, actual)                                                 UNITY_TEST_ASSERT_EQUAL_UINT16( (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                                                 UNITY_TEST_ASSERT_EQUAL_UINT32( (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT64(expected, actual)                                                 UNITY_TEST_ASSERT_EQUAL_UINT64( (expected), (actual), __LINE__, __FILE__, NULL)
#ifdef UNITY_SUPPORT_64
#define TEST_ASSERT_EQUAL_HEX(expected, actual)                                                    UNITY_TEST_ASSERT_EQUAL_HEX64((expected), (actual), __LINE__, __FILE__, NULL)
#else
#define TEST_ASSERT_EQUAL_HEX(expected, actual)                                                    UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, __FILE__, NULL)
#endif /* def UNITY_SUPPORT_64 */
#define TEST_ASSERT_EQUAL_HEX8(expected, actual)                                                   UNITY_TEST_ASSERT_EQUAL_HEX8( (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX16(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_HEX16((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX32(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX64(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_HEX64((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_BITS(mask, expected, actual)                                                   UNITY_TEST_ASSERT_BITS((mask), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_BITS_HIGH(mask, actual)                                                        UNITY_TEST_ASSERT_BITS((mask), (UNITY_UINT32)(-1), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_BITS_LOW(mask, actual)                                                         UNITY_TEST_ASSERT_BITS((mask), (UNITY_UINT32)(0), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_BIT_HIGH(bit, actual)                                                          UNITY_TEST_ASSERT_BITS(((UNITY_UINT32)1 << (bit)), (UNITY_UINT32)(-1), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_BIT_LOW(bit, actual)                                                           UNITY_TEST_ASSERT_BITS(((UNITY_UINT32)1 << (bit)), (UNITY_UINT32)(0), (actual), __LINE__, __FILE__, NULL)

/* Integer Greater Than/ Less Than (of all sizes) */
#define TEST_ASSERT_GREATER_THAN(threshold, actual)                                                UNITY_TEST_ASSERT_GREATER_THAN_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_INT(threshold, actual)                                            UNITY_TEST_ASSERT_GREATER_THAN_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_INT8(threshold, actual)                                           UNITY_TEST_ASSERT_GREATER_THAN_INT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_INT16(threshold, actual)                                          UNITY_TEST_ASSERT_GREATER_THAN_INT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_INT32(threshold, actual)                                          UNITY_TEST_ASSERT_GREATER_THAN_INT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_INT64(threshold, actual)                                          UNITY_TEST_ASSERT_GREATER_THAN_INT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_UINT(threshold, actual)                                           UNITY_TEST_ASSERT_GREATER_THAN_UINT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_UINT8(threshold, actual)                                          UNITY_TEST_ASSERT_GREATER_THAN_UINT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_UINT16(threshold, actual)                                         UNITY_TEST_ASSERT_GREATER_THAN_UINT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_UINT32(threshold, actual)                                         UNITY_TEST_ASSERT_GREATER_THAN_UINT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_UINT64(threshold, actual)                                         UNITY_TEST_ASSERT_GREATER_THAN_UINT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_HEX8(threshold, actual)                                           UNITY_TEST_ASSERT_GREATER_THAN_HEX8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_HEX16(threshold, actual)                                          UNITY_TEST_ASSERT_GREATER_THAN_HEX16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_HEX32(threshold, actual)                                          UNITY_TEST_ASSERT_GREATER_THAN_HEX32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_THAN_HEX64(threshold, actual)                                          UNITY_TEST_ASSERT_GREATER_THAN_HEX64((threshold), (actual), __LINE__, __FILE__, NULL)

#define TEST_ASSERT_LESS_THAN(threshold, actual)                                                   UNITY_TEST_ASSERT_SMALLER_THAN_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_INT(threshold, actual)                                               UNITY_TEST_ASSERT_SMALLER_THAN_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_INT8(threshold, actual)                                              UNITY_TEST_ASSERT_SMALLER_THAN_INT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_INT16(threshold, actual)                                             UNITY_TEST_ASSERT_SMALLER_THAN_INT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_INT32(threshold, actual)                                             UNITY_TEST_ASSERT_SMALLER_THAN_INT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_INT64(threshold, actual)                                             UNITY_TEST_ASSERT_SMALLER_THAN_INT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_UINT(threshold, actual)                                              UNITY_TEST_ASSERT_SMALLER_THAN_UINT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_UINT8(threshold, actual)                                             UNITY_TEST_ASSERT_SMALLER_THAN_UINT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_UINT16(threshold, actual)                                            UNITY_TEST_ASSERT_SMALLER_THAN_UINT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_UINT32(threshold, actual)                                            UNITY_TEST_ASSERT_SMALLER_THAN_UINT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_UINT64(threshold, actual)                                            UNITY_TEST_ASSERT_SMALLER_THAN_UINT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_HEX8(threshold, actual)                                              UNITY_TEST_ASSERT_SMALLER_THAN_HEX8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_HEX16(threshold, actual)                                             UNITY_TEST_ASSERT_SMALLER_THAN_HEX16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_HEX32(threshold, actual)                                             UNITY_TEST_ASSERT_SMALLER_THAN_HEX32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_THAN_HEX64(threshold, actual)                                             UNITY_TEST_ASSERT_SMALLER_THAN_HEX64((threshold), (actual), __LINE__, __FILE__, NULL)

#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)                                            UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_INT(threshold, actual)                                        UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_INT8(threshold, actual)                                       UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_INT16(threshold, actual)                                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_INT32(threshold, actual)                                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_INT64(threshold, actual)                                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT(threshold, actual)                                       UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT8(threshold, actual)                                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT16(threshold, actual)                                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT32(threshold, actual)                                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT64(threshold, actual)                                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX8(threshold, actual)                                       UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX16(threshold, actual)                                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX32(threshold, actual)                                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX64(threshold, actual)                                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX64((threshold), (actual), __LINE__, __FILE__, NULL)

#define TEST_ASSERT_LESS_OR_EQUAL(threshold, actual)                                               UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_INT(threshold, actual)                                           UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_INT8(threshold, actual)                                          UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_INT16(threshold, actual)                                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_INT32(threshold, actual)                                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_INT64(threshold, actual)                                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_UINT(threshold, actual)                                          UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_UINT8(threshold, actual)                                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_UINT16(threshold, actual)                                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_UINT32(threshold, actual)                                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_UINT64(threshold, actual)                                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT64((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_HEX8(threshold, actual)                                          UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX8((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_HEX16(threshold, actual)                                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX16((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_HEX32(threshold, actual)                                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX32((threshold), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_LESS_OR_EQUAL_HEX64(threshold, actual)                                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX64((threshold), (actual), __LINE__, __FILE__, NULL)

/* Integer Ranges (of all sizes) */
#define TEST_ASSERT_INT_WITHIN(delta, expected, actual)                                            UNITY_TEST_ASSERT_INT_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_INT8_WITHIN(delta, expected, actual)                                           UNITY_TEST_ASSERT_INT8_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_INT16_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_INT16_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_INT32_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_INT32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_INT64_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_INT64_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_UINT_WITHIN(delta, expected, actual)                                           UNITY_TEST_ASSERT_UINT_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_UINT8_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_UINT8_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_UINT16_WITHIN(delta, expected, actual)                                         UNITY_TEST_ASSERT_UINT16_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_UINT32_WITHIN(delta, expected, actual)                                         UNITY_TEST_ASSERT_UINT32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_UINT64_WITHIN(delta, expected, actual)                                         UNITY_TEST_ASSERT_UINT64_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_HEX_WITHIN(delta, expected, actual)                                            UNITY_TEST_ASSERT_HEX32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_HEX8_WITHIN(delta, expected, actual)                                           UNITY_TEST_ASSERT_HEX8_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_HEX16_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_HEX16_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_HEX32_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_HEX32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_HEX64_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_HEX64_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)

/* Structs and Strings */
#define TEST_ASSERT_EQUAL_PTR(expected, actual)                                                    UNITY_TEST_ASSERT_EQUAL_PTR((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_STRING(expected, actual)                                                 UNITY_TEST_ASSERT_EQUAL_STRING((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len)                                        UNITY_TEST_ASSERT_EQUAL_STRING_LEN((expected), (actual), (len), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)                                            UNITY_TEST_ASSERT_EQUAL_MEMORY((expected), (actual), (len), __LINE__, __FILE__, NULL)

/* Arrays */
#define TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements)                                UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT8_ARRAY(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EQUAL_INT8_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT16_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_INT16_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT32_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_INT32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_INT64_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_INT64_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT_ARRAY(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EQUAL_UINT_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_UINT8_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT16_ARRAY(expected, actual, num_elements)                             UNITY_TEST_ASSERT_EQUAL_UINT16_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT32_ARRAY(expected, actual, num_elements)                             UNITY_TEST_ASSERT_EQUAL_UINT32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_UINT64_ARRAY(expected, actual, num_elements)                             UNITY_TEST_ASSERT_EQUAL_UINT64_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX_ARRAY(expected, actual, num_elements)                                UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_HEX16_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_HEX64_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_HEX64_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_PTR_ARRAY(expected, actual, num_elements)                                UNITY_TEST_ASSERT_EQUAL_PTR_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_STRING_ARRAY(expected, actual, num_elements)                             UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_MEMORY_ARRAY(expected, actual, len, num_elements)                        UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY((expected), (actual), (len), (num_elements), __LINE__, __FILE__, NULL)

/* Arrays Compared To Single Value */
#define TEST_ASSERT_EACH_EQUAL_INT(expected, actual, num_elements)                                 UNITY_TEST_ASSERT_EACH_EQUAL_INT((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_INT8(expected, actual, num_elements)                                UNITY_TEST_ASSERT_EACH_EQUAL_INT8((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_INT16(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_INT16((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_INT32(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_INT32((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_INT64(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_INT64((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_UINT(expected, actual, num_elements)                                UNITY_TEST_ASSERT_EACH_EQUAL_UINT((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_UINT8(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_UINT8((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_UINT16(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EACH_EQUAL_UINT16((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_UINT32(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EACH_EQUAL_UINT32((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_UINT64(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EACH_EQUAL_UINT64((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_HEX(expected, actual, num_elements)                                 UNITY_TEST_ASSERT_EACH_EQUAL_HEX32((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_HEX8(expected, actual, num_elements)                                UNITY_TEST_ASSERT_EACH_EQUAL_HEX8((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_HEX16(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_HEX16((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_HEX32(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_HEX32((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_HEX64(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_HEX64((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_PTR(expected, actual, num_elements)                                 UNITY_TEST_ASSERT_EACH_EQUAL_PTR((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_STRING(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EACH_EQUAL_STRING((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_MEMORY(expected, actual, len, num_elements)                         UNITY_TEST_ASSERT_EACH_EQUAL_MEMORY((expected), (actual), (len), (num_elements), __LINE__, __FILE__, NULL)

/* Floating Point (If Enabled) */
#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)                                          UNITY_TEST_ASSERT_FLOAT_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_FLOAT(expected, actual)                                                  UNITY_TEST_ASSERT_EQUAL_FLOAT((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EQUAL_FLOAT_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_FLOAT(expected, actual, num_elements)                               UNITY_TEST_ASSERT_EACH_EQUAL_FLOAT((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_INF(actual)                                                           UNITY_TEST_ASSERT_FLOAT_IS_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_NEG_INF(actual)                                                       UNITY_TEST_ASSERT_FLOAT_IS_NEG_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_NAN(actual)                                                           UNITY_TEST_ASSERT_FLOAT_IS_NAN((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_DETERMINATE(actual)                                                   UNITY_TEST_ASSERT_FLOAT_IS_DETERMINATE((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_NOT_INF(actual)                                                       UNITY_TEST_ASSERT_FLOAT_IS_NOT_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_NOT_NEG_INF(actual)                                                   UNITY_TEST_ASSERT_FLOAT_IS_NOT_NEG_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_NOT_NAN(actual)                                                       UNITY_TEST_ASSERT_FLOAT_IS_NOT_NAN((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE(actual)                                               UNITY_TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE((actual), __LINE__, __FILE__, NULL)

/* Double (If Enabled) */
#define TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)                                         UNITY_TEST_ASSERT_DOUBLE_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_DOUBLE(expected, actual)                                                 UNITY_TEST_ASSERT_EQUAL_DOUBLE((expected), (actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EQUAL_DOUBLE_ARRAY(expected, actual, num_elements)                             UNITY_TEST_ASSERT_EQUAL_DOUBLE_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_EACH_EQUAL_DOUBLE(expected, actual, num_elements)                              UNITY_TEST_ASSERT_EACH_EQUAL_DOUBLE((expected), (actual), (num_elements), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_INF(actual)                                                          UNITY_TEST_ASSERT_DOUBLE_IS_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_NEG_INF(actual)                                                      UNITY_TEST_ASSERT_DOUBLE_IS_NEG_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_NAN(actual)                                                          UNITY_TEST_ASSERT_DOUBLE_IS_NAN((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_DETERMINATE(actual)                                                  UNITY_TEST_ASSERT_DOUBLE_IS_DETERMINATE((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_NOT_INF(actual)                                                      UNITY_TEST_ASSERT_DOUBLE_IS_NOT_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF(actual)                                                  UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_NOT_NAN(actual)                                                      UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NAN((actual), __LINE__, __FILE__, NULL)
#define TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE(actual)                                              UNITY_TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE((actual), __LINE__, __FILE__, NULL)

/*-------------------------------------------------------*/
/* Test Asserts (with additional messages)               */
/*-------------------------------------------------------*/

/* Boolean */
#define TEST_ASSERT_MESSAGE(condition, message)                                                    UNITY_TEST_ASSERT((condition), __LINE__, __FILE__, (message))
#define TEST_ASSERT_TRUE_MESSAGE(condition, message)                                               UNITY_TEST_ASSERT((condition), __LINE__, __FILE__, (message))
#define TEST_ASSERT_UNLESS_MESSAGE(condition, message)                                             UNITY_TEST_ASSERT(!(condition), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FALSE_MESSAGE(condition, message)                                              UNITY_TEST_ASSERT(!(condition), __LINE__, __FILE__, (message))
#define TEST_ASSERT_NULL_MESSAGE(pointer, message)                                                 UNITY_TEST_ASSERT_NULL((pointer), __LINE__, __FILE__, (message))
#define TEST_ASSERT_NOT_NULL_MESSAGE(pointer, message)                                             UNITY_TEST_ASSERT_NOT_NULL((pointer), __LINE__, __FILE__, (message))

/* Integers (of all sizes) */
#define TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, message)                                   UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT8_MESSAGE(expected, actual, message)                                  UNITY_TEST_ASSERT_EQUAL_INT8((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT16_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_INT16((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT32_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_INT32((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT64_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_INT64((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_MESSAGE(expected, actual, message)                                       UNITY_TEST_ASSERT_EQUAL_INT((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_NOT_EQUAL_MESSAGE(expected, actual, message)                                   UNITY_TEST_ASSERT(((expected) != (actual)), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT_MESSAGE(expected, actual, message)                                  UNITY_TEST_ASSERT_EQUAL_UINT( (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT8_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_UINT8( (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT16_MESSAGE(expected, actual, message)                                UNITY_TEST_ASSERT_EQUAL_UINT16( (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, actual, message)                                UNITY_TEST_ASSERT_EQUAL_UINT32( (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT64_MESSAGE(expected, actual, message)                                UNITY_TEST_ASSERT_EQUAL_UINT64( (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX_MESSAGE(expected, actual, message)                                   UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected, actual, message)                                  UNITY_TEST_ASSERT_EQUAL_HEX8( (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX16_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_HEX16((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX32_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_HEX32((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX64_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_HEX64((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_BITS_MESSAGE(mask, expected, actual, message)                                  UNITY_TEST_ASSERT_BITS((mask), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_BITS_HIGH_MESSAGE(mask, actual, message)                                       UNITY_TEST_ASSERT_BITS((mask), (UNITY_UINT32)(-1), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_BITS_LOW_MESSAGE(mask, actual, message)                                        UNITY_TEST_ASSERT_BITS((mask), (UNITY_UINT32)(0), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_BIT_HIGH_MESSAGE(bit, actual, message)                                         UNITY_TEST_ASSERT_BITS(((UNITY_UINT32)1 << (bit)), (UNITY_UINT32)(-1), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_BIT_LOW_MESSAGE(bit, actual, message)                                          UNITY_TEST_ASSERT_BITS(((UNITY_UINT32)1 << (bit)), (UNITY_UINT32)(0), (actual), __LINE__, __FILE__, (message))

/* Integer Greater Than/ Less Than (of all sizes) */
#define TEST_ASSERT_GREATER_THAN_MESSAGE(threshold, actual, message)                               UNITY_TEST_ASSERT_GREATER_THAN_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_INT_MESSAGE(threshold, actual, message)                           UNITY_TEST_ASSERT_GREATER_THAN_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_INT8_MESSAGE(threshold, actual, message)                          UNITY_TEST_ASSERT_GREATER_THAN_INT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_INT16_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_GREATER_THAN_INT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_INT32_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_GREATER_THAN_INT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_INT64_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_GREATER_THAN_INT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_UINT_MESSAGE(threshold, actual, message)                          UNITY_TEST_ASSERT_GREATER_THAN_UINT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_UINT8_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_GREATER_THAN_UINT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_UINT16_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_GREATER_THAN_UINT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_GREATER_THAN_UINT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_UINT64_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_GREATER_THAN_UINT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_HEX8_MESSAGE(threshold, actual, message)                          UNITY_TEST_ASSERT_GREATER_THAN_HEX8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_HEX16_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_GREATER_THAN_HEX16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_HEX32_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_GREATER_THAN_HEX32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_THAN_HEX64_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_GREATER_THAN_HEX64((threshold), (actual), __LINE__, __FILE__, (message))

#define TEST_ASSERT_LESS_THAN_MESSAGE(threshold, actual, message)                                  UNITY_TEST_ASSERT_SMALLER_THAN_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_INT_MESSAGE(threshold, actual, message)                              UNITY_TEST_ASSERT_SMALLER_THAN_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_INT8_MESSAGE(threshold, actual, message)                             UNITY_TEST_ASSERT_SMALLER_THAN_INT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_INT16_MESSAGE(threshold, actual, message)                            UNITY_TEST_ASSERT_SMALLER_THAN_INT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_INT32_MESSAGE(threshold, actual, message)                            UNITY_TEST_ASSERT_SMALLER_THAN_INT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_INT64_MESSAGE(threshold, actual, message)                            UNITY_TEST_ASSERT_SMALLER_THAN_INT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_UINT_MESSAGE(threshold, actual, message)                             UNITY_TEST_ASSERT_SMALLER_THAN_UINT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_UINT8_MESSAGE(threshold, actual, message)                            UNITY_TEST_ASSERT_SMALLER_THAN_UINT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_UINT16_MESSAGE(threshold, actual, message)                           UNITY_TEST_ASSERT_SMALLER_THAN_UINT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(threshold, actual, message)                           UNITY_TEST_ASSERT_SMALLER_THAN_UINT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_UINT64_MESSAGE(threshold, actual, message)                           UNITY_TEST_ASSERT_SMALLER_THAN_UINT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_HEX8_MESSAGE(threshold, actual, message)                             UNITY_TEST_ASSERT_SMALLER_THAN_HEX8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_HEX16_MESSAGE(threshold, actual, message)                            UNITY_TEST_ASSERT_SMALLER_THAN_HEX16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_HEX32_MESSAGE(threshold, actual, message)                            UNITY_TEST_ASSERT_SMALLER_THAN_HEX32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_THAN_HEX64_MESSAGE(threshold, actual, message)                            UNITY_TEST_ASSERT_SMALLER_THAN_HEX64((threshold), (actual), __LINE__, __FILE__, (message))

#define TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(threshold, actual, message)                           UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(threshold, actual, message)                       UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_INT8_MESSAGE(threshold, actual, message)                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_INT16_MESSAGE(threshold, actual, message)                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_INT32_MESSAGE(threshold, actual, message)                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_INT64_MESSAGE(threshold, actual, message)                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_INT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT_MESSAGE(threshold, actual, message)                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT8_MESSAGE(threshold, actual, message)                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT16_MESSAGE(threshold, actual, message)                    UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT32_MESSAGE(threshold, actual, message)                    UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT64_MESSAGE(threshold, actual, message)                    UNITY_TEST_ASSERT_GREATER_OR_EQUAL_UINT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX8_MESSAGE(threshold, actual, message)                      UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX16_MESSAGE(threshold, actual, message)                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX32_MESSAGE(threshold, actual, message)                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_GREATER_OR_EQUAL_HEX64_MESSAGE(threshold, actual, message)                     UNITY_TEST_ASSERT_GREATER_OR_EQUAL_HEX64((threshold), (actual), __LINE__, __FILE__, (message))

#define TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(threshold, actual, message)                              UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(threshold, actual, message)                          UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_INT8_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_INT16_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_INT32_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_INT64_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_INT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_UINT_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_UINT8_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(threshold, actual, message)                       UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(threshold, actual, message)                       UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_UINT64_MESSAGE(threshold, actual, message)                       UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_UINT64((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_HEX8_MESSAGE(threshold, actual, message)                         UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX8((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_HEX16_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_SMALLER_OR_EQUAL_HEX16((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_HEX32_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_SMALLER_THAN_HEX32((threshold), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_LESS_OR_EQUAL_HEX64_MESSAGE(threshold, actual, message)                        UNITY_TEST_ASSERT_SMALLER_THAN_HEX64((threshold), (actual), __LINE__, __FILE__, (message))

/* Integer Ranges (of all sizes) */
#define TEST_ASSERT_INT_WITHIN_MESSAGE(delta, expected, actual, message)                           UNITY_TEST_ASSERT_INT_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_INT8_WITHIN_MESSAGE(delta, expected, actual, message)                          UNITY_TEST_ASSERT_INT8_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_INT16_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_INT16_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_INT32_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_INT32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_INT64_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_INT64_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_UINT_WITHIN_MESSAGE(delta, expected, actual, message)                          UNITY_TEST_ASSERT_UINT_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_UINT8_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_UINT8_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_UINT16_WITHIN_MESSAGE(delta, expected, actual, message)                        UNITY_TEST_ASSERT_UINT16_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_UINT32_WITHIN_MESSAGE(delta, expected, actual, message)                        UNITY_TEST_ASSERT_UINT32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_UINT64_WITHIN_MESSAGE(delta, expected, actual, message)                        UNITY_TEST_ASSERT_UINT64_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_HEX_WITHIN_MESSAGE(delta, expected, actual, message)                           UNITY_TEST_ASSERT_HEX32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_HEX8_WITHIN_MESSAGE(delta, expected, actual, message)                          UNITY_TEST_ASSERT_HEX8_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_HEX16_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_HEX16_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_HEX32_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_HEX32_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_HEX64_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_HEX64_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))

/* Structs and Strings */
#define TEST_ASSERT_EQUAL_PTR_MESSAGE(expected, actual, message)                                   UNITY_TEST_ASSERT_EQUAL_PTR((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, message)                                UNITY_TEST_ASSERT_EQUAL_STRING((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_STRING_LEN_MESSAGE(expected, actual, len, message)                       UNITY_TEST_ASSERT_EQUAL_STRING_LEN((expected), (actual), (len), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_MEMORY_MESSAGE(expected, actual, len, message)                           UNITY_TEST_ASSERT_EQUAL_MEMORY((expected), (actual), (len), __LINE__, __FILE__, (message))

/* Arrays */
#define TEST_ASSERT_EQUAL_INT_ARRAY_MESSAGE(expected, actual, num_elements, message)               UNITY_TEST_ASSERT_EQUAL_INT_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT8_ARRAY_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EQUAL_INT8_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT16_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_INT16_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT32_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_INT32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_INT64_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_INT64_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT_ARRAY_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EQUAL_UINT_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_UINT8_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT16_ARRAY_MESSAGE(expected, actual, num_elements, message)            UNITY_TEST_ASSERT_EQUAL_UINT16_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT32_ARRAY_MESSAGE(expected, actual, num_elements, message)            UNITY_TEST_ASSERT_EQUAL_UINT32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_UINT64_ARRAY_MESSAGE(expected, actual, num_elements, message)            UNITY_TEST_ASSERT_EQUAL_UINT64_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX_ARRAY_MESSAGE(expected, actual, num_elements, message)               UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EQUAL_HEX8_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX16_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_HEX16_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX32_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_HEX32_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_HEX64_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_HEX64_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_PTR_ARRAY_MESSAGE(expected, actual, num_elements, message)               UNITY_TEST_ASSERT_EQUAL_PTR_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_STRING_ARRAY_MESSAGE(expected, actual, num_elements, message)            UNITY_TEST_ASSERT_EQUAL_STRING_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_MEMORY_ARRAY_MESSAGE(expected, actual, len, num_elements, message)       UNITY_TEST_ASSERT_EQUAL_MEMORY_ARRAY((expected), (actual), (len), (num_elements), __LINE__, __FILE__, (message))

/* Arrays Compared To Single Value*/
#define TEST_ASSERT_EACH_EQUAL_INT_MESSAGE(expected, actual, num_elements, message)                UNITY_TEST_ASSERT_EACH_EQUAL_INT((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_INT8_MESSAGE(expected, actual, num_elements, message)               UNITY_TEST_ASSERT_EACH_EQUAL_INT8((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_INT16_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_INT16((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_INT32_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_INT32((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_INT64_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_INT64((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_UINT_MESSAGE(expected, actual, num_elements, message)               UNITY_TEST_ASSERT_EACH_EQUAL_UINT((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_UINT8_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_UINT8((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_UINT16_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EACH_EQUAL_UINT16((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_UINT32_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EACH_EQUAL_UINT32((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_UINT64_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EACH_EQUAL_UINT64((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_HEX_MESSAGE(expected, actual, num_elements, message)                UNITY_TEST_ASSERT_EACH_EQUAL_HEX32((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_HEX8_MESSAGE(expected, actual, num_elements, message)               UNITY_TEST_ASSERT_EACH_EQUAL_HEX8((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_HEX16_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_HEX16((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_HEX32_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_HEX32((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_HEX64_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_HEX64((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_PTR_MESSAGE(expected, actual, num_elements, message)                UNITY_TEST_ASSERT_EACH_EQUAL_PTR((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_STRING_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EACH_EQUAL_STRING((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_MEMORY_MESSAGE(expected, actual, len, num_elements, message)        UNITY_TEST_ASSERT_EACH_EQUAL_MEMORY((expected), (actual), (len), (num_elements), __LINE__, __FILE__, (message))

/* Floating Point (If Enabled) */
#define TEST_ASSERT_FLOAT_WITHIN_MESSAGE(delta, expected, actual, message)                         UNITY_TEST_ASSERT_FLOAT_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_FLOAT_MESSAGE(expected, actual, message)                                 UNITY_TEST_ASSERT_EQUAL_FLOAT((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_FLOAT_ARRAY_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EQUAL_FLOAT_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_FLOAT_MESSAGE(expected, actual, num_elements, message)              UNITY_TEST_ASSERT_EACH_EQUAL_FLOAT((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_INF_MESSAGE(actual, message)                                          UNITY_TEST_ASSERT_FLOAT_IS_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_NEG_INF_MESSAGE(actual, message)                                      UNITY_TEST_ASSERT_FLOAT_IS_NEG_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_NAN_MESSAGE(actual, message)                                          UNITY_TEST_ASSERT_FLOAT_IS_NAN((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_DETERMINATE_MESSAGE(actual, message)                                  UNITY_TEST_ASSERT_FLOAT_IS_DETERMINATE((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_NOT_INF_MESSAGE(actual, message)                                      UNITY_TEST_ASSERT_FLOAT_IS_NOT_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_NOT_NEG_INF_MESSAGE(actual, message)                                  UNITY_TEST_ASSERT_FLOAT_IS_NOT_NEG_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_NOT_NAN_MESSAGE(actual, message)                                      UNITY_TEST_ASSERT_FLOAT_IS_NOT_NAN((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE_MESSAGE(actual, message)                              UNITY_TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE((actual), __LINE__, __FILE__, (message))

/* Double (If Enabled) */
#define TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(delta, expected, actual, message)                        UNITY_TEST_ASSERT_DOUBLE_WITHIN((delta), (expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_DOUBLE_MESSAGE(expected, actual, message)                                UNITY_TEST_ASSERT_EQUAL_DOUBLE((expected), (actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EQUAL_DOUBLE_ARRAY_MESSAGE(expected, actual, num_elements, message)            UNITY_TEST_ASSERT_EQUAL_DOUBLE_ARRAY((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_EACH_EQUAL_DOUBLE_MESSAGE(expected, actual, num_elements, message)             UNITY_TEST_ASSERT_EACH_EQUAL_DOUBLE((expected), (actual), (num_elements), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_INF_MESSAGE(actual, message)                                         UNITY_TEST_ASSERT_DOUBLE_IS_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_NEG_INF_MESSAGE(actual, message)                                     UNITY_TEST_ASSERT_DOUBLE_IS_NEG_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_NAN_MESSAGE(actual, message)                                         UNITY_TEST_ASSERT_DOUBLE_IS_NAN((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_DETERMINATE_MESSAGE(actual, message)                                 UNITY_TEST_ASSERT_DOUBLE_IS_DETERMINATE((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_NOT_INF_MESSAGE(actual, message)                                     UNITY_TEST_ASSERT_DOUBLE_IS_NOT_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF_MESSAGE(actual, message)                                 UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_NOT_NAN_MESSAGE(actual, message)                                     UNITY_TEST_ASSERT_DOUBLE_IS_NOT_NAN((actual), __LINE__, __FILE__, (message))
#define TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE_MESSAGE(actual, message)                             UNITY_TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE((actual), __LINE__, __FILE__, (message))

/* end of UNITY_FRAMEWORK_H */

#define EXPECT_ABORT_BEGIN \
    if (TEST_PROTECT())    \
    {

#define VERIFY_FAILS_END                                                       \
    }                                                                          \
    Unity.CurrentTestFailed = (Unity.CurrentTestFailed == 1) ? 0 : 1;          \
    if (Unity.CurrentTestFailed == 1) {                                        \
      SetToOneMeanWeAlreadyCheckedThisTest = 1;                                \
      UnityPrint("[[[[ Test Should Have Failed But Did Not ]]]]");             \
      UNITY_OUTPUT_CHAR('\n');                                                 \
    }

#define VERIFY_IGNORES_END                                                     \
    }                                                                          \
    Unity.CurrentTestFailed = (Unity.CurrentTestIgnored == 1) ? 0 : 1;         \
    Unity.CurrentTestIgnored = 0;                                              \
    if (Unity.CurrentTestFailed == 1) {                                        \
      SetToOneMeanWeAlreadyCheckedThisTest = 1;                                \
      UnityPrint("[[[[ Previous Test Should Have Ignored But Did Not ]]]]");   \
      UNITY_OUTPUT_CHAR('\n');                                                 \
    }

#ifdef __cplusplus
}
#endif
#endif
