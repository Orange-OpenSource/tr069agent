/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C) 2014 Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'. 
 *
 *---------------------------------------------------------------------------
 * File        : traces.h
 *
 * Created     : 2008/06/05
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

/**
 * @file traces.h
 *
 * @brief Definition of the trace system
 *
 * This file defines the trace system.
 * It is made of different macros, and can be used both for
 * userland and kernel-space applications.
 *
 */

#ifndef __TRACES_H__
#define __TRACES_H__

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/* 'ident' shall be set to a sensible value for the program using this logging facility.
 * For userland, 'ident' is used as the name of a file in '/tmp', but could later be
 * used with another meaning. The format for 'ident' is thus to be a valid file name
 * (avoid spaces and special characters!).
 * For kernel space, 'ident' is not used, but MUST be specified.
 */
#define FTX_TRACES_INIT( ident )    __FTX_TRACES_INIT( ident )
#define FTX_TRACES_CLOSE()          __FTX_TRACES_CLOSE()

/* 'filter' is one of the FTX_TRACES_FILTER_MASK_* filters described below */
#define FTX_VERBOSE( filter, ... )  __FTX_TRACE( __FTX_VERBOSE_LEVEL, filter, __VA_ARGS__ )
#define FTX_INFO( filter, ... )     __FTX_TRACE( __FTX_INFO_LEVEL, filter, __VA_ARGS__ )
#define FTX_WARNING( filter, ... )  __FTX_TRACE( __FTX_WARNING_LEVEL, filter, __VA_ARGS__ )
#define FTX_ERROR( filter, ... )    __FTX_TRACE( __FTX_ERROR_LEVEL, filter, __VA_ARGS__ )
#define FTX_FATAL( filter, ... )    __FTX_TRACE( __FTX_FATAL_LEVEL, filter, __VA_ARGS__ )
#define FTX_PANIC( filter, ... )    __FTX_TRACE( __FTX_PANIC_LEVEL, filter, __VA_ARGS__ )

#define FTX_ASSERT( expr )          __FTX_ASSERT( expr )

#define FTX_TRACES_FILTER_MASK_CMAPI    (((unsigned long int)1)<<__FTX_TRACES_FILTER_CMAPI_SHIFT)
#define FTX_TRACES_FILTER_MASK_OTV      (((unsigned long int)1)<<__FTX_TRACES_FILTER_OTV_SHIFT)
#define FTX_TRACES_FILTER_MASK_MAIN     (((unsigned long int)1)<<__FTX_TRACES_FILTER_MAIN_SHIFT)
#define FTX_TRACES_FILTER_MASK_NET      (((unsigned long int)1)<<__FTX_TRACES_FILTER_NET_SHIFT)
#define FTX_TRACES_FILTER_MASK_UPGRADE  (((unsigned long int)1)<<__FTX_TRACES_FILTER_UPGRADE_SHIFT)
#define FTX_TRACES_FILTER_MASK_VOIP     (((unsigned long int)1)<<__FTX_TRACES_FILTER_VOIP_SHIFT)
#define FTX_TRACES_FILTER_MASK_GUI      (((unsigned long int)1)<<__FTX_TRACES_FILTER_GUI_SHIFT)
#define FTX_TRACES_FILTER_MASK_ADMIN    (((unsigned long int)1)<<__FTX_TRACES_FILTER_ADMIN_SHIFT)
#define FTX_TRACES_FILTER_MASK_WIFI     (((unsigned long int)1)<<__FTX_TRACES_FILTER_WIFI_SHIFT)
#define FTX_TRACES_FILTER_MASK_EZP      (((unsigned long int)1)<<__FTX_TRACES_FILTER_EZP_SHIFT)
#define FTX_TRACES_FILTER_MASK_ALL      (~((unsigned long int)0))

/*---------------------------------------------------------------------------
 * Internals: do NOT use directly!
 *
 * WARNING!!!!! What follow are the internals (read: private part) of the
 * trace system!! Consider it as purely informational, and don't call
 * any of these macros directly! It can change any time.
 *
 *---------------------------------------------------------------------------*/

/** @defgroup InternalsGroup The internals
 * This is the internals of the trace system.
 * @{
 */

#ifndef FTX_TRACE_LEVEL
#warning "FTX build system: FTX_TRACE_LEVEL is not defined! Defaulting to level FATAL (3)."
#define FTX_TRACE_LEVEL 3
#endif

typedef enum {
    __FTX_TRACES_FILTER_CMAPI_SHIFT = 0,
    __FTX_TRACES_FILTER_TR069_SHIFT,
    __FTX_TRACES_FILTER_ADMIN_SHIFT,

    __FTX_TRACE_FILTER_MAX
} ftx_trace_filter_t;

