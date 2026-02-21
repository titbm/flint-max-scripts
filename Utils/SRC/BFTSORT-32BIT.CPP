/*
 * BFTSORT2 — Re-sort BFT file with grid-accelerated nearest-neighbor algorithm
 *
 * Enhanced version of BFTSORT.EXE for FLINT laser engraving system.
 * Compiled with Watcom C++ (32-bit) — no memory limits.
 *
 * Algorithm:
 *   Phase 1: Start from point with minimum Z (deepest in glass)
 *   Phase 2: Grid-accelerated nearest-neighbor heuristic
 *
 * Note: or-opt reordering is intentionally omitted — laser physics requires
 * engraving from deepest Z to shallowest Z. The NN heuristic respects this
 * naturally (k_up >> k_down makes going shallower expensive).
 *
 * Usage: BFTSORT2 source.bft dest.bft K_up K_down
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <math.h>

typedef short int16;

#define MAX_POINTS  16000000L
#define GRID_EMPTY      (-1L)

/* ---- Point data ---- */
static long   npoints = 0;
static int16 *points  = NULL;
static int    k_up, k_down;
static int16  bft_header[7];   /* preserved from source file */

/* ---- Tour linked list ---- */
static long *tour_next = NULL;
static long *tour_prev = NULL;
static long  tour_head;
static long  tour_tail;

/* ---- Spatial grid ---- */
static long *grid_heads = NULL;
static long *grid_link  = NULL;
static int   grid_nx, grid_ny;
static int   grid_x0, grid_y0;
static int   grid_cell;
static long  max_z_bonus;

/* ==================================================================
 * Distance metric
 * ================================================================== */
static long calc_delta(long i, long j)
{
    long i3 = i * 3, j3 = j * 3;
    int dx = points[j3]     - points[i3];
    int dy = points[j3 + 1] - points[i3 + 1];
    int dz = points[j3 + 2] - points[i3 + 2];

    long adx = (long)abs(dx);
    long ady = (long)abs(dy);
    long result = (adx > ady) ? adx : ady;

    int k = (dz > 0) ? k_up : k_down;
    result += ((long)k * (long)dz) / 10L;

    return result;
}

/* ==================================================================
 * Spatial grid
 * ================================================================== */
static void grid_build(void)
{
    int xmin, xmax, ymin, ymax, zmin, zmax;
    long i, cell;
    double area, target;
    int x, y, z, cx, cy;

    xmin = ymin = zmin =  32767;
    xmax = ymax = zmax = -32768;

    for (i = 0; i < npoints; i++) {
        x = points[i * 3];
        y = points[i * 3 + 1];
        z = points[i * 3 + 2];
        if (x < xmin) xmin = x;  if (x > xmax) xmax = x;
        if (y < ymin) ymin = y;  if (y > ymax) ymax = y;
        if (z < zmin) zmin = z;  if (z > zmax) zmax = z;
    }

    area   = (double)(xmax - xmin + 1) * (double)(ymax - ymin + 1);
    target = (double)npoints / 25.0;
    if (target < 1.0) target = 1.0;
    grid_cell = (int)sqrt(area / target);
    if (grid_cell < 1) grid_cell = 1;

    grid_x0 = xmin;
    grid_y0 = ymin;
    grid_nx = (xmax - xmin) / grid_cell + 1;
    grid_ny = (ymax - ymin) / grid_cell + 1;

    while ((long)grid_nx * grid_ny > 1000000L) {
        grid_cell *= 2;
        grid_nx = (xmax - xmin) / grid_cell + 1;
        grid_ny = (ymax - ymin) / grid_cell + 1;
    }

    {
        long total = (long)grid_nx * grid_ny;
        grid_heads = (long *)malloc(total * sizeof(long));
        grid_link  = (long *)malloc(npoints * sizeof(long));
        if (!grid_heads || !grid_link) {
            printf("Grid allocation failed\n");
            exit(1);
        }
        for (i = 0; i < total; i++) grid_heads[i] = GRID_EMPTY;
    }

    for (i = 0; i < npoints; i++) {
        cx = (points[i * 3]     - grid_x0) / grid_cell;
        cy = (points[i * 3 + 1] - grid_y0) / grid_cell;
        cell = (long)cy * grid_nx + cx;
        grid_link[i] = grid_heads[cell];
        grid_heads[cell] = i;
    }

    {
        long kmax = (k_up > k_down) ? k_up : k_down;
        max_z_bonus = kmax * (long)(zmax - zmin) / 10L;
    }

    printf("Grid: %d x %d cells, cell=%d, max_z_bonus=%ld\n",
           grid_nx, grid_ny, grid_cell, max_z_bonus);
}

