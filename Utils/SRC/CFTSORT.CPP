/*
 * CFTSORT — Convert CFT (text) to BFT (binary) with nearest-neighbor sorting
 * Part of FLINT laser glass flatness measurement system
 *
 * Reconstructed from CFTSORT.EXE (58756 bytes) disassembly.
 * Borland C++ 4.5, 16-bit DOS, Large memory model.
 *
 * Usage: CFTSORT source.cft dest.bft K_up K_down
 *   source.cft  — Input text coordinate file (flinttab format)
 *   dest.bft    — Output binary sorted file
 *   K_up        — Z-axis weight (1–1000) for upward movement (dz > 0)
 *   K_down      — Z-axis weight (1–1000) for downward movement (dz < 0)
 *
 * CFT format (text):
 *   Line 1: "flinttab"                    (magic header)
 *   Line 2: "nglass=\t<float>"            (refractive index, e.g. 1.506)
 *   Line 3: "xsize=\t<int>"              (X dimension in mm)
 *   Line 4: "ysize=\t<int>"              (Y dimension in mm)
 *   Line 5: "zsize=\t<int>"              (Z dimension in mm)
 *   Data:   "<N>:\t<X>\t<Y>\t<Z>"        (tab-separated integer coords)
 *
 * BFT format (binary, 14-byte header + 6 bytes/point):
 *   int16    magic    = 0x6662 ("bf")
 *   int16    nglass   = (int)(nglass_float * 1000)
 *   int16    xsize    (= CFT xsize * 10, in 0.1mm units)
 *   int16    ysize
 *   int16    zsize
 *   int16    reserved (high word of npoints, normally 0)
 *   int16    npoints  (low word)
 *   int16[3] data[]   = {x, y, z} per point
 *
 * Sorting algorithm:
 *   1. Find point with minimum Z, swap to position 0
 *   2. Nearest-neighbor heuristic from position 0 onward
 *   Distance metric:
 *     delta = max(|dx|, |dy|) + (dz * K) / 10
 *   where K = K_up for dz > 0, K = K_down for dz <= 0
 *   Downward movement produces negative dz, reducing the distance.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <alloc.h>

/* ---- Global variables (matching EXE data segment layout) ---- */
long   alloc_size;         /* total available memory (coreleft)     */
long   npoints;            /* number of points loaded               */
int far *points;           /* far pointer to flat int array [x,y,z] */
int    k_up;               /* Z weight for upward movement          */
int    k_down;             /* Z weight for downward movement        */
double nglass;             /* refractive index (stored as double)   */
int    xsize, ysize, zsize;/* dimensions in 0.1mm units             */

/*
 * calc_delta — Weighted distance between two points.
 *
 * Verified from disassembly at code offset 0x905C:
 *   delta = max(|dx|, |dy|) + ((long)k * dz) / 10
 *   where k = k_up if dz > 0, else k_down
 *
 * Points are stored as flat int array: points[idx*3+0]=x, [idx*3+1]=y, [idx*3+2]=z
 * Subtraction is 16-bit, then sign-extended to 32-bit for result computation.
 */
long calc_delta(long idx1, long idx2)
{
    long abs_dx, abs_dy, dz, result;
    int k, dx_raw, dy_raw;

    idx1 *= 3;
    idx2 *= 3;

    dx_raw = points[idx2]     - points[idx1];
    dy_raw = points[idx2 + 1] - points[idx1 + 1];
    dz = (long)(points[idx2 + 2] - points[idx1 + 2]);

    /* abs via inline abs() — generates cwd; xor ax,dx; sub ax,dx */
    abs_dx = (long)abs(dx_raw);
    abs_dy = (long)abs(dy_raw);

    result = (abs_dx > abs_dy) ? abs_dx : abs_dy;

    k = (dz > 0) ? k_up : k_down;
    result += ((long)k * dz) / 10L;

    return result;
}

/*
 * swap_points — Swap two points in the flat array.
 *
 * Verified from disassembly at 0x91BD:
 *   Multiplies each index by 3, then loops i=0..2 swapping words.
 */
