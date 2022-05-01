#ifndef __NC_TIETUE__
#define __NC_TIETUE__
typedef struct {
  char* name;
  char iscoordinate;
  int ndims;
  char** dimnames; //pointers to names in nct_dim-structs
  size_t* dimlens; //
  int* dimids;     //these three arrays come from one malloc for dimnames + dimlens + dimids
  size_t len;
  int size1;
  nc_type xtype;
  void* data;
} nct_var;

typedef struct {
  char* name;
  size_t len;
  nct_var* coord;
} nct_dim;

typedef struct {
  int ndims;
  nct_dim* dims;
  char** dimnames; //pointers to names in nct_dim-structs
  size_t* dimlens;
  int nvars;
  nct_var* vars;
  char** varnames; //pointers to names in nct_var-structs
} nct_vset;

/*With this macro one can define functions for all nct_var types without repeating things.
  First define ONE_TYPE in a wanted way, then add ALL_TYPES then undef ONE_TYPE
  Functions can be further added into an array of function pointers with the same syntax
  which allows use of nc_type (int) nct_var as an index to access the right function.
  Reading the code will make this clearer.*/
#define ALL_TYPES_EXCEPT_STRING			\
  ONE_TYPE(NC_BYTE, hhi, char)			\
  ONE_TYPE(NC_UBYTE, hhu, unsigned char)     	\
  ONE_TYPE(NC_CHAR, c, char)			\
  ONE_TYPE(NC_SHORT, hi, short)		        \
  ONE_TYPE(NC_USHORT, hu, unsigned short)	\
  ONE_TYPE(NC_INT, i, int)			\
  ONE_TYPE(NC_UINT, u, unsigned)	        \
  ONE_TYPE(NC_UINT64, llu, long long unsigned)	\
  ONE_TYPE(NC_INT64, lli, long long int)	\
  ONE_TYPE(NC_FLOAT, .4f, float)		\
  ONE_TYPE(NC_DOUBLE, .4lf, double)
#define ALL_TYPES				\
  ALL_TYPES_EXCEPT_STRING			\
  ONE_TYPE(NC_STRING, s, char*)

/*The same thing is done here with operations than with types*/
#define ALL_OPERATIONS				\
  ONE_OPERATION(pluseq, +=)         		\
  ONE_OPERATION(minuseq, -=)       		\
  ONE_OPERATION(muleq, *=)        		\
  ONE_OPERATION(diveq, /=)      		\
  ONE_OPERATION(modeq, %=)        		\
  ONE_OPERATION(bitoreq, |=)     		\
  ONE_OPERATION(bitandeq, &=)      		\
  ONE_OPERATION(bitxoreq, ^=)      		\
  ONE_OPERATION(bitlshifteq, <<=)		\
  ONE_OPERATION(bitrshifteq, >>=)

/*All allowed combinations of types (except string) and operations are in "operations_and_types.h"*/

#define ONE_TYPE(nctype, ...) void print_##nctype(void* arr, int i, int end);
ALL_TYPES
#undef ONE_TYPE
void print_nct_var_data(nct_var*);
void print_nct_var(nct_var* var, const char* indent);
void print_nct_vset(nct_vset* vs);
nct_var* var_from_vset(nct_vset* vs, char* name);
#define OPERATION(nctype, a, b, opername, c) nct_var* varvar_##opername##_##nctype(nct_var*, nct_var*);
#include "operations_and_types.h"
#undef OPERATION
#define ONE_OPERATION(opername, a) nct_var* varvar_##opername(nct_var*, nct_var*);
ALL_OPERATIONS
#undef ONE_OPERATION
#define ONE_OPERATION(opername, a) nct_var* varvars_##opername(nct_var*, ...);
ALL_OPERATIONS
#undef ONE_OPERATION
void nct_init();
nct_dim* read_nct_dim(int ncid, int dimid, nct_dim* dest);
nct_var* read_nct_var(int ncid, int varid, nct_dim* dims, nct_var* dest);
nct_vset* read_ncfile(const char* restrict filename, nct_vset* dest);
void link_nct_vars_to_dimnames(nct_vset* vs);
void link_dims_to_coords(nct_vset* dest);
nct_dim* nct_dimcpy(nct_dim* dest, const nct_dim* src);
nct_var* nct_varcpy(nct_var* dest, const nct_var* src);
nct_vset* nct_vsetcpy(nct_vset* dest, const nct_vset* src);
void free_nct_dim(nct_dim*);
void free_nct_var(nct_var*);
void free_nct_vset(nct_vset*);
nct_var* to_nct_var(void* arr, nct_var* dest, size_t len, nc_type xtype, char* name);
nct_var* copy_to_nct_var(void* arr, nct_var* dest, size_t len, nc_type xtype, char* name);
nct_dim* to_nct_coord(void* arr, size_t len, nc_type xtype, char* name);
#endif
