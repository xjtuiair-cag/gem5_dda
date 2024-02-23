#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef GEM5
#include "gem5/m5ops.h"
#endif

// CSR related
int row, col, nnz;
int *row_ptr;
int *col_idx;
double *val;

// SYMGS related
double *diagonal;
double *r;
double *x;

// mtx parsing related
typedef struct {
    int r;
    int c;
    double val;
} mat_t;

void parse_csr_mtx(char *mtx_path) {
    FILE *fp = fopen(mtx_path, "r");

    // mtx file path check
    if (!fp) {
        printf("Invalid File Path!\n");
        exit(0);
    } else {
        printf("Valid File Path: %s\n", mtx_path);
    }

    // skip comment with % as head
    char buffer[200];
    long int fp_pos = ftell(fp);
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (buffer[0] != '%') {
            fseek(fp, fp_pos, SEEK_SET);
            break;
        }
        fp_pos = ftell(fp);
    }

    // get row and col and nnz number
    fscanf(fp, "%d%d%d", &row, &col, &nnz);
    assert(row == col);

    // malloc CSR data
    row_ptr = (int *)malloc((row + 1) * sizeof(int));
    col_idx = (int *)malloc(nnz * sizeof(int));
    val = (double *)malloc(nnz * sizeof(double));
    diagonal = (double *)malloc(row * sizeof(double));
    x = (double *)malloc(row * sizeof(double));
    r = (double *)malloc(row * sizeof(double));

    // read mtx as COO
    mat_t *mat = (mat_t *)malloc(nnz * sizeof(mat_t));
    for (int i = 0; i < nnz; i++) {
        fscanf(fp, "%d%d%lf", &mat[i].r, &mat[i].c, &mat[i].val);
        mat[i].r--;
        mat[i].c--;
    }

    // init diagnoal as near-zero
    for (int d_i = 0; d_i < row; d_i++) {
        diagonal[d_i] = 1.0e-9;
    }

    // convert COO to CSR
    for (int r_i = 0; r_i < row + 1; r_i++) {
        row_ptr[r_i] = 0;
    }
    for (int i = 0; i < nnz; i++) {
        val[i] = mat[i].val;
        col_idx[i] = mat[i].c;
        row_ptr[mat[i].r + 1] += 1;
        if (mat[i].r == mat[i].c) diagonal[mat[i].r] = val[i];
    }

    // convert row_ptr and init x,r
    for (int i = 0; i < row; i++) {
        row_ptr[i + 1] += row_ptr[i];
        r[i] = 1.0;
    }
    for (int c = 0; c < col; c++) {
        x[c] = 1.0;
    }

    // finish parsing
    free(mat);
    fclose(fp);
}

void free_csr() {
    free(row_ptr);
    free(col_idx);
    free(val);
    free(diagonal);
    free(x);
    free(r);
}

void SYMGS_kernel() {
    // forward sweep
    for (int i = 0; i < row; i++) {
        double sum = r[i];

        for (int j = row_ptr[i]; j < row_ptr[i + 1]; j++) {
            sum -= val[j] * x[col_idx[j]];
        }
        sum += x[i] * diagonal[i];

        x[i] = sum / diagonal[i];
    }

    // backward sweep
    for (int i = row - 1; i >= 0; i--) {
        double sum = r[i];

        for (int j = row_ptr[i]; j < row_ptr[i + 1]; j++) {
            sum -= val[j] * x[col_idx[j]];
        }
        sum += x[i] * diagonal[i];

        x[i] = sum / diagonal[i];
    }
}

int main(int argc, char *argv[]) {
    assert(argc == 3);
    parse_csr_mtx(argv[2]);

    if (argv[1][0] == '1') {
        for (int i = 0; i < 2; i++) {
            SYMGS_kernel();
        }
        printf("Processor is warmed.\n");
    }

#ifdef GEM5
    m5_checkpoint(0, 0);
    m5_reset_stats(0, 0);
#endif

    // === benchmark start here
    SYMGS_kernel();
    // === benchmark end here

#ifdef GEM5
    m5_dump_stats(0, 0);
#endif

    printf("SYMGS finished!\nThe first 8 results: \n");
    for (int i = 0; i < 8; i++) {
        printf("%lf\n", x[i]);
    }

    free_csr();

    return 0;
}
