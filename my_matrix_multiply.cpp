#include <string.h>
#include <iostream>
#include <fstream>
#include <pthread.h>
#include "c-timer.cpp"
using namespace std;

char *Usage = (char *)"my_matrix_multiply -a a_matrix_file.txt -b b_matrix_file.txt -t thread_count\n";
#define ARGS "a:b:t:"
struct Matrix
{
    double **matrix = nullptr;
    int Rows = 0;
    int Cols = 0;
    //mb

    Matrix(int _rows, int _cols)
        : Rows(_rows), Cols(_cols)
    {
        //allocate matrix
        matrix = new double *[Rows];
        for (int i = 0; i < Rows; i++)
            matrix[i] = new double[Cols];
    }
};
void print_usage()
{
    fprintf(stderr, "usage: %s", Usage);
    exit(2);
}
Matrix *ReadMatrix(string filename)
{
    fstream fin;
    fin.open(filename);
    int _rows, _cols;
    Matrix *mat;

    if (!fin.is_open())
    {
        cout << "can't open " << filename << endl;
        exit(2);
    }
    else
    {
        //read rows/cols
        fin >> _rows;
        fin >> _cols;
        if (_rows <= 0)
        {
            cout << "Invalid Input!!\nNo Of Rows must be a +ve integer\n";
            exit(2); //mb
        }
        if (_cols <= 0)
        {
            cout << "Invalid Input!!\nNo Of Cols must be a +ve integer\n";
            exit(2); //mb
        }
        //allocate matrix
        mat = new Matrix(_rows, _cols);
        int i, j;
        //read matrix
        for (i = 0; i < mat->Rows && !fin.eof(); i++)
        {
            fin.ignore(256, '\n');
            for (j = 0; j < mat->Cols && !fin.eof(); j++)
            {
                fin.ignore(256, '\n'); //to ignore till '\n'
                fin >> ws;             //to eat up spaces
                fin >> mat->matrix[i][j];
            }
        }
        if (i != mat->Rows || j != mat->Cols)
        {
            cout << "Invlaid Input!!\n";
            //exit(2);
        }

        fin.close();
    }

    return mat;
}
void getRandMatrixFile(string filename, int Rows, int Cols) //for now
{
    //to get random values
    double r;
    struct timeval tm;
    unsigned long seed;
    gettimeofday(&tm, NULL);
    seed = tm.tv_sec + tm.tv_usec;
    srand48(seed);

    //to open file
    ofstream fout;
    fout.open(filename.c_str());

    //to write on file

    fout << Rows << " " << Cols << endl;
    for (int i = 0; i < Rows; i++)
    {
        fout << "# Row " << i << endl;
        for (int j = 0; j < Cols; j++)
        {
            r = drand48();
            if (i == Rows - 1 && j == Cols - 1)
                fout << r;
            else
                fout << r << endl;
        }
    }
    fout.close();
    //close(fd);
}
void print_matrix(Matrix *mat)
{
    if (!mat)
    {
        cout << "!mat==true\n";
        exit(2); //mb
    }
    printf("%d %d\n", mat->Rows, mat->Cols);
    for (int i = 0; i < mat->Rows; i++)
    {
        printf("# Row %d\n", i);
        for (int j = 0; j < mat->Cols; j++)
        {
            printf("%f\n", mat->matrix[i][j]);
        }
    }
}
Matrix *multiplyWithoutThreads(Matrix *mat1,
                               Matrix *mat2)
{
    if (!mat1 || !mat2)
        exit(2); //mb
    if (mat1->Cols != mat2->Rows)
    {
        cout << "Matrix Multiplication for these matrices is not possible!\n"; //mb
        exit(2);                                                               //mb
    }
    Matrix *result = new Matrix(mat1->Rows, mat2->Cols);
    for (int i = 0; i < result->Rows; i++)
    {
        for (int j = 0; j < result->Cols; j++)
        {
            result->matrix[i][j] = 0;
            for (int k = 0; k < mat1->Cols; k++)
                result->matrix[i][j] += mat1->matrix[i][k] *
                                        mat2->matrix[k][j];
        }
    }
    return result;
}

