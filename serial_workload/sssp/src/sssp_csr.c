#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#ifdef GEM5
#include "gem5/m5ops.h"
#endif

// CSR related
int row, col, nnz;

int *row_ptr;
int *col_idx;
int *out_degree;
char *vis;
double *result;
double *value;

// recursion-based BFS related
int start_point;
int *sssp_queue;
int queue_start_ptr = 0;
int queue_end_ptr = 1;

// parse CSR-based mtx
void parse_graph_mtx(char *mtx_path) {
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

    // malloc graph data
    row_ptr = (int *)malloc((row + 1) * sizeof(int));
    col_idx = (int *)malloc(nnz * sizeof(int));
    out_degree = (int *)malloc(row * sizeof(int));
    vis = (char *)malloc(nnz * sizeof(char));
    result = (double *)malloc(nnz * sizeof(double));
    value = (double *)malloc(nnz * sizeof(double));
    sssp_queue = (int *)malloc(nnz * sizeof(int));

    for (int r_i = 0; r_i < row + 1; r_i++) {
        row_ptr[r_i] = 0;
    }

    for (int i = 0; i < nnz; i++) {
        int row_id;
        fscanf(fp, "%d%d%lf", &row_id, &col_idx[i], &value[i]);
        col_idx[i]--;
        row_ptr[row_id]++;
        out_degree[row_id--]++;
    }

    for (int i = 0; i < row; i++) {
        row_ptr[i + 1] += row_ptr[i];
        result[i] = DBL_MAX;
        vis[i] = 0;
    }

    fclose(fp);
}

void free_graph_csr() {
    free(row_ptr);
    free(col_idx);
    free(value);
    free(out_degree);
    free(vis);
    free(result);
    free(sssp_queue);
}

void generate_start_point() {
    // generate start point randomly
    int cnt = 0;
    do {
        start_point = rand() % row;
        cnt++;
    } while (out_degree[start_point] <= 5 && cnt <= 1e5);
    sssp_queue[0] = start_point;
    result[start_point] = 0;
    vis[start_point] = 1;
    queue_start_ptr = 0;
    queue_end_ptr = 1;
}

void SSSP_kernel(int depth) {
    if (depth <= 0) printf("Invalid depth ( depth <= 0 )\n");
    if (queue_start_ptr >= queue_end_ptr) return;
    int pos = sssp_queue[queue_start_ptr++];

    for (int i = row_ptr[pos]; i < row_ptr[pos + 1]; i++)
    {
        // find a shorter path
        double temp_len = result[pos] + value[i];
        if (result[col_idx[i]] > temp_len)
        {
            result[col_idx[i]] = temp_len; 
        }
        
        // add node to queue if not visited
        if (!vis[col_idx[i]]) {
            sssp_queue[queue_end_ptr++] = col_idx[i];
            vis[col_idx[i]] = 1;
        }
    }

    /** [DEPARTED] add node to queue if not visited **/
    // for (int i = row_ptr[pos]; i < row_ptr[pos + 1]; i++)
    // {
    //     if (!vis[col_idx[i]])
    //     {
    //         sssp_queue.push(col_idx[i]);
    //         vis[col_idx[i]] = 1;
    //     }
    // }

    SSSP_kernel(depth + 1);

}

int main(int argc, char *argv[]) {
    assert(argc == 3);
    parse_graph_mtx(argv[2]);

    generate_start_point();

    if (argv[1][0] == '1') {
        for (int i = 0; i < 2; i++) {
            SSSP_kernel(1);
            for (int i = 0; i < row; i++) {
                result[i] = -1;
                vis[i] = 0;
            }
            result[start_point] = 0.0;
            vis[start_point] = 1;
            queue_start_ptr = 0;
            queue_end_ptr = 1;
        }
        printf("Processor is warmed.\n");
    }

#ifdef GEM5
    m5_checkpoint(0, 0);
    m5_reset_stats(0, 0);
#endif

    // === benchmark start here
    SSSP_kernel(1);
    // === benchmark end here

#ifdef GEM5
    m5_dump_stats(0, 0);
#endif

    printf("SSSP finished!\nStart point is NO.%d\n", start_point + 1);
    printf("The first 8 distence:\n");
    for (int i = 0; i < 8; i++) {
        printf("%lf\n", result[i]);
    }

    free_graph_csr();

    return 0;
}
