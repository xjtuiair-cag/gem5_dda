#define cache_size 65536

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

//  User guide
//  $ TC.elf true/false 矩阵路径/名称
//  true/false 表示是否需要预热cache，分支预测器
//  Example : ./TC.elf true matrices/simple_test_csr.mtx

const int max_row = 1000000;
const int max_col = 1000000;
const int max_nnz = 50000000;

int row, col, nnz, TC_tot = 0;

vector <int> row_vec[max_row];
vector <int> col_idx;

int row_ptr_stream[max_row];
int row_ptr_random[max_row];

double cache_flush[cache_size];

typedef struct
{
    int r;
    int c;
    double val;
}COO;

COO mat[max_nnz];

void handle_error(){
    cout<<"Error in loading data."<<endl;
    exit(0);
}

void read_data(string route){
    string file_route,file_line;
    bool data_clean=false; // 是否已经去除注释

    file_route=route;
    ifstream ifs(file_route);
    if (!ifs) cout<<"Error in reading data!"<<endl;

    while (!data_clean){
        streampos pos=ifs.tellg();
        getline(ifs,file_line);
        if (file_line[0]=='%') continue;
        else{
            ifs.seekg(pos,ios::beg);
            data_clean=true;
        }
    }

    if (ifs) ifs>>row>>col>>nnz;
    else handle_error();

    for (int i=0;i<nnz;i++){
        ifs>>mat[i].r>>mat[i].c>>mat[i].val;
        if (mat[i].r == mat[i].c) continue;
        row_vec[mat[i].r-1].push_back(mat[i].c-1);
        // row_vec[mat[i].c-1].push_back(mat[i].r-1);
    }

    int temp=0;
    for(int i=0;i<row;i++){
        sort(row_vec[i].begin(),row_vec[i].end());
        for (int j=0;j<row_vec[i].size();j++){
            col_idx.push_back(row_vec[i][j]);
        }
        row_ptr_stream[i+1]+=row_vec[i].size();
        row_ptr_stream[i+1]+=row_ptr_stream[i];
        row_ptr_random[i+1]=row_ptr_stream[i+1];
    }

}

void TC_kernel()
{
    for (int i=0;i<row;i++){
        for (int j=0;j<(row_ptr_stream[i+1]-row_ptr_stream[i]);j++){
            int neighbor=col_idx[row_ptr_stream[i]+j];
            for (int k=row_ptr_stream[i];k<row_ptr_stream[i+1];k++){
                for (int l=row_ptr_random[neighbor];l<row_ptr_random[neighbor+1];l++){
                    if (col_idx[k]==col_idx[l]) TC_tot++;
                }
            }
        }
    }
}

void print_TC(){
    cout<<"TC result is : "<<TC_tot/6<<endl;
}

int main(int argc,char *argv[])
{
    cout<<"TC begining!"<<endl;

    read_data(string(argv[2]));

    if ((nnz > max_nnz)||(row > max_row)||(col > max_col)){
        cout<<"Overflow in nnz/col/row!"<<endl;
        exit(0);
    }
    cout<<"Check overflow finished."<<endl;

    // flush stream data and random data in  256kB l2 cache
    for(int i=0;i<cache_size;i++){
        cache_flush[i] = 1;
    }

    if (strcmp(argv[1],"true")==0){
        cout<<"The processor is warming."<<endl;
        TC_kernel();
        TC_tot=0;
        cout<<"The processor has been warmed."<<endl;
    }

    cout<<"Start TC kernel."<<endl;

    // clock_t start,end;
    // start = clock();
    TC_kernel();
    // end = clock();
    // cout<<"execute time = "<<double(end-start)/CLOCKS_PER_SEC<<"s"<<endl;

    print_TC();

    return 0;
}