typedef enum {
    __FTX_NOTRACE_LEVEL = 0,
    __FTX_ASSERT_LEVEL,
    __FTX_PANIC_LEVEL,
    __FTX_FATAL_LEVEL,
    __FTX_ERROR_LEVEL,
    __FTX_WARNING_LEVEL,
    __FTX_INFO_LEVEL,
    __FTX_VERBOSE_LEVEL,
} ftx_trace_level_t;

#if ( FTX_TRACE_LEVEL > 0 )

#ifdef FTX_TRACES_MAIN
volatile unsigned long int ftx_traces_filter;
#else
extern volatile unsigned long int ftx_traces_filter;
#endif

#define __FTX_TRACE( level, filter, ... )                           \
do {                                                                \
    if( level <= FTX_TRACE_LEVEL ) {                                \
        if( filter & ftx_traces_filter ) {                          \
            __FTX_DO_TRACE( level, __VA_ARGS__);                           \
        }                                                           \
    }                                                               \
    if( level <= __FTX_FATAL_LEVEL ) {                              \
        __FTX_DO_TRACE_EXIT();                                      \
    }                                                               \
} while( 0 )


#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <linux/limits.h>

#define __FTX_TRACES_FILTER_FILE "/tmp/ftx_traces_filter"
#define __FTX_TRACES_SIG_OFFSET 1

#ifdef FTX_TRACES_MAIN

#ifndef MAX_TRACE_SIZE
#define MAX_TRACE_SIZE 4096
#endif

char  tmpTraceStr[MAX_TRACE_SIZE];

FILE* ftx_traces_out;
struct sigaction ftx_traces_old_sigaction;
void ftx_traces_signal_handler( int signum,
                                siginfo_t* siginfo,
                                void* param )
{
    int fd = open( __FTX_TRACES_FILTER_FILE, O_RDONLY );
    char buffer[16];
    if( fd ) {
        memset( buffer, 0, 16 );
        read( fd, buffer, 15 ); /* not checking for errors puposedly */
        ftx_traces_filter = strtoul( buffer, NULL, 10 ); /* not checking for errors puposedly */
        close( fd );
    }
    /* If the file could not be openned, do nothing */
    /* Call old handler, if it exists */
    if(    ( ftx_traces_old_sigaction.sa_handler == SIG_DFL )
        || ( ftx_traces_old_sigaction.sa_handler == SIG_IGN )
        || ( ftx_traces_old_sigaction.sa_handler == NULL    ) ) {
        ;
    } else {
        if( ftx_traces_old_sigaction.sa_flags & SA_SIGINFO ) {
            ftx_traces_old_sigaction.sa_sigaction( signum, siginfo, param );
        } else {
            ftx_traces_old_sigaction.sa_handler( signum );
        }
    }
}
#else /* FTX_TRACES_MAIN */

extern char tmpTraceStr[];
extern FILE* ftx_traces_out;
extern struct sigaction ftx_traces_old_sigaction;

#endif /* ! FTX_TRACES_MAIN */

#define __FTX_TRACES_INIT( ident )                                                                              \
do {                                                                                                            \
    char filename[PATH_MAX];                                                                                    \
    struct sigaction new_sigaction;                                                                             \
    if( __FTX_TRACE_FILTER_MAX > ( 8 * sizeof( ftx_trace_filter_t ) ) ) {                                       \
        fprintf( stderr, "ERROR: The number of defined filters (%d) is bigger "                                 \
                         "than the maximum number of allowed filters (%d).\n",                                  \
                         __FTX_TRACE_FILTER_MAX, 8 * sizeof( ftx_trace_filter_t ) );                            \
        exit( -1 );                                                                                             \
    }                                                                                                           \
    if( ( SIGRTMIN + __FTX_TRACES_SIG_OFFSET ) > SIGRTMAX ) {                                                   \
        fprintf( stderr, "ERROR: __FTX_TRACES_SIG_OFFSET (=%d) overflows "                                      \
                         "the number of available Real-Time signals.\n",                                        \
                         __FTX_TRACES_SIG_OFFSET );                                                             \
        exit( -1 );                                                                                             \
    }                                                                                                           \
    memset( filename, 0, PATH_MAX );                                                                            \
    strcpy( filename, "/tmp/" );                                                                                \
    strncat( filename, ident, PATH_MAX - 6 ); /* Leave at least one byte for trailing \0 */                     \
    ftx_traces_out = fopen( filename, "a" );                                                                    \
    if( ftx_traces_out == NULL ) {                                                                              \
        fprintf( stderr, "ERROR: could not open file '%s' to store traces. Trying '/dev/null'...\n", filename );\
        ftx_traces_out = fopen( "/dev/null", "a" );                                                             \
        if( ftx_traces_out == NULL ) {                                                                          \
            fprintf( stderr, "ERROR: could not open '/dev/null' either...\n" );                                 \
            exit( -1 );                                                                                         \
        }                                                                                                       \
    }                                                                                                           \
    ftx_traces_filter = FTX_TRACES_FILTER_MASK_ALL;                                                             \
    new_sigaction.sa_sigaction = ftx_traces_signal_handler;                                                     \
    new_sigaction.sa_flags =  SA_SIGINFO;                                                                       \
    sigemptyset( &new_sigaction.sa_mask );                                                                      \
    if( sigaction( SIGRTMIN + __FTX_TRACES_SIG_OFFSET, &new_sigaction, &ftx_traces_old_sigaction ) == -1 ) {    \
        fprintf( stderr, "ERROR: could not set signal handler.\n" );                                            \
    }                                                                                                           \
} while( 0 )