void swap_points(long idx1, long idx2)
{
    int i, temp;

    idx1 *= 3;
    idx2 *= 3;

    for (i = 0; i < 3; i++) {
        temp = points[idx1 + i];
        points[idx1 + i] = points[idx2 + i];
        points[idx2 + i] = temp;
    }
}

/*
 * read_cft — Parse a CFT text file into the global points array.
 *
 * Function at code offset 0x92BC.
 * Uses strtok with "\t\n" delimiter to parse tab-separated fields.
 * Header values xsize/ysize/zsize are multiplied by 10 (mm -> 0.1mm).
 * Returns npoints on success, 0 on failure.
 */
long read_cft(char *filename)
{
    FILE *fp;
    char buf[256];
    char *tok;
    long count = 0;
    long max_points;

    fp = fopen(filename, "r");
    if (!fp) {
        printf("File Open Error\n");
        return 0;
    }

    /* Verify "flinttab" magic header */
    fgets(buf, sizeof(buf), fp);
    if (strncmp(buf, "flinttab", 8) != 0) {
        printf("Not cft file\n");
        fclose(fp);
        return 0;
    }

    /* Parse "nglass=\t1.506" */
    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n");
    tok = strtok(NULL, "\t\n");
    if (tok) nglass = atof(tok);

    /* Parse "xsize=\t50" (multiply by 10 for 0.1mm units) */
    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n");
    tok = strtok(NULL, "\t\n");
    if (tok) xsize = atoi(tok) * 10;

    /* Parse "ysize=\t50" */
    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n");
    tok = strtok(NULL, "\t\n");
    if (tok) ysize = atoi(tok) * 10;

    /* Parse "zsize=\t50" */
    fgets(buf, sizeof(buf), fp);
    tok = strtok(buf, "\t\n");
    tok = strtok(NULL, "\t\n");
    if (tok) zsize = atoi(tok) * 10;

    max_points = (alloc_size - 30000L) / 6;

    /* Read data lines: "1:\t-4888\t3092\t-19" */
    while (fgets(buf, sizeof(buf), fp)) {
        if (count >= max_points) {
            printf("Buffer overflow\n");
            break;
        }
        tok = strtok(buf, "\t\n");    /* skip "N:" prefix */
        tok = strtok(NULL, "\t\n");   /* x */
        if (!tok) continue;
        points[count * 3] = atoi(tok);

        tok = strtok(NULL, "\t\n");   /* y */
        if (!tok) continue;
        points[count * 3 + 1] = atoi(tok);

        tok = strtok(NULL, "\t\n");   /* z */
        if (!tok) continue;
        points[count * 3 + 2] = atoi(tok);

        count++;
    }

    fclose(fp);
    npoints = count;
    return count;
}

/*
 * write_bft — Write sorted points to a BFT binary file.
 *
 * Function at code offset 0x95F9.
 * Builds 14-byte header on stack, writes with fwrite(hdr, 2, 7, fp).
 * Point data written with fwrite(points, 12, ceil(npoints/2), fp).
 * nglass stored as (int)(nglass_double * 1000) via FPU.
 * Returns 0 on success, 1 on error.
 */
int write_bft(char *filename)
{
    FILE *fp;
    int header[7];

    header[0] = 0x6662;                                /* "bf" magic         */
    header[1] = (int)(nglass * 1000.0);                /* nglass * 1000      */
    header[2] = xsize;
    header[3] = ysize;
    header[4] = zsize;
    header[5] = (int)((unsigned long)npoints >> 16);   /* reserved (hi word) */
    header[6] = (int)npoints;                          /* npoints (lo word)  */

    fp = fopen(filename, "wb");
    if (!fp) {
        printf("file open(wb) error");
        return 1;
    }

    rewind(fp);
    fwrite(header, 2, 7, fp);
    fwrite(points, 12, (int)((npoints / 2) + (npoints % 2)), fp);

    return 0;
}

