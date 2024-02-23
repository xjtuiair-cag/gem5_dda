#include "mat_csr.h"

MatCSR::MatCSR(int row_num, int col_num, int nz_num): row_num(row_num), col_num(col_num)
{
    row_ptr = new int [row_num+1];
    col_idx = new int [nz_num];
    values = new value_t [nz_num];

    *(row_ptr+row_num) = nz_num;
}

MatCSR::~MatCSR()
{
    delete[] row_ptr;
    delete[] col_idx;
    delete[] values;
}

bool MatCSR::InitOrderedValues(value_t* mat)
{
    int* col_idx_tr = col_idx;
    value_t* values_tr = values;

    for (int r=0; r<row_num; r++)
    {
        *(row_ptr+r) = col_idx_tr-col_idx;

        for (int c=0; c<col_num; c++)
        {
            if (*(mat+r*col_num+c)!=0.0)
            {
                *(col_idx_tr) = c;
                *(values_tr) = *(mat+r*col_num+c);
                col_idx_tr++;
                values_tr++;
            }
        }
    }

    return true;
}

bool MatCSR::SeqSpMM(value_t* dense_mat, value_t* res_mat, int dense_col_num)
{
    int i, j, k;
    for (i=0; i<row_num; i++)
    {
        for (j=*(row_ptr+i); j<*(row_ptr+i+1); j++)
        {
            for (k=0; k<dense_col_num; k++)
            {
                *(res_mat+i*dense_col_num+k) += *(values+j) * *(dense_mat+*(col_idx+j)*dense_col_num+k);
            }
        }
    }

    return true;
}