static void grid_remove(long idx)
{
    int cx = (points[idx * 3]     - grid_x0) / grid_cell;
    int cy = (points[idx * 3 + 1] - grid_y0) / grid_cell;
    long cell = (long)cy * grid_nx + cx;
    long p;

    if (grid_heads[cell] == idx) {
        grid_heads[cell] = grid_link[idx];
    } else {
        p = grid_heads[cell];
        while (p != GRID_EMPTY && grid_link[p] != idx)
            p = grid_link[p];
        if (p != GRID_EMPTY)
            grid_link[p] = grid_link[idx];
    }
}

static long grid_nearest(long from_idx)
{
    int fx, fy, cx, cy;
    long best = GRID_EMPTY, best_d = 0x7FFFFFFFL;
    int ring, dx, dy, gx, gy;
    long ring_min, cell, j, d;

    fx = points[from_idx * 3];
    fy = points[from_idx * 3 + 1];
    cx = (fx - grid_x0) / grid_cell;
    cy = (fy - grid_y0) / grid_cell;

    for (ring = 0; ; ring++) {
        if (ring > 1) {
            ring_min = (long)(ring - 1) * grid_cell;
            if (ring_min - max_z_bonus > best_d)
                break;
        }

        for (dy = -ring; dy <= ring; dy++) {
            for (dx = -ring; dx <= ring; dx++) {
                if (dy != -ring && dy != ring && dx != -ring && dx != ring)
                    continue;
                gx = cx + dx;
                gy = cy + dy;
                if (gx < 0 || gx >= grid_nx || gy < 0 || gy >= grid_ny)
                    continue;

                cell = (long)gy * grid_nx + gx;
                j = grid_heads[cell];
                while (j != GRID_EMPTY) {
                    d = calc_delta(from_idx, j);
                    if (d < best_d) {
                        best_d = d;
                        best = j;
                    }
                    j = grid_link[j];
                }
            }
        }

        if (cx - ring <= 0 && cx + ring >= grid_nx - 1 &&
            cy - ring <= 0 && cy + ring >= grid_ny - 1)
            break;
    }

    return best;
}

static void grid_free(void)
{
    free(grid_heads); grid_heads = NULL;
    free(grid_link);  grid_link  = NULL;
}

/* ==================================================================
 * Phase 1: Find min-Z
 * ================================================================== */
static long find_min_z(void)
{
    long best = 0, i;
    int16 zmin = points[2];

    for (i = 1; i < npoints; i++) {
        if (points[i * 3 + 2] < zmin) {
            zmin = points[i * 3 + 2];
            best = i;
        }
    }
    printf("Phase 1: Zmin = %d (point %ld)\n", (int)zmin, best);
    return best;
}

/* ==================================================================
 * Phase 2: Grid NN → linked list
 * ================================================================== */
static void phase2_nn(void)
{
    long *order;
    long i, start, nearest;
    long pct, last_pct = -1;

    order = (long *)malloc(npoints * sizeof(long));
    if (!order) { printf("Order alloc failed\n"); exit(1); }

    grid_build();

    start = find_min_z();
    order[0] = start;
    grid_remove(start);

    for (i = 1; i < npoints; i++) {
        nearest = grid_nearest(order[i - 1]);
        order[i] = nearest;
        grid_remove(nearest);

        pct = i * 100L / npoints;
        if (pct != last_pct) {
            last_pct = pct;
            printf("Phase 2 (NN): %ld%%\r", pct);
        }
        if (kbhit() && getch() == 0x1b) {
            printf("\nAborted.\n");
            exit(0);
        }
    }
    printf("Phase 2 (NN): 100%%\n");

    grid_free();

    tour_next = (long *)malloc(npoints * sizeof(long));
    tour_prev = (long *)malloc(npoints * sizeof(long));
    if (!tour_next || !tour_prev) { printf("Tour alloc failed\n"); exit(1); }

    tour_head = order[0];
    tour_tail = order[npoints - 1];

    for (i = 0; i < npoints; i++) {
        tour_next[order[i]] = (i < npoints - 1) ? order[i + 1] : GRID_EMPTY;
        tour_prev[order[i]] = (i > 0)           ? order[i - 1] : GRID_EMPTY;
    }

    free(order);
}

