//#include <iostream>

#define NZ_NUM 90000
#define M 300
#define N 300
#define K 1

//using namespace std;

#include "mat_csr.h"
#include "data.h"


int main()
{
    MatCSR S_csr(M, N, NZ_NUM);
    S_csr.InitOrderedValues(mat_S);

    // sequential solution
    value_t seq_mat_O[M*K] = {0};
    S_csr.SeqSpMM(mat_D, seq_mat_O, K);

    // output test results
    //for (int i=0; i<M; i++)
    //{
    //    for (int j=0; j<K; j++)
    //    {
    //        cout << (int)seq_mat_O[i*K+j] << " ";
    //    }
    //    cout << endl;
    //}
    return 0;
}
