/**
 * File: print_utility.c
 * ---------------------
 * This file contains a helpful function for plotting runtime data for our heap
 * allocators. We use gnuplot to plot the requests to the heap for utilitzation,
 * free node totals, and time per request.
 */
#include "print_utility.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static int gnuplot_process( void )
{
    int fds[2];
    if ( pipe2( fds, O_CLOEXEC ) < 0 ) {
        printf( "Could not open pipe for gnuplot process." );
        abort();
    }
    if ( fork() ) {
        close( fds[0] );
        return fds[1];
    }
    close( fds[1] );
    if ( dup2( fds[0], STDIN_FILENO ) < 0 ) {
        printf( "Child could not redirect pipe input." );
        abort();
    }
    char *args[] = { "-persist", NULL };
    execvp( "gnuplot", args );
    return -1;
}

void print_gnuplots( struct gnuplots *graphs )
{
    if ( graphs->num_ops == 0 ) {
        printf( "no operations to graph" );
        return;
    }
    printf( "Gnuplot printing " COLOR_CYN "3" COLOR_NIL " graphs. This may take a moment for large data sets...\n"
    );
    // UTILIZATION GRAPH
    int util_process = gnuplot_process();
    if ( util_process < 0 ) {
        printf( "gnuplot failed and may not be installed on your system. Install gnuplot for graphing." );
        return;
    }
    FILE *util_pipe = fdopen( util_process, "w" );
    if ( NULL == util_pipe ) {
        printf( "Parent could not open write pipe." );
        return;
    }
    (void)fprintf(
        util_pipe,
        "set terminal dumb ansi256;"
        "set colorsequence classic;"
        "set grid;"
        "set autoscale;"
        "set title 'Utilization %% over Heap Lifetime';"
        "set xlabel 'Script Line Number';"
        "plot '-' pt '#' lc rgb 'green' notitle\n"
    );
    for ( size_t req = 0; req < graphs->num_ops; req++ ) {
        (void)fprintf( util_pipe, "%ld %f \n", req + 1UL, graphs->util_percents[req] );
    }
    (void)fprintf( util_pipe, "e\n" );
    (void)fclose( util_pipe );
    (void)close( util_process );
    waitpid( -1, NULL, 0 );

    // FREE NODES GRAPH
    int free_process = gnuplot_process();
    if ( free_process < 0 ) {
        printf( "gnuplot failed and may not be installed on your system. Install gnuplot for graphing." );
        return;
    }
    FILE *free_pipe = fdopen( free_process, "w" );
    if ( NULL == free_pipe ) {
        printf( "Parent could not open write pipe." );
        return;
    }
    (void)fprintf(
        free_pipe,
        "set terminal dumb ansi256;"
        "set colorsequence classic;"
        "set grid;"
        "set autoscale;"
        "set title 'Number of Free Nodes over Heap Lifetime';"
        "set xlabel 'Script Line Number';"
        "plot '-' pt '#' lc rgb 'red' notitle\n"
    );
    size_t total_free = 0;
    for ( size_t req = 0; req < graphs->num_ops; req++ ) {
        total_free += graphs->free_nodes[req];
        (void)fprintf( free_pipe, "%ld %zu \n", req + 1UL, graphs->free_nodes[req] );
    }
    (void)fprintf( free_pipe, "e\n" );
    (void)fclose( free_pipe );
    (void)close( free_process );
    waitpid( -1, NULL, 0 );
    printf( "Average free nodes: %zu\n", total_free / graphs->num_ops );

    // REQUEST TIME GRAPH
    int requests_process = gnuplot_process();
    if ( requests_process < 0 ) {
        printf( "gnuplot failed and may not be installed on your system. Install gnuplot for graphing." );
        return;
    }
    FILE *requests_pipe = fdopen( requests_process, "w" );
    if ( NULL == requests_pipe ) {
        printf( "Parent could not open write pipe." );
        return;
    }
    (void)fprintf(
        requests_pipe,
        "set terminal dumb ansi256;set colorsequence "
        "classic;set zero 1e-20;set grid;"
        "set autoscale;set title 'Time (milliseconds) to "
        "Service a Heap Request';"
        "set xlabel 'Script Line Number';"
        "plot '-' pt '#' lc rgb 'cyan' notitle\n"
    );
    double total_time = 0;
    for ( size_t req = 0; req < graphs->num_ops; req++ ) {
        total_time += graphs->request_times[req];
        (void)fprintf( requests_pipe, "%ld %f \n", req + 1UL, graphs->request_times[req] );
    }
    (void)fprintf( requests_pipe, "e\n" );
    (void)fclose( requests_pipe );
    (void)close( requests_process );
    waitpid( -1, NULL, 0 );
    printf( "Average time (milliseconds) per request overall: %lfms\n", total_time / (double)graphs->num_ops );
}
