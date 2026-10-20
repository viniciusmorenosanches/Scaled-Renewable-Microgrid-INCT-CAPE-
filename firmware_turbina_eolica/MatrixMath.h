#ifndef MatrixMath_h
#define MatrixMath_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

typedef float mtx_type;

class MatrixMath {
public:
    MatrixMath();
    void Print(mtx_type* A, int m, int n, String label);
    void Copy(mtx_type* A, int n, int m, mtx_type* B);
    void Multiply(mtx_type* A, mtx_type* B, int m, int p, int n, mtx_type* C);
    void Add(mtx_type* A, mtx_type* B, int m, int n, mtx_type* C);
    void Subtract(mtx_type* A, mtx_type* B, int m, int n, mtx_type* C);
    void Transpose(mtx_type* A, int m, int n, mtx_type* C);
    void Scale(mtx_type* A, int m, int n, mtx_type k);
    int Invert(mtx_type* A, int n);
};

extern MatrixMath Matrix;

#endif