/*
 * main — Entry point.
 *
 * Function at code offset 0x8C31 (enter 0x48, 0).
 * Copies argv strings to local buffers via strcpy.
 * Uses coreleft() to determine available memory, farmalloc for point array.
 * Phase 1: selection of min-Z point to position 0 (swap during scan).
 * Phase 2: nearest-neighbor sort from position 0 onward.
 * Progress displayed as "%d%%\r" using ltoa, ESC key aborts.
 */
int main(int argc, char *argv[])
{
    char source_file[14];
    char dest_file[14];
    char temp[10];
    char buf[6];
    long idx, j, best_idx;
    long best_delta, d;
    long last_pct, pct;
    int  zmin;

    if (argc != 5) {
        printf("%s\n%s\n%s\n%s\n",
            "1 parameter - source file name",
            "2 parameter - destination file name",
            "3 parameter - K(1-1000) for dz>0",
            "4 parameter - K(1-1000) for dz<0");
        exit(0);
    }

    strcpy(source_file, argv[1]);
    strcpy(dest_file, argv[2]);

    strcpy(temp, argv[3]);
    k_up = atoi(temp);
    if (k_up < 1 || k_up > 1000) {
        printf("%s\n%s\n%s\n%s\n",
            "1 parameter - source file name",
            "2 parameter - destination file name",
            "3 parameter - K(1-1000) for dz>0",
            "4 parameter - K(1-1000) for dz<0");
        exit(0);
    }

    strcpy(temp, argv[4]);
    k_down = atoi(temp);
    if (k_down < 1 || k_down > 1000) {
        printf("%s\n%s\n%s\n%s\n",
            "1 parameter - source file name",
            "2 parameter - destination file name",
            "3 parameter - K(1-1000) for dz>0",
            "4 parameter - K(1-1000) for dz<0");
        exit(0);
    }

    printf("%d\t%d\n", k_up, k_down);

    alloc_size = coreleft();
    points = (int far *)farmalloc(alloc_size - 30000L);
    if (!points) {
        printf("Memory allocation error");
        exit(0);
    }

    printf("%s%s\n", "max pixel number = ",
           ltoa((alloc_size - 30000L) / 6, buf, 10));

    if (!read_cft(source_file)) {
        farfree(points);
        exit(0);
    }

    printf("%s%s\n", "pixel number = ", ltoa(npoints, buf, 10));

    /* ---- Phase 1: Find minimum Z, swap to position 0 ---- */
    last_pct = 1;
    for (idx = 1; idx < npoints; idx++) {
        zmin = points[2];   /* z of point 0 — re-read each iteration */
        if (points[idx * 3 + 2] < zmin) {
            swap_points(0, idx);
        }

        pct = idx * 100L / npoints;
        if (pct != last_pct) {
            last_pct = pct;
            printf("%s%s\r", ltoa(pct, buf, 10), "%");
        }

        if (kbhit()) {
            if (getch() == 0x1b) {
                farfree(points);
                exit(0);
            }
        }
    }
    printf("%s\n", "100%");

    /* si register held points[0].z throughout the loop */
    zmin = points[2];
    printf("%s%s\n", "Zmin = ", itoa(zmin, buf, 10));

    /* ---- Phase 2: Nearest-neighbor sort ---- */
    last_pct = 1;
    for (idx = 0; idx < npoints - 2; idx++) {
        /* Start with next unsorted point as initial candidate */
        best_delta = calc_delta(idx, idx + 1);
        best_idx = idx + 1;

        /* Search remaining points for a closer neighbor */
        for (j = idx + 2; j < npoints; j++) {
            d = calc_delta(idx, j);
            if (d < best_delta) {
                best_delta = d;
                best_idx = j;
            }
        }

        /* Swap closest point to position idx+1 */
        swap_points(best_idx, idx + 1);

        pct = idx * 100L / npoints;
        if (pct != last_pct) {
            last_pct = pct;
            printf("%s%s\r", ltoa(pct, buf, 10), "%");
        }

        if (kbhit()) {
            if (getch() == 0x1b) {
                farfree(points);
                exit(0);
            }
        }
    }
    printf("%s\n", "100%");
    printf("%s\n", "Ok");

    write_bft(dest_file);

    farfree(points);
    exit(0);
    return 0;
}
