#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef GEM5
#include "gem5/m5ops.h"
#endif

//  User guide
//  $ bfs.elf 1 矩阵路径/名称
//  1/0 表示是否需要会进行多次迭代，迭代次数为 kernel_iter_times
//  Example : ./bfs.elf 1 matrices/simple_test_csr.mtx

//  该程序将会记录每一个节点的深度，开始时会随机挑选一个出度非零
//  的节点作为起点，最终输出为每一个节点的深度。

// CSR related
int row, col, nnz;

int *row_ptr;
int *col_idx;
int *out_degree;
char *vis;
int *result;
// double value[max_num];

// recursion-based BFS related
int start_point;
int *bfs_queue;
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
    assert(row == col);

    // malloc graph data
    row_ptr = (int *)malloc((row + 1) * sizeof(int));
    col_idx = (int *)malloc(nnz * sizeof(int));
    vis = (char *)malloc(nnz * sizeof(int));
    result = (int *)malloc(nnz * sizeof(int));
    out_degree = (int *)malloc(row * sizeof(int));
    bfs_queue = (int *)malloc(nnz * sizeof(int));

    for (int r_i = 0; r_i < row + 1; r_i++) {
        row_ptr[r_i] = 0;
    }

    for (int i = 0; i < nnz; i++) {
        int row_id;
        double fake_value;
        fscanf(fp, "%d%d%lf", &row_id, &col_idx[i], &fake_value);
        col_idx[i]--;
        row_ptr[row_id]++;
        out_degree[row_id--]++;
    }

    for (int i = 0; i < row; i++) {
        row_ptr[i + 1] += row_ptr[i];
        result[i] = -1;
        vis[i] = 0;
    }

    fclose(fp);
}

void free_graph_csr() {
    free(row_ptr);
    free(col_idx);
    free(out_degree);
    free(vis);
    free(result);
    free(bfs_queue);
}

void generate_start_point() {
    // generate start point randomly
    int cnt = 0;
    do {
        start_point = rand() % row;
        cnt++;
    } while (out_degree[start_point] <= 5 && cnt <= 1e5);
    bfs_queue[0] = start_point;
    result[start_point] = 0;
    vis[start_point] = 1;
    queue_start_ptr = 0;
    queue_end_ptr = 1;
}

void BFS_kernel(int depth) {
    if (depth <= 0) printf("Invalid depth ( depth <= 0 )\n");
    if (queue_start_ptr >= queue_end_ptr) return;
    int pos = bfs_queue[queue_start_ptr++];
    for (int i = row_ptr[pos]; i < row_ptr[pos + 1]; i++) {
        // printf("pos : %d, i : %d, col_idx[i] : %d, vis[col_idx[i]] :
        // %d\n",pos+1,i+1,col_idx[i]+1,vis[col_idx[i]]);
        if (!vis[col_idx[i]]) {
            bfs_queue[queue_end_ptr++] = col_idx[i];
            result[col_idx[i]] = depth;
            vis[col_idx[i]] = 1;
        }
    }
    BFS_kernel(depth + 1);
}

int main(int argc, char *argv[]) {
    assert(argc == 3);
    parse_graph_mtx(argv[2]);

    generate_start_point();

    if (argv[1][0] == '1') {
        for (int i = 0; i < 2; i++) {
            BFS_kernel(1);
            for (int i = 0; i < row; i++) {
                result[i] = -1;
                vis[i] = 0;
            }
            result[start_point] = 0;
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
    BFS_kernel(1);
    // === benchmark end here

#ifdef GEM5
    m5_dump_stats(0, 0);
#endif

    printf("BFS finished!\nStart point is NO.%d\n", start_point + 1);
    for (int i = 0; i < 8; i++) {
        printf("depth for NO.%d = %d\n", i + 1, result[i]);
    }

    free_graph_csr();

    return 0;
}
