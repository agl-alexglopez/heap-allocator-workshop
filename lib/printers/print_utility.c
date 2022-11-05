/**
 * File: print_utility.c
 * ---------------------
 * This file contains a helpful function for plotting runtime data for our heap allocators. We use
 * gnuplot to plot the requests to the heap for utilitzation, free node totals, and time per
 * request.
 */
#include <stdio.h>
#include <stdlib.h>
#include "print_utility.h"

/* @brief print_gnuplots  a wrapper for the three gnuplot functions with helpful information in
 *                        case someone is waiting for large data. It can take time.
 * @brief *graphs         the gnuplots struct containing all the graphs to print.
 */
void print_gnuplots(gnuplots *graphs) {
    // We rely alot on Unix like system help. Redirect which output and disgard. Not portable.
    if (system("which gnuplot > /dev/null 2>&1")) {
        printf("Gnuplot not installed. For graph output, install gnuplot...\n");
    } else {
        printf("Gnuplot printing "COLOR_CYN"3"COLOR_NIL" graphs. This may take a moment for large data sets...\n");
        FILE *util_pipe = popen("gnuplot -persist", "w");
        FILE *free_pipe = popen("gnuplot -persist", "w");
        FILE *time_pipe = popen("gnuplot -persist", "w");
                            // This helps us see nice colors with our graphs.
        fprintf(util_pipe, "set terminal dumb ansi256;"
                            // This helps with compatibility on dumb terminals.
                           "set colorsequence classic;"
                           // Adds a nice backing grid of dots.
                           "set grid;"
                           // I don't want to manage dimensions and ticks, let gnuplot do it.
                           "set autoscale;"
                           // Sits above the graph.
                           "set title 'Utilization %% over Heap Lifetime';"
                           // Makes it clear x label number corresponds to script lines=lifetime.
                           "set xlabel 'Script Line Number';"
                           // '-'/notitle prevents title inside graph. Set the point to desired char.
                          "plot '-' pt '#' lc rgb 'green' notitle\n");

        // We have no significant changes in settings, except color, for the free nodes.
        fprintf(free_pipe, "set terminal dumb ansi256;set colorsequence classic;set grid;"
                           "set autoscale;set title 'Number of Free Nodes over Heap Lifetime';"
                           "set xlabel 'Script Line Number';plot '-' pt '#' lc rgb 'red' notitle\n");

        // Time graph is the same, but we set zero to be more sensitive to small values---v
        fprintf(time_pipe, "set terminal dumb ansi256;set colorsequence classic;set zero 1e-20;set grid;"
                           "set autoscale;set title 'Time (milliseconds) to Service a Heap Request';"
                           "set xlabel 'Script Line Number';plot '-' pt '#' lc rgb 'cyan' notitle\n");

        double total_time = 0;
        double total_util = 0;
        size_t total_free = 0;

        // Getting all of the pipes plotting in one O(n) loop helped speed. Still slow though.
        for (int req = 0; req < graphs->num_ops; req++) {
            total_time += graphs->request_times[req];
            total_util += graphs->util_percents[req];
            total_free += graphs->free_nodes[req];
            fprintf(util_pipe, "%d %lf \n", req + 1, graphs->util_percents[req]);
            fprintf(free_pipe, "%d %zu \n", req + 1, graphs->free_nodes[req]);
            fprintf(time_pipe, "%d %lf \n", req + 1, graphs->request_times[req]);
        }

        fprintf(util_pipe, "e\n");
        fprintf(free_pipe, "e\n");
        fprintf(time_pipe, "e\n");
        pclose(util_pipe);
        printf("Average utilization: %.2f%%\n", total_util / graphs->num_ops);
        pclose(free_pipe);
        printf("Average free nodes: %zu\n", total_free / graphs->num_ops);
        pclose(time_pipe);
        printf("Average time (milliseconds) per request overall: %lfms\n", total_time / graphs->num_ops);
    }
}

