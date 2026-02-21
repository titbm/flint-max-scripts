/*
 * BFTSORT — Re-sort a BFT (binary) file with nearest-neighbor algorithm
 * Part of FLINT laser glass flatness measurement system
 *
 * Reconstructed from BFTSORT.EXE (42756 bytes) disassembly.
 * Borland C++ 4.5, 16-bit DOS, Large memory model.
 *
 * Usage: BFTSORT source.bft dest.bft K_up K_down
 *   source.bft  — Input binary coordinate file
 *   dest.bft    — Output binary sorted file
 *   K_up        — Z-axis weight (1–1000) for upward movement (dz > 0)
 *   K_down      — Z-axis weight (1–1000) for downward movement (dz < 0)
 *
 * BFT format (binary, 14-byte header + 6 bytes/point):
 *   int16    magic    = 0x6662 ("bf")
 *   int16    nglass   = (int)(nglass_float * 1000)
 *   int16    xsize    (in 0.1mm units)
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <alloc.h>

/* ---- Global variables (matching EXE data segment layout) ---- */
long   alloc_size;         /* total available memory (coreleft)     */
int far *points;           /* far pointer to flat int array [x,y,z] */
int    k_up;               /* Z weight for upward movement          */
int    k_down;             /* Z weight for downward movement        */
long   npoints;            /* number of points loaded               */
int    header[7];          /* BFT header stored at global DS:0x9A4  */

/*
 * calc_delta — Weighted distance between two points.
 *
 * Verified from disassembly at code offset 0x833B (enter 0x16).
 * Identical formula to CFTSORT.EXE:
 *   delta = max(|dx|, |dy|) + ((long)k * dz) / 10
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
 * Verified from disassembly at 0x849E (enter 0x02).
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
 * read_bft — Read a BFT binary file into the global points array.
 *
 * Function at code offset 0x8592 (enter 0x04).
 * Reads 14-byte header with fread(header, 2, 7, fp).
 * Verifies "bf" magic (0x6662).
 * Reconstructs npoints from header[5] (reserved=hi) and header[6] (lo).
 * Checks memory capacity, then reads data with fread(points, 12, ceil(n/2), fp).
 * Returns npoints on success, 0 on failure.
 */
long read_bft(char *filename)
{
    FILE *fp;

    fp = fopen(filename, "rb");
    if (!fp) {
        printf("File open error");
        return 0;
    }

    /* Read 14-byte BFT header into global array */
    fread(header, 2, 7, fp);

    /* Verify "bf" magic */
    if (header[0] != 0x6662) {
        fclose(fp);
        printf("Not bft file");
        return 0;
    }

    /* Reconstruct 32-bit npoints from header fields */
    npoints = ((long)(unsigned)header[5] << 16) + (unsigned)header[6];

    /* Check if allocated memory is sufficient */
    if ((alloc_size - 30000L) / 6 < npoints) {
        fclose(fp);
        printf("Allocation error");
        return 0;
    }

    /* Read point data */
    fread(points, 12, (int)((npoints / 2) + (npoints % 2)), fp);

    return npoints;
}

/*
 * write_bft — Write sorted BFT file preserving original header.
 *
 * Function at code offset 0x8697 (enter 0x04).
 * Writes original header (global), then point data.
 * Returns 0 on success, 1 on error.
 */
int write_bft(char *filename)
{
    FILE *fp;

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
 * Function at code offset 0x7EF8 (enter 0x48, 0).
 * Same structure as CFTSORT: argc check, memory alloc, read, sort, write.
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
        printf("Allocation error");
        exit(0);
    }

    printf("%s%s\n", "max pixel number = ",
           ltoa((alloc_size - 30000L) / 6, buf, 10));

    if (!read_bft(source_file)) {
        farfree(points);
        exit(0);
    }

    printf("%s%s\n", "pixel number = ", ltoa(npoints, buf, 10));

    /* ---- Phase 1: Find minimum Z, swap to position 0 ---- */
    last_pct = 1;
    for (idx = 1; idx < npoints; idx++) {
        zmin = points[2];
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

    zmin = points[2];
    printf("%s%s\n", "Zmin = ", itoa(zmin, buf, 10));

    /* ---- Phase 2: Nearest-neighbor sort ---- */
    last_pct = 1;
    for (idx = 0; idx < npoints - 2; idx++) {
        best_delta = calc_delta(idx, idx + 1);
        best_idx = idx + 1;

        for (j = idx + 2; j < npoints; j++) {
            d = calc_delta(idx, j);
            if (d < best_delta) {
                best_delta = d;
                best_idx = j;
            }
        }

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
