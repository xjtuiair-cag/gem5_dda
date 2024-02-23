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
// double *val; // abandon csr value

// PR related
#define MAX_ITER_TIME 5
#define d 0.85
#define eps 1e-4

int iter_time = 0;
double *vec;
double *ret;
int *L_tot;
double *outbound_param;
double *warm_ret;

// mtx parsing related
typedef struct {
    int r;
    int c;
    double val;
} mat_t;

void parse_pr_mtx(char *mtx_path) {
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
    assert(row == col);  // graph only

    // malloc CSR data
    row_ptr = (int *)malloc((row + 1) * sizeof(int));
    col_idx = (int *)malloc(nnz * sizeof(int));
    vec = (double *)malloc(col * sizeof(double));
    ret = (double *)malloc(row * sizeof(double));
    L_tot = (int *)malloc(row * sizeof(int));
    outbound_param = (double *)malloc(row * sizeof(double));

    // read mtx as COO
    mat_t *mat = (mat_t *)malloc(nnz * sizeof(mat_t));
    for (int i = 0; i < nnz; i++) {
        fscanf(fp, "%d%d%lf", &mat[i].r, &mat[i].c, &mat[i].val);
        mat[i].r--;
        mat[i].c--;
    }

    // convert COO to CSR
    for (int r_i = 0; r_i < row + 1; r_i++) {
        row_ptr[r_i] = 0;
    }
    for (int i = 0; i < nnz; i++) {
        col_idx[i] = mat[i].c;
        row_ptr[mat[i].r + 1] += 1;
    }
    for (int i = 1; i < row + 1; i++) {
        row_ptr[i] += row_ptr[i - 1];
    }

    // count outbound link total degree for every node
    for (int i = 0; i < nnz; i++) {
        L_tot[col_idx[i]]++;
    }
    for (int i = 0; i < row; i++) {
        outbound_param[i] = 1.0 / L_tot[col_idx[i]];
    }

    // initialize PR(node) as mean value
    for (int i = 0; i < row; i++) {
        vec[i] = 1.0 / row;
    }

    // finish parsing
    free(mat);
    fclose(fp);
}

void free_pr_csr() {
    free(row_ptr);
    free(col_idx);
    free(vec);
    free(ret);
    free(L_tot);
    free(outbound_param);
}

// check if PR(p_i;t) is convergent
int check_converge() {
    iter_time++;
    int flag = 0;
    double temp = 0;
    for (int i = 0; i < row; i++) {
        temp += (ret[i] > vec[i] ? ret[i] - vec[i] : vec[i] - ret[i]);
        vec[i] = ret[i];
    }
    if (temp > eps) flag = 1;
    if (iter_time > MAX_ITER_TIME) flag = 0;
    return flag;
}

void PR_kernel() {
    do {
        int last_row, this_row;

        // compute new iter PR(node) as ret
        for (int i = 0; i < row; i++) {
            last_row = row_ptr[i];
            this_row = row_ptr[i + 1];
            ret[i] = (1.0 - d) / row;
            for (int j = last_row; j < this_row; j++) {
                ret[i] += d * vec[col_idx[j]] * outbound_param[col_idx[j]];
            }
        }

        // Normalization
        double ret_sum = 0.0;
        for (int s = 0; s < row; s++) ret_sum += ret[s];
        for (int i = 0; i < row; i++) ret[i] = ret[i] / ret_sum;

    } while (check_converge());
}

int main(int argc, char *argv[]) {
    assert(argc == 3);
    parse_pr_mtx(argv[2]);

    if (argv[1][0] == '1') {
        for (int i = 0; i < 2; i++) {
            PR_kernel();
        }

        iter_time = 0;
        for (int i = 0; i < row; i++) {
            vec[i] = 1.0 / row;
        }
        printf("Processor is warmed.\n");
    }

#ifdef GEM5
    m5_checkpoint(0, 0);
    m5_reset_stats(0, 0);
#endif

    // === benchmark start here
    PR_kernel();
    // === benchmark end here

#ifdef GEM5
    m5_dump_stats(0, 0);
#endif

    printf("PageRank finished! The first 8 results:\n");
    for (int i = 0; i < 8; i++) {
        printf("%lf\n", ret[i]);
    }

    free_pr_csr();

    return 0;
}
