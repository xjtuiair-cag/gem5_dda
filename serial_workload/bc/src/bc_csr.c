#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef GEM5
#include "gem5/m5ops.h"
#endif

typedef float ScoreT;
typedef double CountT;
typedef int NodeID;

// CSR related
int row, col, nnz;
int *row_ptr;
NodeID *col_idx;

// BC related
ScoreT *scores;
CountT *path_counts;
NodeID *vis_queue;
ScoreT *deltas;
int *depths;
int *out_degree;
int start_point;

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
    col_idx = (NodeID *)malloc(nnz * sizeof(NodeID));

    scores = (ScoreT *)malloc(row * sizeof(ScoreT));
    path_counts = (CountT *)malloc(row * sizeof(CountT));
    vis_queue = (NodeID *)malloc(row * sizeof(NodeID));
    deltas = (ScoreT *)malloc(row * sizeof(ScoreT));
    depths = (int *)malloc(row * sizeof(int));
    out_degree = (int *)malloc(row * sizeof(int));

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
        scores[i] = 0.;  // init scores
    }

    fclose(fp);
}

void free_graph_csr() {
    free(row_ptr);
    free(col_idx);
    free(scores);
    free(path_counts);
    free(vis_queue);
    free(deltas);
    free(depths);
    free(out_degree);
}

void generate_start_point() {
    // generate start point randomly
    int cnt = 0;
    do {
        start_point = rand() % row;
        cnt++;
    } while (out_degree[start_point] <= 5 && cnt <= 1e5);
}

void BrandesStep(const int *row_ptr, const NodeID *col_idx, const int nrow,
                 const NodeID start_v, NodeID vis_queue[nrow], int *depths,
                 CountT *path_counts, ScoreT *deltas, ScoreT *scores) {
    // depths
    for (NodeID nr = 0; nr < nrow; nr++) depths[nr] = -1;
    depths[start_v] = 0;

    // path_counts
    for (NodeID pcr = 0; pcr < nrow; pcr++) path_counts[pcr] = 0;
    path_counts[start_v] = 1;

    // BFS, records depth & queue_at_depth & path_counts
    int queue_at_depth[nrow + 1];  // worest case need O(nrow)
    int depth_level = 1;
    int vis_start, vis_end, vis_flag;
    vis_start = 0;
    vis_end = 1;
    vis_flag = 1;  // update depth level if reach
    vis_queue[0] = start_v;
    queue_at_depth[0] = 0;

    while (vis_start != vis_end) {
        NodeID node_cur = vis_queue[vis_start];
        for (int v = row_ptr[node_cur]; v < row_ptr[node_cur + 1]; v++) {
            NodeID node_v = col_idx[v];  // node_v(col_idx[v]) -> stream
            if (depths[node_v] == -1) {  // depths[node_v] -> indirect
                depths[node_v] = depth_level;
                vis_queue[vis_end++] = node_v;
            }
            if (depths[node_v] == depth_level) {
                path_counts[node_v] +=
                    path_counts[node_cur];  // path_counts[node_v] -> indirect
            }
        }
        vis_start++;
        if (vis_start == vis_flag) {
            vis_flag = vis_end;
            queue_at_depth[++depth_level] = vis_start;
        }
    }

    // Going from farthest to clostest, compute "depencies" (deltas)
    for (NodeID deltas_i = 0; deltas_i < nrow; deltas_i++) deltas[deltas_i] = 0;

    for (int cur_depth = depth_level - 1; cur_depth >= 0; cur_depth--) {
        for (int d = queue_at_depth[cur_depth];
             d < queue_at_depth[cur_depth + 1]; d++) {  // d -> stream
            NodeID node_d = vis_queue[d];  // node_d( vis_queue[d] ) -> stream
            for (int dv = row_ptr[node_d]; dv < row_ptr[node_d + 1];
                 dv++) {  // row_ptr[node_d] -> indirect
                NodeID node_dv = col_idx[dv];
                if (depths[node_dv] ==
                    depths[node_d] + 1) {  // depths[node_dv] -> indirect ，
                                           // depths[node_d] -> indirect
                    deltas[node_d] += (path_counts[node_d] /
                                       path_counts[node_dv]) *  // all indirect
                                      (1 + deltas[node_dv]);
                }
            }
            scores[node_d] += deltas[node_d];
        }
    }
    return;
}

int main(int argc, char *argv[]) {
    assert(argc == 3);
    parse_graph_mtx(argv[2]);

    generate_start_point();

    if (argv[1][0] == '1') {
        BrandesStep(row_ptr, col_idx, row, start_point, vis_queue, depths,
                    path_counts, deltas, scores);
        BrandesStep(row_ptr, col_idx, row, start_point, vis_queue, depths,
                    path_counts, deltas, scores);
        printf("Processor is warmed.\n");
    }

#ifdef GEM5
    m5_checkpoint(0, 0);
    m5_reset_stats(0, 0);
#endif

    // === benchmark start here
    BrandesStep(row_ptr, col_idx, row, start_point, vis_queue, depths,
                path_counts, deltas, scores);
    // === benchmark end here

#ifdef GEM5
    m5_dump_stats(0, 0);
#endif

    // iterations done and normalizing scores
    ScoreT max_score = scores[0];
    for (int s_i = 1; s_i < row; s_i++) {
        max_score = (scores[s_i] > max_score) ? scores[s_i] : max_score;
    }
    for (int s_i = 0; s_i < row; s_i++) {
        scores[s_i] /= max_score;
    }

    printf("BC (Brandes alg.) finished!\nThe first 8 scores:\n");
    for (int p = 0; p < 8; p++) {
        printf("%e\n", scores[p]);
    }

    free_graph_csr();

    return 0;
}
