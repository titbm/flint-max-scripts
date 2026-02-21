/*
 * CFTSORT2 — Convert CFT (text) to BFT (binary) with optimized sorting
 *
 * Enhanced version of CFTSORT.EXE for FLINT laser engraving system.
 * Compiled with Watcom C++ (32-bit DOS4GW) — no memory limits.
 *
 * Algorithm:
 *   Phase 1: Start from point with minimum Z
 *   Phase 2: Grid-accelerated nearest-neighbor heuristic
 *   Phase 3: Or-opt local search (chains of 1-3 points, no reversal)
 *
 * Or-opt is correct for asymmetric metrics (unlike 2-opt which reverses
 * segments and flips dz signs).
 *
 * Usage: CFTSORT2 source.cft dest.bft K_up K_down
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <math.h>

typedef short int16;

#define MAX_POINTS  16000000L
#define INITIAL_CAP     50000L
#define GRID_EMPTY      (-1L)
#define OROPT_WINDOW      500   /* search window for or-opt (positions) */

/* ---- Point data ---- */
static long   npoints  = 0;
static long   capacity = 0;
static int16 *points   = NULL;   /* [x0,y0,z0, x1,y1,z1, ...] */
static int    k_up, k_down;
static double nglass;
static int16  xsize, ysize, zsize;

/* ---- Tour as doubly-linked list (indexed by point index) ---- */
static long *tour_next = NULL;
static long *tour_prev = NULL;
static long  tour_head;          /* first point in tour */
static long  tour_tail;          /* last  point in tour */

/* ---- Spatial grid for NN acceleration ---- */
static long *grid_heads = NULL;  /* head of bucket chain per cell */
static long *grid_link  = NULL;  /* per-point next in same bucket */
static int   grid_nx, grid_ny;
static int   grid_x0, grid_y0;
static int   grid_cell;         /* cell side length */
static long  max_z_bonus;       /* for ring pruning */

/* ==================================================================
 * Distance metric (unchanged from original disassembly)
 * delta = max(|dx|, |dy|) + (dz * K) / 10
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

    /* Cell size: target ~25 points per cell */
    area   = (double)(xmax - xmin + 1) * (double)(ymax - ymin + 1);
    target = (double)npoints / 25.0;
    if (target < 1.0) target = 1.0;
    grid_cell = (int)sqrt(area / target);
    if (grid_cell < 1) grid_cell = 1;

    grid_x0 = xmin;
    grid_y0 = ymin;
    grid_nx = (xmax - xmin) / grid_cell + 1;
    grid_ny = (ymax - ymin) / grid_cell + 1;

    /* Cap at ~1M cells */
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

    /* Max possible Z bonus for ring pruning */
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
        /* Prune: min possible distance from this ring > best? */
        if (ring > 1) {
            ring_min = (long)(ring - 1) * grid_cell;
            if (ring_min - max_z_bonus > best_d)
                break;
        }

        /* Iterate border cells of this ring */
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

        /* Stop if ring covers entire grid */
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
 * Phase 1: Find point with minimum Z
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
 * Phase 2: Grid-accelerated nearest-neighbor → build tour linked list
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

    /* Build doubly-linked list from order */
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
 * Tour cost (traverse linked list)
 * ================================================================== */
static long tour_cost(void)
{
    long cost = 0, cur = tour_head;
    while (tour_next[cur] != GRID_EMPTY) {
        cost += calc_delta(cur, tour_next[cur]);
        cur = tour_next[cur];
    }
    return cost;
}

/* ==================================================================
 * Or-opt helpers
 * ================================================================== */

/*
 * Compute cost change of removing chain [seg_s..seg_e] and inserting
 * it after point 'after'. Negative = improvement.
 */
static long or_opt_change(long seg_s, long seg_e, long after)
{
    long ps  = tour_prev[seg_s];
    long ne  = tour_next[seg_e];
    long anx = tour_next[after];
    long saved = 0, added = 0;

    /* Edges removed by extracting chain */
    if (ps != GRID_EMPTY)  saved += calc_delta(ps, seg_s);
    if (ne != GRID_EMPTY)  saved += calc_delta(seg_e, ne);

    /* Edge created by closing the gap */
    if (ps != GRID_EMPTY && ne != GRID_EMPTY)
        added += calc_delta(ps, ne);

    /* Edge broken at insertion point */
    if (anx != GRID_EMPTY) saved += calc_delta(after, anx);

    /* Edges created by inserting chain */
    added += calc_delta(after, seg_s);
    if (anx != GRID_EMPTY) added += calc_delta(seg_e, anx);

    return added - saved;
}