#define __FTX_TRACES_CLOSE()                                                            \
do {                                                                                    \
    /* Restore the old signal handler (or NULL if none) */                              \
    sigaction( SIGRTMIN + __FTX_TRACES_SIG_OFFSET, &ftx_traces_old_sigaction, NULL );   \
    /* Close the trace file */                                                          \
    fclose( ftx_traces_out );                                                           \
} while( 0 )



/*

typedef enum {
    __FTX_NOTRACE_LEVEL = 0,
    __FTX_ASSERT_LEVEL,
    __FTX_PANIC_LEVEL,
    __FTX_FATAL_LEVEL,
    __FTX_ERROR_LEVEL,
    __FTX_WARNING_LEVEL,
    __FTX_INFO_LEVEL,
    __FTX_VERBOSE_LEVEL,
} ftx_trace_level_t;

*/



/* We need to save errno, as gettimeofday sets it on error */
#define __FTX_DO_TRACE(level, ... )                                                       \
do {                                                                                \
    int __errno = errno;                                                            \
    struct timeval t;                                                               \
    errno = 0;                                                                      \
    gettimeofday( &t, NULL );                                                       \
    if( errno != 0 ) {                                                              \
        t.tv_sec = 0;                                                               \
        t.tv_usec = 0;                                                              \
    }                                                                               \
    fprintf( ftx_traces_out, "[% 10ld.%03ld][PID=%d]", t.tv_sec, t.tv_usec/1000, (int)getpid() );          \
    fprintf( ftx_traces_out, " %s:%d(%s): ", __FILE__, __LINE__, __FUNCTION__ );    \
    if(level == __FTX_PANIC_LEVEL)   {fprintf( ftx_traces_out, " - PANIC - " ); }       \
    if(level == __FTX_FATAL_LEVEL)   {fprintf( ftx_traces_out, " - FATAL - " ); }       \
    if(level == __FTX_ERROR_LEVEL)   {fprintf( ftx_traces_out, " - ERROR - " ); }       \
    if(level == __FTX_WARNING_LEVEL) {fprintf( ftx_traces_out, " - WARN - " ); }        \
    if(level == __FTX_INFO_LEVEL)    {fprintf( ftx_traces_out, " - INFO - " ); }        \
    if(level == __FTX_VERBOSE_LEVEL) {fprintf( ftx_traces_out, " - DBG - " ); }         \
    fprintf( ftx_traces_out, __VA_ARGS__ );                                         \
    fflush( ftx_traces_out );                                                       \
    errno = __errno;                                                                \
} while( 0 )

#define __FTX_DO_TRACE_EXIT()   do { fflush( NULL ); exit( -1 ); } while( 0 )

#define __FTX_ASSERT( expr )                                    \
do {                                                            \
    if( FTX_TRACE_LEVEL >= __FTX_ASSERT_LEVEL ) {               \
        if( ! (expr) ) {                                        \
            __FTX_DO_TRACE(__FTX_ASSERT_LEVEL, "ASSERT failed: '" # expr "'\n" );  \
            __FTX_DO_TRACE_EXIT();                              \
        }                                                       \
    }                                                           \
} while( 0 )


#else /* FTX_TRACE_LEVEL = 0 */
#define __FTX_TRACES_INIT( ident )
#define __FTX_TRACES_CLOSE()
#define __FTX_TRACE( ... )
#define __FTX_ASSERT( expr )
#endif

/** @} */ /* InternalsGroup */

#endif /* __TRACES_H__ */
