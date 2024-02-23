#ifndef MAT_CSR_H
#define MAT_CSR_H

typedef unsigned char value_t;

class MatCSR {
    public:
        const int row_num;
        const int col_num;

        int* row_ptr;
        int* col_idx;
        value_t* values;

        MatCSR(int row_num, int col_num, int nz_num);
        ~MatCSR();
        MatCSR(MatCSR&&) = default;

        bool InitOrderedValues(value_t* mat);
        bool SeqSpMM(value_t* dense_mat, value_t* res_mat, int dense_col_num);
        int GetNZNum(){ return *(row_ptr+row_num); };
        
};

#endif // MAT_CSR_H