/* Execute the chain move in the linked list */
static void or_opt_move(long seg_s, long seg_e, long after)
{
    long ps  = tour_prev[seg_s];
    long ne  = tour_next[seg_e];
    long anx = tour_next[after];

    /* Remove chain from current position */
    if (ps != GRID_EMPTY) tour_next[ps] = ne;
    else                  tour_head = ne;

    if (ne != GRID_EMPTY) tour_prev[ne] = ps;
    else                  tour_tail = ps;

    /* Insert chain after 'after' */
    tour_next[after] = seg_s;
    tour_prev[seg_s] = after;

    tour_next[seg_e] = anx;
    if (anx != GRID_EMPTY) tour_prev[anx] = seg_e;
    else                   tour_tail = seg_e;
}

/* ==================================================================
 * Phase 3: Or-opt local search
 *
 * For chain lengths 1, 2, 3: try removing each chain and reinserting
 * within a window of OROPT_WINDOW positions forward/backward.
 * No segment reversal → correct for asymmetric metrics.
 * ================================================================== */
static void phase3_or_opt(void)
{
    int pass, improved, len, c;
    long total_before, total_after;
    long seg_s, seg_e, next_s;
    long probe, best_after, best_change, change;
    int steps, ok;
    long count;
    long pct, last_pct;

    total_before = tour_cost();
    printf("Path cost before or-opt: %ld\n", total_before);

    for (pass = 1; pass <= 5; pass++) {
        improved = 0;

        for (len = 1; len <= 3; len++) {
            seg_s = tour_head;
            count = 0;
            last_pct = -1;

            while (seg_s != GRID_EMPTY) {
                /* Build chain of 'len' points starting at seg_s */
                seg_e = seg_s;
                ok = 1;
                for (c = 1; c < len; c++) {
                    if (tour_next[seg_e] == GRID_EMPTY) { ok = 0; break; }
                    seg_e = tour_next[seg_e];
                }
                if (!ok) break;

                /* Save next start before potential move */
                next_s = tour_next[seg_s];

                best_after  = GRID_EMPTY;
                best_change = 0;

                /* --- Forward search (after the chain) --- */
                probe = tour_next[seg_e];
                for (steps = 0; steps < OROPT_WINDOW && probe != GRID_EMPTY; steps++) {
                    change = or_opt_change(seg_s, seg_e, probe);
                    if (change < best_change) {
                        best_change = change;
                        best_after  = probe;
                    }
                    probe = tour_next[probe];
                }

                /* --- Backward search (before the chain) --- */
                /* Skip tour_prev[seg_s] — inserting there is a no-op */
                probe = tour_prev[seg_s];
                if (probe != GRID_EMPTY) probe = tour_prev[probe];
                for (steps = 0; steps < OROPT_WINDOW && probe != GRID_EMPTY; steps++) {
                    change = or_opt_change(seg_s, seg_e, probe);
                    if (change < best_change) {
                        best_change = change;
                        best_after  = probe;
                    }
                    probe = tour_prev[probe];
                }

                /* Apply best improving move */
                if (best_after != GRID_EMPTY && best_change < 0) {
                    or_opt_move(seg_s, seg_e, best_after);
                    improved = 1;
                }

                seg_s = next_s;
                count++;

                pct = count * 100L / npoints;
                if (pct != last_pct) {
                    last_pct = pct;
                    printf("Phase 3 (or-opt pass %d, len %d): %ld%%\r",
                           pass, len, pct);
                }

                if (kbhit() && getch() == 0x1b) {
                    printf("\nOr-opt aborted.\n");
                    goto done;
                }
            }
        }

        printf("Phase 3 (or-opt pass %d): 100%%       \n", pass);
        if (!improved) break;
    }

done:
    total_after = tour_cost();
    printf("Path cost after or-opt:  %ld\n", total_after);
    if (total_before > 0) {
        printf("Improvement: %.1f%%\n",
               (double)(total_before - total_after) * 100.0 /
               (double)total_before);
    }
}

/* ==================================================================
 * Flatten tour: reorder points[] to match linked list order
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
 * Dynamic array growth for CFT reading
 * ================================================================== */
