#include <assert.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef GEM5
#include "m5ops.h"
#endif

#define NUM_THREADS 4

// CSR related
int row, col, nnz;
int *row_ptr;
int *col_idx;
double *val;

// SpMV related
double *vec;
double *ret[NUM_THREADS];
double *warm_ret;

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

    // malloc CSR data
    row_ptr = (int *)malloc((row + 1) * sizeof(int));
    col_idx = (int *)malloc(nnz * sizeof(int));
    val = (double *)malloc(nnz * sizeof(double));
    vec = (double *)malloc(col * sizeof(double));
    for (int t = 0; t < NUM_THREADS; t++) {
        ret[t] = (double *)malloc(row * sizeof(double));
    }

    // read mtx as COO
    mat_t *mat = (mat_t *)malloc(nnz * sizeof(mat_t));
    for (int i = 0; i < nnz; i++) {
        fscanf(fp, "%d%d%lf", &mat[i].r, &mat[i].c, &mat[i].val);
        mat[i].r--;
        mat[i].c--;
    }

    // render vec
    for (int i = 0; i < col; i++) {
        vec[i] = 1;
    }

    // convert COO to CSR
    for (int r_i = 0; r_i < row + 1; r_i++) {
        row_ptr[r_i] = 0;
    }
    for (int i = 0; i < nnz; i++) {
        val[i] = mat[i].val;
        col_idx[i] = mat[i].c;
        row_ptr[mat[i].r + 1] += 1;
    }
    for (int i = 1; i < row + 1; i++) {
        row_ptr[i] += row_ptr[i - 1];
    }

    // finish parsing
    free(mat);
    fclose(fp);
}

void free_csr() {
    free(row_ptr);
    free(col_idx);
    free(vec);
    free(val);
    for (int i; i < NUM_THREADS; i++) {
        free(ret[i]);
    }
}

inline __attribute__((always_inline)) void mul(double *ret, double *vec,
                                               double *val, int *row_ptr,
                                               int *col_idx) {
    int last_row, this_row;
    for (int i = 0; i < row; i++) {
        last_row = row_ptr[i];
        this_row = row_ptr[i + 1];
        for (int j = last_row; j < this_row; j++) {
            ret[i] += val[j] * vec[col_idx[j]];
        }
    }
}

int main(int argc, char *argv[]) {
    assert(argc == 3);
    parse_csr_mtx(argv[2]);

    omp_set_dynamic(0);
    omp_set_num_threads(NUM_THREADS);

#ifdef GEM5
    m5_checkpoint(0, 0);
#endif

    if (argv[1][0] == '1') {
        warm_ret = (double *)malloc(row * sizeof(double));
        for (int i = 0; i < 2; i++) {
            mul(warm_ret, vec, val, row_ptr, col_idx);
        }
        free(warm_ret);
        printf("Processor is warmed.\n");
    }

#ifdef GEM5
    m5_reset_stats(0, 0);
#endif

    // === benchmark start here
#pragma omp parallel for schedule(static, 1)
    for (int i_th = 0; i_th < NUM_THREADS; i_th++) {
        // printf("thread %d, cpu %d\n", omp_get_thread_num(), sched_getcpu());
        mul(ret[i_th], vec, val, row_ptr, col_idx);
        // sleep(1000000);
    }
    // === benchmark end here

#ifdef GEM5
    m5_dump_stats(0, 0);
#endif

    printf("SpMV-OMP finished!\nThe first 8 results: \n");
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < NUM_THREADS; j++) printf("%lf ", ret[j][i]);
        printf("\n");
    }

    free_csr();

    return 0;
}
