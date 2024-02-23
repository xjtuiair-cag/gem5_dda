#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef GEM5
#include "gem5/m5ops.h"
#endif

typedef int NodeID;

// CSR related
int row, col, nnz;

int *row_ptr;
int *col_idx;

// CC related
int *comp;

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
    comp = (int *)malloc(row * sizeof(int));

    for (int r_i = 0; r_i < row + 1; r_i++) {
        row_ptr[r_i] = 0;
    }

    for (int i = 0; i < nnz; i++) {
        int row_id;
        double fake_value;
        fscanf(fp, "%d%d%lf", &row_id, &col_idx[i], &fake_value);
        col_idx[i]--;
        row_ptr[row_id]++;
    }

    for (int i = 0; i < row; i++) {
        row_ptr[i + 1] += row_ptr[i];
        comp[i] = -1;
    }

    fclose(fp);
}

void free_graph_csr() {
    free(row_ptr);
    free(col_idx);
    free(comp);
}

void ShiloachVishkin(const int *row_ptr, const NodeID *col_idx, const int nrow, int *comp)
{
    // initial
    for (NodeID i=0; i < nrow; i++) {
        comp[i] = i;
    }

    int change = 1;
    
    while(change == 1) {
        change = 0;

        // hooking
        for (NodeID node_r=0; node_r < nrow; node_r++) {
            for (int n = row_ptr[node_r]; n < row_ptr[node_r+1]; n++) {
                NodeID node_n = col_idx[n];
                int comp_r = comp[node_r];
                int comp_n = comp[node_n]; // comp[node_n] -> indirect
                if (comp_r == comp_n) continue;
                int high_comp = comp_r > comp_n ? comp_r : comp_n;
                int low_comp = comp_r + (comp_n - high_comp);
                // lower component ID matters
                if (high_comp == comp[high_comp]) { // Access [1]
                    change = 1;
                    comp[high_comp] = low_comp;
                }
            }
        }

        // shortcutting
        for (NodeID s=0; s < nrow; s++) {
            while (comp[s] != comp[comp[s]]) { // Access [2]
                comp[s] = comp[comp[s]];
            }
        }
    }

    return;
}

int main(int argc, char *argv[]) {
    assert(argc == 3);
    parse_graph_mtx(argv[2]);

    if (argv[1][0] == '1') {
        ShiloachVishkin(row_ptr, col_idx, row, comp);
        ShiloachVishkin(row_ptr, col_idx, row, comp);
        printf("Processor is warmed.\n");
    }

#ifdef GEM5
    m5_checkpoint(0, 0);
    m5_reset_stats(0, 0);
#endif

    // === benchmark start here
    ShiloachVishkin(row_ptr, col_idx, row, comp);
    // === benchmark end here

#ifdef GEM5
    m5_dump_stats(0, 0);
#endif

    printf("CC finished!\nThe first 8 result\n");
    for (int i = 0; i < row; i++) {
        printf("%d\n", comp[i]);
    }

    free_graph_csr();

    return 0; 
}
