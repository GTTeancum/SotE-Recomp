#pragma once
typedef struct {int locked,destroyed;} mtx_t;
void mtx_destroy(mtx_t*);