struct ComputRowParam
{
    Matrix *mat1;
    Matrix *mat2;
    Matrix *result;
    //inclusive of these indices
    int start_row;
    int end_row;
    ComputRowParam(Matrix *m1, Matrix *m2, Matrix *res, int s_row, int e_row)
    {
        mat1 = m1;
        mat2 = m2;
        result = res;
        start_row = s_row;
        end_row = e_row;
    }
};
void *ComputeRow(void *param)
{
    ComputRowParam *input = (ComputRowParam *)param;
    // cout<<"\nIn thread\n";
    // cout << "start_row = " << input->start_row << endl
    //    << "end_row = " << input->end_row << endl;
    for (int i = input->start_row; i <= input->end_row; i++)
    {
        for (int j = 0; j < input->result->Cols; j++)
        {
            input->result->matrix[i][j] = 0;
            for (int k = 0; k < input->mat1->Cols; k++)
                input->result->matrix[i][j] += input->mat1->matrix[i][k] *
                                               input->mat2->matrix[k][j];
        }
    }
    pthread_exit(NULL); //mb
}
int getSlice(int total_rows, int thread_count, int &extra_slice)
{

    if (thread_count > total_rows)
    {
        cout << "Thread_Count is greater than Resultant Matrix Rows!!\n";
        cout << "Giving one thread to one Production Row!!\n";
        extra_slice = 0;
        return -1;
    }
    extra_slice = total_rows % thread_count;
    return total_rows / thread_count;
}
Matrix *multiplyUsingThreads(Matrix *mat1,
                             Matrix *mat2, int thread_count)
{
    if (!mat1 || !mat2)
        exit(2); //mb
    if (mat1->Cols != mat2->Rows)
    {
        cout << "Matrix Multiplication for these matrices is not possible!\n"; //mb
        exit(2);                                                               //mb
    }
    Matrix *result = new Matrix(mat1->Rows, mat2->Cols);

    int slice = result->Rows, extra_slice = 0;
    int start_row = 0;
    int end_row = -1;
    int Count = thread_count;

    slice = getSlice(result->Rows, thread_count, extra_slice);
    if (slice == -1) //thread_count > total_rows
    {
        Count = result->Rows;
        slice = 1;
    }
    pthread_t id[Count];
    // cout << "Thread_count = " << Count << endl;
    for (int i = 0; i < Count; i++)
    {

        // cout << "slice = " << slice << endl
        //      << "extra_slice = " << extra_slice << endl;
        start_row = end_row + 1;
        end_row = start_row + slice - 1; //mb
        if (extra_slice)
        {
            end_row++;
            extra_slice--;
        }
        // cout << "start_row = " << start_row << endl
        //      << "end_row = " << end_row << endl;
        if (pthread_create(&id[i], NULL, &ComputeRow,
                           new ComputRowParam(mat1, mat2, result, start_row, end_row)) == -1)
        {
            cout << "Thread Creation Failed!" << endl;
            return nullptr; //mb
        }
    }
    for (int i = 0; i < Count; i++)
        pthread_join(id[i], NULL); //mb

    return result;
}

int main(int argc, char **argv)
{
    //*****************to make matrices files*****************
    getRandMatrixFile("a.txt", 10, 9);
    getRandMatrixFile("b.txt", 9, 10);

    double start_time, total_time;
    start_time = CTimer();
    if (argc < 2) //mb
        print_usage();
    int option;
    string fileA, fileB;
    int thread_count;
    //parse arguments
    while ((option = getopt(argc, argv, ARGS)) != -1) //EOF
    {
        switch (option)
        {
        case 'a':
            fileA = optarg;
            break;
        case 'b':
            fileB = optarg;
            break;
        case 't':
            thread_count = atoi(optarg);
            break;
        default:
            fprintf(stderr, "unrecognized command %option\n",
                    (char)option);
            print_usage();
        }
    }
    if (thread_count <= 0)
    {
        cout << "ERROR!!\tThread_count must be greater than 0!\n";
        exit(0);
    }
    //read and print matrices
    Matrix *mat1 = ReadMatrix(fileA);
    Matrix *mat2 = ReadMatrix(fileB);
    cout << endl
         << "Matrix A" << endl;
    print_matrix(mat1);
    cout << endl
         << "Matrix B" << endl;
    print_matrix(mat2);
    cout << endl
         << "RESULT:\n";
    //multiplyUsingThreads(mat1, mat2, thread_count);
    //print_matrix(multiplyUsingThreads(mat1, mat2));
    print_matrix(multiplyUsingThreads(mat1, mat2, thread_count));
    total_time = CTimer() - start_time;
    cout << "Time Consumed: " << total_time << " seconds" << endl;

    exit(0);
}
//done