/* ==================================================================
 * Flatten tour
 * ================================================================== */
static void flatten_tour(void)
{
    int16 *sorted;
    long cur, i;

    sorted = (int16 *)malloc((size_t)npoints * 3 * sizeof(int16));
    if (!sorted) { printf("Flatten alloc failed\n"); exit(1); }

    cur = tour_head;
    i = 0;
    while (cur != GRID_EMPTY && i < npoints) {
        sorted[i * 3]     = points[cur * 3];
        sorted[i * 3 + 1] = points[cur * 3 + 1];
        sorted[i * 3 + 2] = points[cur * 3 + 2];
        cur = tour_next[cur];
        i++;
    }

    memcpy(points, sorted, (size_t)npoints * 3 * sizeof(int16));
    free(sorted);
    free(tour_next); tour_next = NULL;
    free(tour_prev); tour_prev = NULL;
}

/* ==================================================================
 * Read / Write BFT
 * ================================================================== */
static long read_bft(const char *filename)
{
    FILE *fp;

    fp = fopen(filename, "rb");
    if (!fp) { printf("File open error\n"); return 0; }

    fread(bft_header, 2, 7, fp);

    if (bft_header[0] != 0x6662) {
        fclose(fp);
        printf("Not bft file\n");
        return 0;
    }

    npoints = ((long)(unsigned short)bft_header[5] << 16)
            + (unsigned short)bft_header[6];

    if (npoints <= 0 || npoints > MAX_POINTS) {
        fclose(fp);
        printf("Invalid point count: %ld\n", npoints);
        return 0;
    }

    points = (int16 *)malloc((size_t)(npoints * 3) * sizeof(int16));
    if (!points) {
        fclose(fp);
        printf("Memory allocation error (%ld points)\n", npoints);
        return 0;
    }

    fread(points, 6, (size_t)npoints, fp);
    fclose(fp);
    return npoints;
}

static int write_bft(const char *filename)
{
    FILE *fp;

    fp = fopen(filename, "wb");
    if (!fp) { printf("file open(wb) error\n"); return 1; }

    fwrite(bft_header, 2, 7, fp);
    fwrite(points, 6, (size_t)npoints, fp);
    fclose(fp);
    return 0;
}

/* ==================================================================
 * Main
 * ================================================================== */
static void usage(void)
{
    printf("BFTSORT2 - BFT re-sorter with optimized algorithm\n");
    printf("Usage:  BFTSORT2 source.bft dest.bft K_up K_down\n");
    printf("  K_up   = Z weight (1-1000) for upward movement\n");
    printf("  K_down = Z weight (1-1000) for downward movement\n");
    printf("\nAlgorithm: grid-accelerated nearest-neighbor (Z-monotone)\n");
}

int main(int argc, char *argv[])
{
    if (argc != 5) { usage(); return 1; }

    k_up   = atoi(argv[3]);
    k_down = atoi(argv[4]);

    if (k_up < 1 || k_up > 1000 || k_down < 1 || k_down > 1000) {
        usage();
        return 1;
    }

    printf("K_up=%d  K_down=%d\n", k_up, k_down);

    if (!read_bft(argv[1])) return 1;
    printf("Points loaded: %ld\n", npoints);

    if (npoints < 2) {
        printf("Too few points\n");
        free(points);
        return 1;
    }

    /* Sort */
    phase2_nn();
    flatten_tour();

    /* Write */
    if (write_bft(argv[2]) != 0) { free(points); return 1; }

    printf("Ok -- wrote %s\n", argv[2]);
    free(points);
    return 0;
}
