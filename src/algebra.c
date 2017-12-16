#include <algebra.h>

#include <stdio.h>
#include <math.h>

void algebra_matrix_rotation_x(float *matrix, float theta)
{
	matrix[0] = 1.0f, matrix[1] = 0.0f, matrix[2] = 0.0f;
	matrix[3] = 0.0f, matrix[4] = cos(theta), matrix[5] = -sin(theta);
	matrix[6] = 0.0f, matrix[7] = sin(theta), matrix[8] = cos(theta);
}


void algebra_matrix_rotation_y(float *matrix, float theta)
{
	matrix[0] = cos(theta), matrix[1] = 0.0f, matrix[2] = -sin(theta);
	matrix[3] = 0.0f, matrix[4] = 1.0f, matrix[5] = 0.0f;
	matrix[6] = sin(theta), matrix[7] = 0.0f, matrix[8] = cos(theta);
}

void algebra_matrix_multiply(float *product, float *a, float *b)
{
	for (int i=0; i<9; i++)
		product[i] =
		a[i/3*3]*b[i%3]+a[i/3*3+1]*b[i%3+3]+a[i/3*3+2]*b[i%3+6];
}