static int ensure_capacity(long n)
{
    long new_cap;
    int16 *new_pts;

    if (n <= capacity) return 1;

    new_cap = capacity * 2;
    if (new_cap < n)         new_cap = n;
    if (new_cap > MAX_POINTS) new_cap = MAX_POINTS;
    if (new_cap < n)         return 0;

    new_pts = (int16 *)realloc(points, (size_t)(new_cap * 3) * sizeof(int16));
    if (!new_pts) return 0;

    points   = new_pts;
    capacity = new_cap;
    return 1;
}

/* ==================================================================
 * Read CFT file
 * ================================================================== */
static long read_cft(const char *filename)
{
    FILE *fp;
    char buf[256];
    char *tok;
    long count = 0;

    fp = fopen(filename, "r");
    if (!fp) { printf("File Open Error\n"); return 0; }

    fgets(buf, sizeof(buf), fp);
    if (strncmp(buf, "flinttab", 8) != 0) {
        printf("Not cft file\n");
        fclose(fp);
        return 0;
    }

    /* Header: nglass, xsize, ysize, zsize */
    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n"); tok = strtok(NULL, "\t\n");
    if (tok) nglass = atof(tok);

    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n"); tok = strtok(NULL, "\t\n");
    if (tok) xsize = (int16)(atoi(tok) * 10);

    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n"); tok = strtok(NULL, "\t\n");
    if (tok) ysize = (int16)(atoi(tok) * 10);

    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n"); tok = strtok(NULL, "\t\n");
    if (tok) zsize = (int16)(atoi(tok) * 10);

    /* Data lines: N:\tX\tY\tZ */
    while (fgets(buf, sizeof(buf), fp)) {
        if (!ensure_capacity(count + 1)) {
            printf("Memory error at point %ld\n", count);
            break;
        }
        tok = strtok(buf, "\t\n");         /* "N:" */
        tok = strtok(NULL, "\t\n");        /* x */
        if (!tok) continue;
        points[count * 3] = (int16)atoi(tok);
        tok = strtok(NULL, "\t\n");        /* y */
        if (!tok) continue;
        points[count * 3 + 1] = (int16)atoi(tok);
        tok = strtok(NULL, "\t\n");        /* z */
        if (!tok) continue;
        points[count * 3 + 2] = (int16)atoi(tok);
        count++;
    }

    fclose(fp);
    npoints = count;
    return count;
}

/* ==================================================================
 * Write BFT file
 * ================================================================== */
static int write_bft(const char *filename)
{
    FILE *fp;
    int16 hdr[7];

    hdr[0] = 0x6662;
    hdr[1] = (int16)(nglass * 1000.0);
    hdr[2] = xsize;
    hdr[3] = ysize;
    hdr[4] = zsize;
    hdr[5] = (int16)((unsigned long)npoints >> 16);
    hdr[6] = (int16)npoints;

    fp = fopen(filename, "wb");
    if (!fp) { printf("file open(wb) error\n"); return 1; }

    fwrite(hdr, 2, 7, fp);
    fwrite(points, 6, (size_t)npoints, fp);
    fclose(fp);
    return 0;
}

/* ==================================================================
 * Main
 * ================================================================== */
static void usage(void)
{
    printf("CFTSORT2 - CFT to BFT converter with optimized sorting\n");
    printf("Usage:  CFTSORT2 source.cft dest.bft K_up K_down\n");
    printf("  K_up   = Z weight (1-1000) for upward movement\n");
    printf("  K_down = Z weight (1-1000) for downward movement\n");
    printf("\nAlgorithm: grid NN + or-opt (correct for asymmetric metric)\n");
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

    /* Initial allocation */
    capacity = INITIAL_CAP;
    points = (int16 *)malloc((size_t)(capacity * 3) * sizeof(int16));
    if (!points) { printf("Memory allocation error\n"); return 1; }

    if (!read_cft(argv[1])) { free(points); return 1; }
    printf("Points loaded: %ld\n", npoints);

    if (npoints < 2) {
        printf("Too few points\n");
        free(points);
        return 1;
    }

    /* Sort */
    phase2_nn();        /* includes Phase 1 (min-Z) + grid NN */
    phase3_or_opt();    /* or-opt local search */
    flatten_tour();     /* linked list -> flat array */

    /* Write */
    if (write_bft(argv[2]) != 0) { free(points); return 1; }

    printf("Ok -- wrote %s\n", argv[2]);
    free(points);
    return 0;
}
