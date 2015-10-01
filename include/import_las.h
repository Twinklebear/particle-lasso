#ifndef IMPORT_LAS_H
#define IMPORT_LAS_H

#include <string>
#include <vector>
#include <liblas/liblas.hpp>
#include "types.h"

void import_las(const FileName &file_name, ParticleModel &model);

#endif

